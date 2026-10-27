// purrdometer-logger — BLE Cycling Speed & Cadence (CSC) sensor → SQLite.
//
// Talks to BlueZ over D-Bus (sdbus-c++). One event-loop thread receives
// signals; the main thread runs a small state machine: scan, connect, start
// notifications, and follow the settings. Every measurement is turned into a
// revolution delta and added to a per-minute bucket in SQLite
// (docs/intents/data/storage.md). Cadence sensors count crank revolutions,
// speed sensors wheel revolutions (docs/intents/ble/sensor.md); either way one
// revolution is one turn of the cat wheel.
//
// Link-layer robustness (long LE supervision timeout, a random address, WiFi
// power-save off) is configured system-wide by the Debian package, not here.
//
// Usage: purrdometer-logger [--db PATH] [--sensor-file PATH] [--config PATH]
//                           [--overrides PATH] [--scan-request PATH] [--selftest]

#include "common/config.h"

#include <sdbus-c++/sdbus-c++.h>
#include <sqlite3.h>
#include <sys/stat.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {

const sdbus::ServiceName BLUEZ{"org.bluez"};
const std::string ADAPTER_PATH = "/org/bluez/hci0";
const std::string ADAPTER_IFACE = "org.bluez.Adapter1";
const std::string DEVICE_IFACE = "org.bluez.Device1";
const std::string CHAR_IFACE = "org.bluez.GattCharacteristic1";
const std::string PROPS_IFACE = "org.freedesktop.DBus.Properties";
const std::string OM_IFACE = "org.freedesktop.DBus.ObjectManager";
const std::string CSC_SERVICE_UUID = "00001816-0000-1000-8000-00805f9b34fb";
const std::string CSC_MEASUREMENT_UUID = "00002a5b-0000-1000-8000-00805f9b34fb";
const std::string CSC_FEATURE_UUID = "00002a5c-0000-1000-8000-00805f9b34fb";
const std::string BATTERY_LEVEL_UUID = "00002a19-0000-1000-8000-00805f9b34fb";

// Anything faster than this is treated as a counter reset, not real motion.
// 5 rev/s on a 48" wheel is ~70 km/h.
constexpr double MAX_REVS_PER_SECOND = 5.0;
constexpr auto SCAN_ON_REQUEST = std::chrono::seconds(20);
constexpr auto TICK = std::chrono::milliseconds(500);

using Props = std::map<std::string, sdbus::Variant>;
using ManagedObjects = std::map<sdbus::ObjectPath, std::map<std::string, Props>>;
using Clock = std::chrono::steady_clock;

std::mutex logMutex;

template <typename... Args>
void log(Args&&... args)
{
    std::scoped_lock lock(logMutex);
    (std::cout << ... << args) << std::endl; // journald adds timestamps
}

