// purrdometer-logger — BLE Cycling Speed & Cadence (CSC) sensor → SQLite.
//
// Talks to BlueZ over D-Bus (sdbus-c++). One event-loop thread receives
// signals; the main thread runs a small reconnect state machine. Every
// CSC Measurement notification is turned into a revolution delta and added
// to a per-minute bucket in SQLite (see schema below). The display process
// reads the same database.
//
// Link-layer robustness (long LE supervision timeout, WiFi power-save off) is
// configured system-wide by the Debian package, not here.
//
// Usage: purrdometer-logger [--db PATH] [--sensor-file PATH] [--selftest]

#include <sdbus-c++/sdbus-c++.h>
#include <sqlite3.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {

const std::string BLUEZ = "org.bluez";
const std::string ADAPTER_PATH = "/org/bluez/hci0";
const std::string ADAPTER_IFACE = "org.bluez.Adapter1";
const std::string DEVICE_IFACE = "org.bluez.Device1";
const std::string CHAR_IFACE = "org.bluez.GattCharacteristic1";
const std::string PROPS_IFACE = "org.freedesktop.DBus.Properties";
const std::string OM_IFACE = "org.freedesktop.DBus.ObjectManager";
const std::string CSC_SERVICE_UUID = "00001816-0000-1000-8000-00805f9b34fb";
const std::string CSC_MEASUREMENT_UUID = "00002a5b-0000-1000-8000-00805f9b34fb";
const std::string BATTERY_LEVEL_UUID = "00002a19-0000-1000-8000-00805f9b34fb";

// Anything faster than this is treated as a counter reset, not real motion.
// 5 rev/s on a 48.5" wheel is ~70 km/h.
constexpr double MAX_REVS_PER_SECOND = 5.0;

using Props = std::map<std::string, sdbus::Variant>;
using ManagedObjects = std::map<sdbus::ObjectPath, std::map<std::string, Props>>;

std::mutex logMutex;

template <typename... Args>
void log(Args&&... args)
{
    std::lock_guard<std::mutex> lock(logMutex);
    (std::cout << ... << args) << std::endl; // journald adds timestamps
}

double nowSeconds()
{
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
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
        return mac.substr(0, 17);
    return std::nullopt;
}

uint16_t le16(const std::vector<uint8_t>& d, size_t i) { return d[i] | (d[i + 1] << 8); }

// ---------------------------------------------------------------------------
// Revolution accounting. Pure logic, unit-tested by --selftest.
// ---------------------------------------------------------------------------

struct CrankSample {
    uint16_t revs;      // cumulative crank revolutions (wraps at 65536)
    uint16_t eventTime; // time of last crank event, 1/1024 s (wraps every 64 s)
};

struct Accounting {
    uint32_t delta = 0;  // revolutions to add
    double rpm = -1;     // < 0: unknown
    bool reset = false;  // counter discontinuity detected
};

// last: previous sample (if any) and the wall-clock time it was received.
Accounting account(std::optional<CrankSample> last, double lastWall, CrankSample cur, double nowWall)
{
    Accounting a;
    if (!last)
        return a;
    uint16_t dRevs = uint16_t(cur.revs - last->revs);
    double elapsed = std::max(0.0, nowWall - lastWall);
    double plausible = std::ceil(elapsed * MAX_REVS_PER_SECOND) + 2;
    if (dRevs > plausible) {
        a.reset = true; // sensor rebooted / battery swap / garbage
        return a;
    }
    a.delta = dRevs;
    uint16_t dTime = uint16_t(cur.eventTime - last->eventTime);
    if (dRevs > 0 && dTime > 0 && elapsed < 60)
        a.rpm = dRevs * 60.0 * 1024.0 / dTime;
    else if (dRevs == 0)
        a.rpm = 0;
    return a;
}

