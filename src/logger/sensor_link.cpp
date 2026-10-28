#include "logger/sensor_link.h"

#include "logger/gatt.h"
#include "logger/log.h"

namespace purr::logger {

using namespace bluez;

SensorLink::SensorLink(sdbus::IConnection& bus, std::string mac, std::function<void(bool)> onConnected)
    : bus_(bus), mac_(std::move(mac)), path_(devicePath(mac_)), onConnected_(std::move(onConnected))
{
    device_ = sdbus::createProxy(bus_, SERVICE, sdbus::ObjectPath{path_});
    device_->uponSignal("PropertiesChanged").onInterface(PROPS_IFACE).call(
        [this, guard = guard_](const std::string& iface, const Props& changed, const std::vector<std::string>&) {
            if (iface != DEVICE_IFACE)
                return;
            guard->run([&] {
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
        });
    log("target sensor ", mac_);
}

SensorLink::~SensorLink() { guard_->close(); }

void SensorLink::setConnected(bool c)
{
    connected_ = c;
    if (!c) {
        servicesResolved_ = false;
        notifying_ = false;
    }
    onConnected_(c);
}

void SensorLink::update(const Props& props)
{
    bool c = props.at("Connected").get<bool>();
    if (c != connected_)
        setConnected(c);
    servicesResolved_ = props.at("ServicesResolved").get<bool>();
}

void SensorLink::lost()
{
    log("device object removed by BlueZ");
    setConnected(false);
}

void SensorLink::trust() { device_->setProperty("Trusted").onInterface(DEVICE_IFACE).toValue(true); }

void SensorLink::connect()
{
    log("connecting");
    try {
        device_->callMethod("Connect").onInterface(DEVICE_IFACE).withTimeout(std::chrono::seconds(30));
    } catch (const sdbus::Error& e) {
        log("connect failed (", e.getMessage(), "), waiting for advertisement");
    }
}

void SensorLink::disconnect()
{
    if (!connected_)
        return;
    try {
        device_->callMethod("Disconnect").onInterface(DEVICE_IFACE);
    } catch (const sdbus::Error&) {
    }
}

bool SensorLink::startNotify(const ManagedObjects& objs, std::function<void(const std::vector<uint8_t>&)> onValue)
{
    auto charPath = findCharacteristic(objs, path_, CSC_MEASUREMENT_UUID);
    if (charPath.empty()) {
        log("CSC Measurement characteristic not found yet");
        return false;
    }
    characteristic_ = sdbus::createProxy(bus_, SERVICE, sdbus::ObjectPath{charPath});
    characteristic_->uponSignal("PropertiesChanged").onInterface(PROPS_IFACE).call(
        [onValue = std::move(onValue), guard = guard_](const std::string& iface, const Props& changed,
                                                        const std::vector<std::string>&) {
            if (iface != CHAR_IFACE)
                return;
            if (auto it = changed.find("Value"); it != changed.end())
                guard->run([&] { onValue(it->second.get<std::vector<uint8_t>>()); });
        });
    // The notification session belongs to this D-Bus connection and lives as long as it does.
    characteristic_->callMethod("StartNotify").onInterface(CHAR_IFACE);
    notifying_ = true;
    log("CSC notifications on");
    return true;
}

} // namespace purr::logger
