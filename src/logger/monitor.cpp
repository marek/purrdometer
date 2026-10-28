#include "logger/monitor.h"

#include "logger/gatt.h"
#include "logger/log.h"

#include <thread>

namespace purr::logger {

using namespace bluez;
using Clock = std::chrono::steady_clock;

Monitor::Monitor(Store& store, const Paths& paths)
    : store_(store), bus_(sdbus::createSystemBusConnection()),
      adapter_(*bus_,
               [this](const std::string& path) {
                   if (link_ && path == link_->path())
                       link_->lost();
               }),
      counter_(store), paths_(paths), settings_({paths.config, paths.overrides, paths.profiles}),
      choice_(paths.sensorFile, sensorChoice(paths.config, paths.overrides)), scanRequest_({paths.scanRequest})
{
    applySettings(loadEditable(paths.config, paths.overrides));
    bus_->enterEventLoopAsync();
}

void Monitor::applySettings(const Editable& e)
{
    counter_.setWakeTurns(e.wakeTurns);
    counter_.setWheel(currentProfile(paths_.profiles));
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
        link_->connect();
    } else if (!link_->connected())
        scanStep();
    else if (link_->servicesResolved() && !link_->notifying())
        startNotify();
}

void Monitor::follow(const std::optional<std::string>& mac)
{
    link_.reset();
    found_ = false;
    if (!mac)
        return;
    link_ = std::make_unique<SensorLink>(*bus_, *mac, [this](bool c) { publishConnected(c); });
    store_.setState("sensor", *mac);
}

void Monitor::followSettings()
{
    if (!settings_.changed())
        return;
    Editable e = loadEditable(paths_.config, paths_.overrides);
    applySettings(e);
    if (!choice_.choose(e.sensor))
        return;
    auto target = choice_.target();
    if (link_ && target == link_->mac())
        return;
    log("sensor choice: ", choice_.describe());
    if (link_)
        link_->disconnect();
    counter_.forget();
    publishConnected(false);
    follow(target);
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
        store_.sensorHeard(props.at("Address").get<std::string>(), deviceName(props), props.at("RSSI").get<int16_t>());
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
    SensorInfo info = readSensorInfo(*bus_, objs, link_->path());
    if (info.battery) {
        store_.setState("battery", std::to_string(*info.battery));
        log("battery ", *info.battery, "%");
    }
    counter_.setKind(info.counter());
    store_.setState("sensor_kind", info.kind.value_or("cadence"));
    if (auto it = objs.find(sdbus::ObjectPath(link_->path())); it != objs.end() && it->second.count(DEVICE_IFACE))
        store_.sensorHeard(link_->mac(), deviceName(it->second.at(DEVICE_IFACE)), std::nullopt, info.kind);
    log(info.kind.value_or("cadence"), " sensor; counting ", counterName(info.counter()), " revolutions");
    link_->startNotify(objs, [this](const std::vector<uint8_t>& bytes) { counter_.onMeasurement(bytes); });
}

void Monitor::publishConnected(bool c)
{
    if (c)
        counter_.connected(firstConnection_.exchange(false));
    store_.setState("connected", c ? "1" : "0");
    if (!c)
        store_.setState("rpm", "0");
}

} // namespace purr::logger