// ---------------------------------------------------------------------------
// SQLite storage
// ---------------------------------------------------------------------------
//
//   minutes(minute PK, revs)        permanent history; minute = unix_time / 60
//   samples(ts, revs, event_time, delta)  raw readings, pruned after 30 days
//   state(key PK, value)            live status for the display
//                                   (connected, rpm, last_motion, battery, sensor,
//                                    last_revs, last_event_time, last_wall)

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
        exec("CREATE TABLE IF NOT EXISTS state (key TEXT PRIMARY KEY, value TEXT)");
        exec("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)");
        exec("INSERT OR IGNORE INTO meta VALUES ('schema_version', '1')");
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
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Stmt s(db_, "INSERT INTO state VALUES (?1, ?2) ON CONFLICT(key) DO UPDATE SET value = excluded.value");
        s.text(1, key).text(2, value).step();
    }

    std::optional<std::string> state(const std::string& key)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Stmt s(db_, "SELECT value FROM state WHERE key = ?1");
        s.text(1, key);
        if (s.step() == SQLITE_ROW)
            return std::string(reinterpret_cast<const char*>(sqlite3_column_text(s.get(), 0)));
        return std::nullopt;
    }

    void record(double ts, CrankSample cur, uint32_t delta)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        exec("BEGIN");
        if (delta > 0) {
            Stmt s(db_, "INSERT INTO minutes VALUES (?1, ?2) "
                        "ON CONFLICT(minute) DO UPDATE SET revs = revs + excluded.revs");
            s.i64(1, int64_t(ts) / 60).i64(2, delta).step();
            Stmt r(db_, "INSERT INTO samples VALUES (?1, ?2, ?3, ?4)");
            r.dbl(1, ts).i64(2, cur.revs).i64(3, cur.eventTime).i64(4, delta).step();
        }
        setState("last_revs", std::to_string(cur.revs));
        setState("last_event_time", std::to_string(cur.eventTime));
        setState("last_wall", std::to_string(ts));
        exec("COMMIT");
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
        Stmt& i64(int i, int64_t v) { sqlite3_bind_int64(s_, i, v); return *this; }
        Stmt& dbl(int i, double v) { sqlite3_bind_double(s_, i, v); return *this; }
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

class Monitor {
public:
    Monitor(Store& store, std::string sensorFile) : store_(store), sensorFile_(std::move(sensorFile))
    {
        restoreCounter();
        bus_ = sdbus::createSystemBusConnection();
        root_ = sdbus::createProxy(*bus_, BLUEZ, "/");
        root_->uponSignal("InterfacesRemoved").onInterface(OM_IFACE).call(
            [this](const sdbus::ObjectPath& path, const std::vector<std::string>&) {
                if (path == devicePath_) {
                    log("device object removed by BlueZ");
                    setConnected(false);
                }
            });
        root_->finishRegistration();
        adapter_ = sdbus::createProxy(*bus_, BLUEZ, ADAPTER_PATH);
        bus_->enterEventLoopAsync();
    }