double nowSeconds()
{
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

long long mtime(const std::string& path)
{
    struct stat st{};
    return stat(path.c_str(), &st) == 0 ? (long long)st.st_mtim.tv_sec * 1'000'000'000 + st.st_mtim.tv_nsec : -1;
}

std::string macToPath(const std::string& mac)
{
    std::string path = ADAPTER_PATH + "/dev_";
    for (char c : mac)
        path += (c == ':') ? '_' : c;
    return path;
}

std::optional<std::string> loadMac(const std::string& file)
{
    std::ifstream in(file);
    std::string mac;
    if (in && std::getline(in, mac) && mac.size() >= 17)
        return purr::normalizeAddress(mac.substr(0, 17));
    return std::nullopt;
}

uint16_t le16(const std::vector<uint8_t>& d, size_t i) { return uint16_t(d[i] | (d[i + 1] << 8)); }
uint32_t le32(const std::vector<uint8_t>& d, size_t i) { return le16(d, i) | (uint32_t(le16(d, i + 2)) << 16); }

// ---------------------------------------------------------------------------
// Revolution accounting. Pure logic, unit-tested by --selftest.
// ---------------------------------------------------------------------------

enum class Counter { Crank, Wheel }; // 16-bit crank revolutions, 32-bit wheel revolutions

const char* counterName(Counter c) { return c == Counter::Wheel ? "wheel" : "crank"; }

struct Sample {
    Counter counter = Counter::Crank;
    uint32_t revs = 0;      // cumulative revolutions, wrapping at the counter's width
    uint16_t eventTime = 0; // time of the last revolution, 1/1024 s (wraps every 64 s)
};

struct Accounting {
    uint32_t delta = 0;  // revolutions to add
    double rpm = -1;     // < 0: unknown
    bool reset = false;  // counter discontinuity detected
};

// last: previous sample (if any) and the wall-clock time it was received.
Accounting account(std::optional<Sample> last, double lastWall, Sample cur, double nowWall)
{
    Accounting a;
    if (!last || last->counter != cur.counter)
        return a; // a baseline: the first sample, or another kind of counter
    const uint32_t dRevs = cur.counter == Counter::Wheel ? cur.revs - last->revs : uint16_t(cur.revs - last->revs);
    const double elapsed = std::max(0.0, nowWall - lastWall);
    const double plausible = std::ceil(elapsed * MAX_REVS_PER_SECOND) + 2;
    if (dRevs > plausible) {
        a.reset = true; // sensor rebooted / battery swap / garbage
        return a;
    }
    a.delta = dRevs;
    const uint16_t dTime = uint16_t(cur.eventTime - last->eventTime);
    if (dRevs > 0 && dTime > 0 && elapsed < 60)
        a.rpm = dRevs * 60.0 * 1024.0 / dTime;
    else if (dRevs == 0)
        a.rpm = 0;
    return a;
}

// The revolutions a measurement carries for the given kind of sensor
// (docs/intents/ble/sensor.md#the-measurement).
std::optional<Sample> parseMeasurement(const std::vector<uint8_t>& d, Counter wanted)
{
    if (d.empty())
        return std::nullopt;
    const uint8_t flags = d[0];
    size_t i = 1;
    std::optional<Sample> wheel, crank;
    if (flags & 0x01) {
        if (d.size() < i + 6)
            return std::nullopt;
        wheel = Sample{Counter::Wheel, le32(d, i), le16(d, i + 4)};
        i += 6;
    }
    if ((flags & 0x02) && d.size() >= i + 4)
        crank = Sample{Counter::Crank, le16(d, i), le16(d, i + 2)};
    return wanted == Counter::Wheel ? wheel : crank;
}

// ---------------------------------------------------------------------------
// SQLite storage
// ---------------------------------------------------------------------------
//
//   minutes(minute PK, revs)        permanent history; minute = unix_time / 60
//   samples(ts, revs, event_time, delta)  raw readings, pruned after 30 days
//   sensors(address PK, name, kind, rssi, last_seen)  every CSC sensor heard
//   state(key PK, value)            live status for the display and the dashboard
//                                   (connected, rpm, last_motion, battery, sensor,
//                                    sensor_kind, last_counter, last_revs,
//                                    last_event_time, last_wall)

class Store {
public:
    explicit Store(const std::string& path)
    {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
            throw std::runtime_error("sqlite open " + path + ": " + sqlite3_errmsg(db_));
        sqlite3_busy_timeout(db_, 5000);
        exec("PRAGMA journal_mode=WAL");
        exec("PRAGMA synchronous=NORMAL");
        exec("CREATE TABLE IF NOT EXISTS minutes (minute INTEGER PRIMARY KEY, revs INTEGER NOT NULL)");
        exec("CREATE TABLE IF NOT EXISTS samples (ts REAL NOT NULL, revs INTEGER NOT NULL, "
             "event_time INTEGER NOT NULL, delta INTEGER NOT NULL)");
        exec("CREATE INDEX IF NOT EXISTS samples_ts ON samples(ts)");
        exec("CREATE TABLE IF NOT EXISTS sensors (address TEXT PRIMARY KEY, name TEXT, kind TEXT, "
             "rssi INTEGER, last_seen REAL)");
        exec("CREATE TABLE IF NOT EXISTS state (key TEXT PRIMARY KEY, value TEXT)");
        exec("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)");
        exec("INSERT OR IGNORE INTO meta VALUES ('schema_version', '2')");
        exec("UPDATE meta SET value = '2' WHERE key = 'schema_version'");
        exec("INSERT OR IGNORE INTO meta VALUES ('created', strftime('%s','now'))");
        exec("DELETE FROM samples WHERE ts < strftime('%s','now') - 30*86400");
    }
    ~Store() { sqlite3_close(db_); }

    void exec(const std::string& sql)
    {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "?";
            sqlite3_free(err);
            throw std::runtime_error("sqlite: " + msg + " in: " + sql);
        }
    }

    void setState(const std::string& key, const std::string& value)
    {
        std::scoped_lock lock(mutex_);
        Stmt s(db_, "INSERT INTO state VALUES (?1, ?2) ON CONFLICT(key) DO UPDATE SET value = excluded.value");
        s.text(1, key).text(2, value).step();
    }

    void clearState(const std::string& key)
    {
        std::scoped_lock lock(mutex_);
        Stmt s(db_, "DELETE FROM state WHERE key = ?1");
        s.text(1, key).step();
    }

    std::optional<std::string> state(const std::string& key)
    {
        std::scoped_lock lock(mutex_);
        Stmt s(db_, "SELECT value FROM state WHERE key = ?1");
        s.text(1, key);
        if (s.step() == SQLITE_ROW)
            return std::string(reinterpret_cast<const char*>(sqlite3_column_text(s.get(), 0)));
        return std::nullopt;
    }

    // A sensor heard advertising (rssi) or whose features were read (kind).
    void sensorHeard(const std::string& address, const std::string& name, std::optional<int> rssi,
                     std::optional<std::string> kind = std::nullopt)
    {
        std::scoped_lock lock(mutex_);
        Stmt s(db_, "INSERT INTO sensors VALUES (?1, ?2, ?3, ?4, ?5) ON CONFLICT(address) DO UPDATE SET "
                    "name = excluded.name, kind = coalesce(excluded.kind, kind), "
                    "rssi = coalesce(excluded.rssi, rssi), last_seen = excluded.last_seen");
        s.text(1, address).text(2, name).optText(3, kind).optInt(4, rssi).dbl(5, nowSeconds()).step();
    }

    void record(double ts, Sample cur, uint32_t delta)
    {
        std::scoped_lock lock(mutex_);
        exec("BEGIN");
        if (delta > 0) {
            Stmt s(db_, "INSERT INTO minutes VALUES (?1, ?2) "
                        "ON CONFLICT(minute) DO UPDATE SET revs = revs + excluded.revs");
            s.i64(1, int64_t(ts) / 60).i64(2, delta).step();
            Stmt r(db_, "INSERT INTO samples VALUES (?1, ?2, ?3, ?4)");
            r.dbl(1, ts).i64(2, cur.revs).i64(3, cur.eventTime).i64(4, delta).step();
        }
        setState("last_counter", counterName(cur.counter));
        setState("last_revs", std::to_string(cur.revs));
        setState("last_event_time", std::to_string(cur.eventTime));
        setState("last_wall", std::to_string(ts));
        exec("COMMIT");
    }

    // A new sensor: nothing is counted across the switch.
    void forgetCounter()
    {
        for (const char* key : {"last_counter", "last_revs", "last_event_time", "last_wall"})
            clearState(key);
    }

private:
    class Stmt {
    public:
        Stmt(sqlite3* db, const char* sql) : db_(db)
        {
            if (sqlite3_prepare_v2(db, sql, -1, &s_, nullptr) != SQLITE_OK)
                throw std::runtime_error(std::string("sqlite prepare: ") + sqlite3_errmsg(db));
        }
        ~Stmt() { sqlite3_finalize(s_); }
        Stmt& text(int i, const std::string& v) { sqlite3_bind_text(s_, i, v.c_str(), -1, SQLITE_TRANSIENT); return *this; }
        Stmt& optText(int i, const std::optional<std::string>& v) { v ? text(i, *v) : null(i); return *this; }
        Stmt& i64(int i, int64_t v) { sqlite3_bind_int64(s_, i, v); return *this; }
        Stmt& optInt(int i, std::optional<int> v) { v ? i64(i, *v) : null(i); return *this; }
        Stmt& dbl(int i, double v) { sqlite3_bind_double(s_, i, v); return *this; }
        Stmt& null(int i) { sqlite3_bind_null(s_, i); return *this; }
        int step()
        {
            int rc = sqlite3_step(s_);
            if (rc != SQLITE_ROW && rc != SQLITE_DONE)
                throw std::runtime_error(std::string("sqlite step: ") + sqlite3_errmsg(db_));
            return rc;
        }
        sqlite3_stmt* get() { return s_; }

    private:
        sqlite3* db_;
        sqlite3_stmt* s_ = nullptr;
    };

    sqlite3* db_ = nullptr;
    std::recursive_mutex mutex_; // called from the D-Bus thread and the main thread
};

