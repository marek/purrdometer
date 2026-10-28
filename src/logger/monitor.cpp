#include "logger/monitor.h"

#include "logger/gatt.h"
#include "logger/log.h"

#include <thread>

namespace purr::logger {

using namespace bluez;
using Clock = std::chrono::steady_clock;

Monitor::Monitor(Store& store, const Paths& paths)
    : store_(store), bus_(sdbus::createSystemBusConnection()),
      adapter_(*bus_, [this](const std::string& path) { link_.lost(path); }),
      rawLog_(paths.sensorLogDir, loadEditable(paths.config, paths.overrides).rawLog,
              loadEditable(paths.config, paths.overrides).rawLogDays),
      counter_(store, rawLog_), paths_(paths), settings_({paths.config, paths.overrides, paths.profiles}),
      choice_(paths.sensorFile, sensorChoice(paths.config, paths.overrides)), scanRequest_({paths.scanRequest})
{
    applySettings(loadEditable(paths.config, paths.overrides));
    bus_->enterEventLoopAsync();
}

void Monitor::applySettings(const Editable& e)
{
    rawLog_.configure(e.rawLog, e.rawLogDays);
    counter_.setWakeTurns(e.wakeTurns);
    int wheel = currentProfile(paths_.profiles);
    counter_.setWheel(wheel);
    rawLog_.event("settings changed", "wake_turns=" + std::to_string(e.wakeTurns) + " wheel=" +
                                           std::to_string(wheel) + " sensor=" + e.sensor.value_or("automatic"));
}

Monitor::~Monitor() { bus_->leaveEventLoop(); }

void Monitor::run()
{
    store_.setState("connected", "0");
    follow(choice_.target());
    int backoff = 1;
    while (true) {
        try {
            step();
            backoff = 1;
        } catch (const sdbus::Error& e) {
            log("D-Bus error ", e.getName(), ": ", e.getMessage());
            backoff = std::min(backoff * 2, 30);
            std::this_thread::sleep_for(std::chrono::seconds(backoff));
        }
        std::this_thread::sleep_for(TICK);
    }
}

void Monitor::step()
{
    followSettings();
    if (scanRequest_.touched())
        serveScanRequest();
    if (!link_ || !deviceExists())
        scanStep();
    else if (!link_->connected() && found_) {
        found_ = false;
        rawLog_.event("connecting", link_->mac());
        link_->connect();
    } else if (!link_->connected())
        scanStep();
    else if (link_->servicesResolved() && !link_->notifying())
        startNotify();
}

void Monitor::follow(const std::optional<std::string>& mac)
{
    link_.replace(nullptr);
    found_ = false;
    if (!mac)
        return;
    link_.replace(std::make_unique<SensorLink>(*bus_, *mac, [this](bool c) { publishConnected(c); }));
    store_.setState("sensor", *mac);
}

bool Monitor::forgetCount()
{
    try {
        counter_.forget();
        return true;
    } catch (const std::exception& e) {
        log("storage error forgetting the old sensor's count: ", e.what());
        return false;
    }
}

void Monitor::followSettings()
{
    if (settings_.changed()) {
        Editable e = loadEditable(paths_.config, paths_.overrides);
        applySettings(e);
        if (choice_.choose(e.sensor))
            switchPending_ = true;
    }
    if (!switchPending_)
        return;
    auto target = choice_.target();
    if (link_ ? target == link_->mac() : !target) {
        switchPending_ = false;
        return;
    }
    // The old sensor stays followed, and counted, until its count is gone from the database too:
    // a count that survived could be resumed under the new sensor after a restart.
    if (!forgetCount())
        return; // tried again at the next tick
    log("sensor choice: ", choice_.describe());
    if (link_)
        link_->disconnect();
    publishConnected(false);
    follow(target);
    switchPending_ = false;
}

void Monitor::serveScanRequest()
{
    log("scanning for ", SCAN_ON_REQUEST.count(), " s, as the settings page asked");
    adapter_.startDiscovery("CSC sensors, for the settings page");
    auto end = Clock::now() + SCAN_ON_REQUEST;
    while (Clock::now() < end) {
        recordHeard(adapter_.objects());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    adapter_.stopDiscovery();
}

void Monitor::recordHeard(const ManagedObjects& objs)
{
    for (auto& [path, ifaces] : objs) {
        auto dev = ifaces.find(DEVICE_IFACE);
        if (dev == ifaces.end() || !hasCsc(dev->second) || !dev->second.count("RSSI"))
            continue;
        auto& props = dev->second;
        std::string address = props.at("Address").get<std::string>();
        int rssi = props.at("RSSI").get<int16_t>();
        store_.sensorHeard(address, deviceName(props), rssi);
        rawLog_.event("heard", address + " rssi=" + std::to_string(rssi));
    }
}

bool Monitor::deviceExists()
{
    auto objs = adapter_.objects();
    auto it = objs.find(sdbus::ObjectPath(link_->path()));
    if (it == objs.end() || !it->second.count(DEVICE_IFACE))
        return false;
    link_->update(it->second.at(DEVICE_IFACE));
    return true;
}

void Monitor::scanStep()
{
    if (!adapter_.discovering())
        adapter_.startDiscovery(link_ ? link_->mac() : "any CSC sensor");
    auto objs = adapter_.objects();
    recordHeard(objs);
    for (auto& [path, ifaces] : objs) {
        auto dev = ifaces.find(DEVICE_IFACE);
        if (dev == ifaces.end())
            continue;
        auto& props = dev->second;
        std::string addr = props.at("Address").get<std::string>();
        bool match = link_ ? addr == link_->mac() : hasCsc(props);
        if (!match || !props.count("RSSI"))
            continue;
        log("found ", addr, " rssi=", props.at("RSSI").get<int16_t>());
        adapter_.stopDiscovery();
        if (!link_) {
            choice_.remember(addr);
            follow(addr);
        }
        found_ = true;
        link_->trust();
        return;
    }
}

void Monitor::startNotify()
{
    auto objs = adapter_.objects();
    rawLog_.event("services resolved", link_->mac());
    SensorInfo info = readSensorInfo(*bus_, objs, link_->path());
    if (info.battery) {
        store_.setState("battery", std::to_string(*info.battery));
        log("battery ", *info.battery, "%");
        rawLog_.event("battery", std::to_string(*info.battery));
    }
    counter_.setKind(info.counter());
    store_.setState("sensor_kind", info.kind.value_or("cadence"));
    rawLog_.event("sensor kind", info.kind.value_or("cadence"));
    if (auto it = objs.find(sdbus::ObjectPath(link_->path())); it != objs.end() && it->second.count(DEVICE_IFACE))
        store_.sensorHeard(link_->mac(), deviceName(it->second.at(DEVICE_IFACE)), std::nullopt, info.kind);
    log(info.kind.value_or("cadence"), " sensor; counting ", counterName(info.counter()), " revolutions");
    if (link_->startNotify(objs, [this](const std::vector<uint8_t>& bytes) { counter_.onMeasurement(bytes); }))
        rawLog_.event("notifications on", link_->mac());
}

void Monitor::publishConnected(bool c)
{
    rawLog_.event(c ? "connected" : "disconnected", link_ ? link_->mac() : "");
    if (c)
        counter_.connected(firstConnection_.exchange(false));
    try {
        store_.setState("connected", c ? "1" : "0");
        if (!c)
            store_.setState("rpm", "0");
    } catch (const std::exception& e) {
        log("storage error publishing the connection: ", e.what()); // from the D-Bus thread: nothing above to catch it
    }
}

} // namespace purr::logger