    void run()
    {
        store_.setState("connected", "0");
        if (auto mac = loadMac(sensorFile_))
            setDevice(*mac);

        int backoff = 1;
        while (true) {
            try {
                if (devicePath_.empty() || !deviceExists()) {
                    discover();
                } else if (!connected_) {
                    connect();
                } else if (servicesResolved_ && !notifying_) {
                    startNotify();
                }
                backoff = 1;
            } catch (const sdbus::Error& e) {
                log("D-Bus error ", e.getName(), ": ", e.getMessage());
                backoff = std::min(backoff * 2, 30);
                std::this_thread::sleep_for(std::chrono::seconds(backoff));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

private:
    void restoreCounter()
    {
        auto revs = store_.state("last_revs"), t = store_.state("last_event_time"), w = store_.state("last_wall");
        if (revs && t && w) {
            last_ = CrankSample{uint16_t(std::stoul(*revs)), uint16_t(std::stoul(*t))};
            lastWall_ = std::stod(*w);
            log("restored counter revs=", last_->revs);
        }
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
        device_ = sdbus::createProxy(*bus_, BLUEZ, devicePath_);
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
        device_->finishRegistration();
        log("target sensor ", mac_);
    }

    ManagedObjects objects()
    {
        ManagedObjects objs;
        root_->callMethod("GetManagedObjects").onInterface(OM_IFACE).storeResultsTo(objs);
        return objs;
    }

    bool deviceExists()
    {
        auto objs = objects();
        auto it = objs.find(sdbus::ObjectPath(devicePath_));
        if (it == objs.end() || !it->second.count(DEVICE_IFACE))
            return false;
        auto& props = it->second.at(DEVICE_IFACE);
        bool c = props.at("Connected").get<bool>();
        if (c != connected_)
            setConnected(c);
        servicesResolved_ = props.at("ServicesResolved").get<bool>();
        return true;
    }

    // Scan until a CSC sensor (the saved one, or any if none saved) advertises.
    void discover()
    {
        log("scanning for ", mac_.empty() ? "any CSC sensor" : mac_);
        adapter_->callMethod("SetDiscoveryFilter").onInterface(ADAPTER_IFACE).withArguments(
            Props{{"UUIDs", std::vector<std::string>{CSC_SERVICE_UUID}}, {"Transport", std::string("le")}});
        try {
            adapter_->callMethod("StartDiscovery").onInterface(ADAPTER_IFACE);
        } catch (const sdbus::Error& e) {
            if (e.getName() != "org.bluez.Error.InProgress")
                throw;
        }
        while (true) {
            for (auto& [path, ifaces] : objects()) {
                auto dev = ifaces.find(DEVICE_IFACE);
                if (dev == ifaces.end())
                    continue;
                auto& props = dev->second;
                std::string addr = props.at("Address").get<std::string>();
                bool match = mac_.empty() ? hasCsc(props) : addr == mac_;
                // RSSI is only present while the device is actively advertising.
                if (match && props.count("RSSI")) {
                    log("found ", addr, " rssi=", props.at("RSSI").get<int16_t>());
                    stopDiscovery();
                    if (mac_.empty()) {
                        std::ofstream(sensorFile_) << addr << '\n';
                        setDevice(addr);
                    }
                    // Trusted makes BlueZ keep the (unbonded) device object.
                    device_->setProperty("Trusted").onInterface(DEVICE_IFACE).toValue(true);
                    return;
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    static bool hasCsc(const Props& props)
    {
        auto it = props.find("UUIDs");
        if (it == props.end())
            return false;
        auto uuids = it->second.get<std::vector<std::string>>();
        return std::find(uuids.begin(), uuids.end(), CSC_SERVICE_UUID) != uuids.end();
    }

    void stopDiscovery()
    {
        try {
            adapter_->callMethod("StopDiscovery").onInterface(ADAPTER_IFACE);
        } catch (const sdbus::Error&) {
        }
    }

    void connect()
    {
        log("connecting");
        try {
            device_->callMethod("Connect").onInterface(DEVICE_IFACE).withTimeout(std::chrono::seconds(30));
        } catch (const sdbus::Error& e) {
            // Not advertising (asleep) → scan so we connect as soon as it wakes.
            log("connect failed (", e.getMessage(), "), waiting for advertisement");
            discover();
        }
    }

    std::string findChar(const ManagedObjects& objs, const std::string& uuid)
    {
        for (auto& [path, ifaces] : objs) {
            auto ch = ifaces.find(CHAR_IFACE);
            if (ch != ifaces.end() && std::string(path).rfind(devicePath_ + "/", 0) == 0
                && ch->second.at("UUID").get<std::string>() == uuid)
                return path;
        }
        return {};
    }

    void readBattery(const ManagedObjects& objs)
    {
        auto path = findChar(objs, BATTERY_LEVEL_UUID);
        if (path.empty())
            return;
        try {
            std::vector<uint8_t> v;
            sdbus::createProxy(*bus_, BLUEZ, path)->callMethod("ReadValue").onInterface(CHAR_IFACE)
                .withArguments(Props{}).storeResultsTo(v);
            if (!v.empty()) {
                store_.setState("battery", std::to_string(v[0]));
                log("battery ", int(v[0]), "%");
            }
        } catch (const sdbus::Error& e) {
            log("battery read failed: ", e.getMessage());
        }
    }

    void startNotify()
    {
        auto objs = objects();
        auto charPath = findChar(objs, CSC_MEASUREMENT_UUID);
        if (charPath.empty()) {
            log("CSC Measurement characteristic not found yet");
            return;
        }
        readBattery(objs);
        characteristic_ = sdbus::createProxy(*bus_, BLUEZ, charPath);
        characteristic_->uponSignal("PropertiesChanged").onInterface(PROPS_IFACE).call(
            [this](const std::string& iface, const Props& changed, const std::vector<std::string>&) {
                if (iface != CHAR_IFACE)
                    return;
                if (auto it = changed.find("Value"); it != changed.end())
                    onMeasurement(it->second.get<std::vector<uint8_t>>());
            });
        characteristic_->finishRegistration();
        // The notification session belongs to this D-Bus connection and lives as long as it does.
        characteristic_->callMethod("StartNotify").onInterface(CHAR_IFACE);
        notifying_ = true;
        log("CSC notifications on");
    }

    void onMeasurement(const std::vector<uint8_t>& d)
    {
        if (d.empty())
            return;
        uint8_t flags = d[0];
        size_t i = 1;
        if (flags & 0x01)
            i += 6; // wheel revolutions (not provided by this sensor)
        if (!(flags & 0x02) || d.size() < i + 4)
            return;
        CrankSample cur{le16(d, i), le16(d, i + 2)};
        double now = nowSeconds();
        Accounting a = account(last_, lastWall_, cur, now);
        if (a.reset)
            log("counter discontinuity (", last_->revs, " -> ", cur.revs, "), resyncing");
        try {
            store_.record(now, cur, a.delta);
            if (a.delta > 0)
                store_.setState("last_motion", std::to_string(now));
            if (a.rpm >= 0)
                store_.setState("rpm", std::to_string(int(a.rpm + 0.5)));
        } catch (const std::exception& e) {
            log("storage error: ", e.what());
        }
        if (a.delta > 0)
            log("revs=", cur.revs, " +", a.delta, " rpm=", int(a.rpm + 0.5));
        last_ = cur;
        lastWall_ = now;
    }

    Store& store_;
    std::string sensorFile_, mac_, devicePath_;
    std::unique_ptr<sdbus::IConnection> bus_;
    std::unique_ptr<sdbus::IProxy> root_, adapter_, device_, characteristic_;
    std::atomic<bool> connected_{false}, servicesResolved_{false}, notifying_{false};
    std::optional<CrankSample> last_;
    double lastWall_ = 0;
};

int selftest()
{
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        std::cout << (ok ? "ok   " : "FAIL ") << what << '\n';
        failures += !ok;
    };
    auto a = account(std::nullopt, 0, {10, 0}, 100);
    check(a.delta == 0 && !a.reset, "first sample adds nothing");
    a = account(CrankSample{10, 0}, 100, {11, 1024}, 101);
    check(a.delta == 1 && std::lround(a.rpm) == 60, "1 rev in 1 s = 60 rpm");
    a = account(CrankSample{65535, 64000}, 100, {1, 1000}, 101);
    check(a.delta == 2 && !a.reset, "16-bit revolution counter wraps");
    a = account(CrankSample{100, 0}, 100, {5, 0}, 101);
    check(a.reset && a.delta == 0, "counter going backwards = reset");
    a = account(CrankSample{100, 0}, 100, {1100, 0}, 101);
    check(a.reset, "implausible jump = reset");
    a = account(CrankSample{100, 0}, 100, {1100, 0}, 100 + 3600);
    check(a.delta == 1000 && !a.reset, "revs counted while logger was offline are kept");
    a = account(CrankSample{100, 500}, 100, {100, 500}, 101);
    check(a.delta == 0 && a.rpm == 0, "no motion = 0 rpm");

    Store mem(":memory:");
    mem.record(120.5, {5, 0}, 3);
    mem.record(150.0, {7, 0}, 2);
    mem.record(185.0, {7, 0}, 0);
    check(mem.state("last_wall").value().rfind("185", 0) == 0, "state persisted");
    std::cout << (failures ? "FAILED\n" : "all passed\n");
    return failures ? 1 : 0;
}

} // namespace

int main(int argc, char** argv)
{
    std::string db = "/var/lib/purrdometer/purrdometer.db";
    std::string sensorFile = "/var/lib/purrdometer/sensor";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--selftest")
            return selftest();
        if (arg == "--db" && i + 1 < argc)
            db = argv[++i];
        else if (arg == "--sensor-file" && i + 1 < argc)
            sensorFile = argv[++i];
        else {
            std::cerr << "usage: " << argv[0] << " [--db PATH] [--sensor-file PATH] [--selftest]\n";
            return 2;
        }
    }
    try {
        Store store(db);
        Monitor(store, sensorFile).run();
    } catch (const std::exception& e) {
        log("fatal: ", e.what());
        return 1;
    }
}