// ---------------------------------------------------------------------------
// BLE monitor
// ---------------------------------------------------------------------------

struct Paths {
    std::string db = "/var/lib/purrdometer/purrdometer.db";
    std::string sensorFile = "/var/lib/purrdometer/sensor";
    std::string config = purr::CONFIG_PATH;
    std::string overrides = purr::OVERRIDES_PATH;
    std::string scanRequest = "/var/lib/purrdometer/scan-request";
};

class Monitor {
public:
    Monitor(Store& store, Paths paths) : store_(store), paths_(std::move(paths))
    {
        restoreCounter();
        bus_ = sdbus::createSystemBusConnection();
        root_ = sdbus::createProxy(*bus_, BLUEZ, sdbus::ObjectPath{"/"});
        root_->uponSignal("InterfacesRemoved").onInterface(OM_IFACE).call(
            [this](const sdbus::ObjectPath& path, const std::vector<std::string>&) {
                if (path == devicePath_) {
                    log("device object removed by BlueZ");
                    setConnected(false);
                }
            });
        adapter_ = sdbus::createProxy(*bus_, BLUEZ, sdbus::ObjectPath{ADAPTER_PATH});
        bus_->enterEventLoopAsync();
    }

    void run()
    {
        store_.setState("connected", "0");
        configStamp_ = configStamp();
        scanStamp_ = mtime(paths_.scanRequest);
        choice_ = purr::sensorChoice(paths_.config, paths_.overrides);
        if (auto mac = choice_ ? choice_ : loadMac(paths_.sensorFile))
            setDevice(*mac);

        int backoff = 1;
        while (true) {
            try {
                followSettings();
                serveScanRequest();
                if (devicePath_.empty() || !deviceExists()) {
                    scanStep();
                } else if (!connected_) {
                    found_ ? connect() : scanStep();
                } else if (servicesResolved_ && !notifying_) {
                    startNotify();
                }
                backoff = 1;
            } catch (const sdbus::Error& e) {
                log("D-Bus error ", e.getName(), ": ", e.getMessage());
                backoff = std::min(backoff * 2, 30);
                std::this_thread::sleep_for(std::chrono::seconds(backoff));
            }
            std::this_thread::sleep_for(TICK);
        }
    }

private:
    void restoreCounter()
    {
        auto counter = store_.state("last_counter"), revs = store_.state("last_revs"),
             t = store_.state("last_event_time"), w = store_.state("last_wall");
        if (revs && t && w) {
            Counter c = counter == std::optional<std::string>("wheel") ? Counter::Wheel : Counter::Crank;
            last_ = Sample{c, uint32_t(std::stoul(*revs)), uint16_t(std::stoul(*t))};
            lastWall_ = std::stod(*w);
            log("restored ", counterName(c), " counter at ", last_->revs);
        }
    }

    std::pair<long long, long long> configStamp() const { return {mtime(paths_.config), mtime(paths_.overrides)}; }

    // [sensor] address from the settings page or the config file, without a restart.
    void followSettings()
    {
        auto stamp = configStamp();
        if (stamp == configStamp_)
            return;
        configStamp_ = stamp;
        auto choice = purr::sensorChoice(paths_.config, paths_.overrides);
        if (choice == choice_)
            return;
        choice_ = choice;
        auto target = choice_ ? choice_ : loadMac(paths_.sensorFile);
        if (target && *target == mac_)
            return;
        log("sensor choice: ", choice_.value_or("the first one heard"));
        if (device_ && connected_) {
            try {
                device_->callMethod("Disconnect").onInterface(DEVICE_IFACE);
            } catch (const sdbus::Error&) {
            }
        }
        last_.reset();
        store_.forgetCounter();
        mac_.clear();
        devicePath_.clear();
        found_ = false;
        device_.reset();
        characteristic_.reset();
        setConnected(false);
        if (target)
            setDevice(*target);
    }

    // A touch of the scan-request file asks for a scan, connected or not, for the settings page.
    void serveScanRequest()
    {
        auto stamp = mtime(paths_.scanRequest);
        if (stamp == scanStamp_ || stamp < 0)
            return;
        scanStamp_ = stamp;
        log("scanning for ", SCAN_ON_REQUEST.count(), " s, as the settings page asked");
        startDiscovery();
        auto end = Clock::now() + SCAN_ON_REQUEST;
        while (Clock::now() < end) {
            recordHeard(objects());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        stopDiscovery();
    }

    void setConnected(bool c)
    {
        connected_ = c;
        if (!c) {
            servicesResolved_ = false;
            notifying_ = false;
        }
        store_.setState("connected", c ? "1" : "0");
        if (!c)
            store_.setState("rpm", "0");
    }

    void setDevice(const std::string& mac)
    {
        mac_ = mac;
        devicePath_ = macToPath(mac);
        store_.setState("sensor", mac);
        device_ = sdbus::createProxy(*bus_, BLUEZ, sdbus::ObjectPath{devicePath_});
        device_->uponSignal("PropertiesChanged").onInterface(PROPS_IFACE).call(
            [this](const std::string& iface, const Props& changed, const std::vector<std::string>&) {
                if (iface != DEVICE_IFACE)
                    return;
                if (auto it = changed.find("Connected"); it != changed.end()) {
                    bool c = it->second.get<bool>();
                    log(c ? "connected" : "disconnected");
                    setConnected(c);
                }
                if (auto it = changed.find("ServicesResolved"); it != changed.end()) {
                    servicesResolved_ = it->second.get<bool>();
                    log("services resolved: ", servicesResolved_ ? "yes" : "no");
                }
            });
        log("target sensor ", mac_);
    }

    ManagedObjects objects()
    {
        ManagedObjects objs;
        root_->callMethod("GetManagedObjects").onInterface(OM_IFACE).storeResultsTo(objs);
        return objs;
    }

    std::optional<Props> deviceProps()
    {
        auto objs = objects();
        auto it = objs.find(sdbus::ObjectPath(devicePath_));
        if (it == objs.end() || !it->second.count(DEVICE_IFACE))
            return std::nullopt;
        return it->second.at(DEVICE_IFACE);
    }

    bool deviceExists()
    {
        auto props = deviceProps();
        if (!props)
            return false;
        bool c = props->at("Connected").get<bool>();
        if (c != connected_)
            setConnected(c);
        servicesResolved_ = props->at("ServicesResolved").get<bool>();
        return true;
    }

    static bool hasCsc(const Props& props)
    {
        auto it = props.find("UUIDs");
        return it != props.end() && std::ranges::contains(it->second.get<std::vector<std::string>>(), CSC_SERVICE_UUID);
    }

    static std::string name(const Props& props)
    {
        for (const char* key : {"Name", "Alias"})
            if (auto it = props.find(key); it != props.end())
                return it->second.get<std::string>();
        return "";
    }

    // Every CSC sensor heard goes into the sensors table, for the settings page.
    void recordHeard(const ManagedObjects& objs)
    {
        for (auto& [path, ifaces] : objs) {
            auto dev = ifaces.find(DEVICE_IFACE);
            if (dev == ifaces.end() || !hasCsc(dev->second) || !dev->second.count("RSSI"))
                continue;
            auto& props = dev->second;
            store_.sensorHeard(props.at("Address").get<std::string>(), name(props), props.at("RSSI").get<int16_t>());
        }
    }

    void startDiscovery()
    {
        adapter_->callMethod("SetDiscoveryFilter").onInterface(ADAPTER_IFACE).withArguments(
            Props{{"UUIDs", sdbus::Variant{std::vector<std::string>{CSC_SERVICE_UUID}}},
                  {"Transport", sdbus::Variant{std::string("le")}}});
        try {
            adapter_->callMethod("StartDiscovery").onInterface(ADAPTER_IFACE);
            if (!discovering_)
                log("scanning for ", mac_.empty() ? "any CSC sensor" : mac_);
            discovering_ = true;
        } catch (const sdbus::Error& e) {
            if (e.getName() != "org.bluez.Error.InProgress")
                throw;
            discovering_ = true;
        }
    }

    void stopDiscovery()
    {
        try {
            adapter_->callMethod("StopDiscovery").onInterface(ADAPTER_IFACE);
        } catch (const sdbus::Error&) {
        }
        discovering_ = false;
    }

    // One look at what the scan has heard; returns to the loop either way, so a
    // change of settings or a scan request is seen while the sensor sleeps.
    void scanStep()
    {
        if (!discovering_)
            startDiscovery();
        auto objs = objects();
        recordHeard(objs);
        for (auto& [path, ifaces] : objs) {
            auto dev = ifaces.find(DEVICE_IFACE);
            if (dev == ifaces.end())
                continue;
            auto& props = dev->second;
            std::string addr = props.at("Address").get<std::string>();
            bool match = mac_.empty() ? hasCsc(props) : addr == mac_;
            if (!match || !props.count("RSSI"))
                continue;
            log("found ", addr, " rssi=", props.at("RSSI").get<int16_t>());
            stopDiscovery();
            found_ = true;
            if (mac_.empty()) {
                std::ofstream(paths_.sensorFile) << addr << '\n';
                setDevice(addr);
            }
            // Trusted makes BlueZ keep the (unbonded) device object.
            device_->setProperty("Trusted").onInterface(DEVICE_IFACE).toValue(true);
            return;
        }
    }

    void connect()
    {
        found_ = false;
        log("connecting");
        try {
            device_->callMethod("Connect").onInterface(DEVICE_IFACE).withTimeout(std::chrono::seconds(30));
        } catch (const sdbus::Error& e) {
            // Not advertising (asleep): the scan picks it up when it wakes.
            log("connect failed (", e.getMessage(), "), waiting for advertisement");
        }
    }

    std::string findChar(const ManagedObjects& objs, const std::string& uuid)
    {
        for (auto& [path, ifaces] : objs) {
            auto ch = ifaces.find(CHAR_IFACE);
            if (ch != ifaces.end() && path.starts_with(devicePath_ + "/") &&
                ch->second.at("UUID").get<std::string>() == uuid)
                return path;
        }
        return {};
    }

    std::optional<std::vector<uint8_t>> readChar(const ManagedObjects& objs, const std::string& uuid)
    {
        auto path = findChar(objs, uuid);
        if (path.empty())
            return std::nullopt;
        try {
            std::vector<uint8_t> v;
            sdbus::createProxy(*bus_, BLUEZ, sdbus::ObjectPath{path})
                ->callMethod("ReadValue").onInterface(CHAR_IFACE).withArguments(Props{}).storeResultsTo(v);
            return v;
        } catch (const sdbus::Error& e) {
            log("read of ", uuid, " failed: ", e.getMessage());
            return std::nullopt;
        }
    }

    // The battery, and whether this is a cadence or a speed sensor.
    void readSensor(const ManagedObjects& objs)
    {
        if (auto v = readChar(objs, BATTERY_LEVEL_UUID); v && !v->empty()) {
            store_.setState("battery", std::to_string((*v)[0]));
            log("battery ", int((*v)[0]), "%");
        }
        std::optional<std::string> kind;
        if (auto v = readChar(objs, CSC_FEATURE_UUID); v && v->size() >= 2) {
            uint16_t features = le16(*v, 0);
            if (features & 0x01)
                kind = "speed"; // wheel revolutions; a sensor with both is used as a speed sensor
            else if (features & 0x02)
                kind = "cadence";
        }
        counter_ = kind == std::optional<std::string>("speed") ? Counter::Wheel : Counter::Crank;
        store_.setState("sensor_kind", kind.value_or("cadence"));
        if (auto props = deviceProps())
            store_.sensorHeard(mac_, name(*props), std::nullopt, kind);
        log(kind.value_or("cadence"), " sensor; counting ", counterName(counter_), " revolutions");
    }

    void startNotify()
    {
        auto objs = objects();
        auto charPath = findChar(objs, CSC_MEASUREMENT_UUID);
        if (charPath.empty()) {
            log("CSC Measurement characteristic not found yet");
            return;
        }
        readSensor(objs);
        characteristic_ = sdbus::createProxy(*bus_, BLUEZ, sdbus::ObjectPath{charPath});
        characteristic_->uponSignal("PropertiesChanged").onInterface(PROPS_IFACE).call(
            [this](const std::string& iface, const Props& changed, const std::vector<std::string>&) {
                if (iface != CHAR_IFACE)
                    return;
                if (auto it = changed.find("Value"); it != changed.end())
                    onMeasurement(it->second.get<std::vector<uint8_t>>());
            });
        // The notification session belongs to this D-Bus connection and lives as long as it does.
        characteristic_->callMethod("StartNotify").onInterface(CHAR_IFACE);
        notifying_ = true;
        log("CSC notifications on");
    }

    void onMeasurement(const std::vector<uint8_t>& d)
    {
        auto cur = parseMeasurement(d, counter_);
        if (!cur)
            return;
        double now = nowSeconds();
        Accounting a = account(last_, lastWall_, *cur, now);
        if (a.reset)
            log("counter discontinuity (", last_->revs, " -> ", cur->revs, "), resyncing");
        try {
            store_.record(now, *cur, a.delta);
            if (a.delta > 0)
                store_.setState("last_motion", std::to_string(now));
            if (a.rpm >= 0)
                store_.setState("rpm", std::to_string(int(a.rpm + 0.5)));
        } catch (const std::exception& e) {
            log("storage error: ", e.what());
        }
        if (a.delta > 0)
            log("revs=", cur->revs, " +", a.delta, " rpm=", int(a.rpm + 0.5));
        last_ = *cur;
        lastWall_ = now;
    }

    Store& store_;
    Paths paths_;
    std::string mac_, devicePath_;
    std::optional<std::string> choice_;
    std::pair<long long, long long> configStamp_;
    long long scanStamp_ = -1;
    std::unique_ptr<sdbus::IConnection> bus_;
    std::unique_ptr<sdbus::IProxy> root_, adapter_, device_, characteristic_;
    std::atomic<bool> connected_{false}, servicesResolved_{false}, notifying_{false};
    bool discovering_ = false;
    bool found_ = false; // heard advertising: worth a Connect
    Counter counter_ = Counter::Crank;
    std::optional<Sample> last_;
    double lastWall_ = 0;
};

int selftest()
{
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        std::cout << (ok ? "ok   " : "FAIL ") << what << '\n';
        failures += !ok;
    };
    const Sample crank{Counter::Crank, 10, 0};
    auto a = account(std::nullopt, 0, crank, 100);
    check(a.delta == 0 && !a.reset, "first sample adds nothing");
    a = account(crank, 100, {Counter::Crank, 11, 1024}, 101);
    check(a.delta == 1 && std::lround(a.rpm) == 60, "1 rev in 1 s = 60 rpm");
    a = account(Sample{Counter::Crank, 65535, 64000}, 100, {Counter::Crank, 1, 1000}, 101);
    check(a.delta == 2 && !a.reset, "16-bit crank counter wraps");
    a = account(Sample{Counter::Crank, 100, 0}, 100, {Counter::Crank, 5, 0}, 101);
    check(a.reset && a.delta == 0, "counter going backwards = reset");
    a = account(Sample{Counter::Crank, 100, 0}, 100, {Counter::Crank, 1100, 0}, 101);
    check(a.reset, "implausible jump = reset");
    a = account(Sample{Counter::Crank, 100, 0}, 100, {Counter::Crank, 1100, 0}, 100 + 3600);
    check(a.delta == 1000 && !a.reset, "revs counted while logger was offline are kept");
    a = account(Sample{Counter::Crank, 100, 500}, 100, {Counter::Crank, 100, 500}, 101);
    check(a.delta == 0 && a.rpm == 0, "no motion = 0 rpm");
    a = account(Sample{Counter::Wheel, 0xFFFFFFFF, 0}, 100, {Counter::Wheel, 2, 2048}, 101);
    check(a.delta == 3 && std::lround(a.rpm) == 90, "32-bit wheel counter wraps");
    a = account(Sample{Counter::Wheel, 70000, 0}, 100, {Counter::Wheel, 70004, 1024}, 101);
    check(a.delta == 4, "wheel counter past 16 bits");
    a = account(Sample{Counter::Crank, 100, 0}, 100, {Counter::Wheel, 5000, 0}, 101);
    check(a.delta == 0 && !a.reset, "another kind of counter is a new baseline");

    // flags, wheel revs (le32), wheel time (le16), crank revs (le16), crank time (le16)
    std::vector<uint8_t> both = {0x03, 0x10, 0x27, 0x01, 0x00, 0x00, 0x04, 0x2A, 0x00, 0x00, 0x08};
    auto w = parseMeasurement(both, Counter::Wheel), c = parseMeasurement(both, Counter::Crank);
    check(w && w->revs == 0x12710 && w->eventTime == 0x0400, "a speed sensor's wheel revolutions");
    check(c && c->revs == 42 && c->eventTime == 0x0800, "a cadence sensor's crank revolutions");
    check(!parseMeasurement({0x02, 0x2A, 0x00, 0x00, 0x08}, Counter::Wheel), "no wheel data for a speed sensor");
    check(!parseMeasurement({0x01, 0x01}, Counter::Wheel), "truncated measurement");

    Store mem(":memory:");
    mem.record(120.5, {Counter::Crank, 5, 0}, 3);
    mem.record(150.0, {Counter::Wheel, 7, 0}, 2);
    check(mem.state("last_wall").value().starts_with("150") && mem.state("last_counter") == "wheel", "state persisted");
    mem.forgetCounter();
    check(!mem.state("last_revs") && !mem.state("last_counter"), "switching sensor forgets the counter");
    mem.sensorHeard("AA:BB:CC:DD:EE:FF", "SPD-BLE1", -60);
    mem.sensorHeard("AA:BB:CC:DD:EE:FF", "SPD-BLE1", std::nullopt, "speed");
    check(true, "sensors recorded");
    std::cout << (failures ? "FAILED\n" : "all passed\n");
    return failures ? 1 : 0;
}

} // namespace

int main(int argc, char** argv)
{
    Paths paths;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](std::string& field) {
            if (i + 1 >= argc)
                throw std::invalid_argument(arg + " needs a value");
            field = argv[++i];
        };
        try {
            if (arg == "--selftest")
                return selftest();
            else if (arg == "--db") next(paths.db);
            else if (arg == "--sensor-file") next(paths.sensorFile);
            else if (arg == "--config") next(paths.config);
            else if (arg == "--overrides") next(paths.overrides);
            else if (arg == "--scan-request") next(paths.scanRequest);
            else throw std::invalid_argument("unknown argument " + arg);
        } catch (const std::invalid_argument& e) {
            std::cerr << e.what() << "\nusage: " << argv[0]
                      << " [--db PATH] [--sensor-file PATH] [--config PATH] [--overrides PATH] [--scan-request PATH]"
                         " [--selftest]\n";
            return 2;
        }
    }
    try {
        Store store(paths.db);
        Monitor(store, paths).run();
    } catch (const std::exception& e) {
        log("fatal: ", e.what());
        return 1;
    }
}
