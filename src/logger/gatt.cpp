#include "logger/gatt.h"

#include "logger/log.h"

namespace purr::logger {

using namespace bluez;

std::string findCharacteristic(const ManagedObjects& objs, const std::string& devicePath, const std::string& uuid)
{
    for (auto& [path, ifaces] : objs) {
        auto ch = ifaces.find(CHAR_IFACE);
        if (ch != ifaces.end() && path.starts_with(devicePath + "/") && ch->second.at("UUID").get<std::string>() == uuid)
            return path;
    }
    return {};
}

std::optional<std::vector<uint8_t>> readCharacteristic(sdbus::IConnection& bus, const ManagedObjects& objs,
                                                       const std::string& devicePath, const std::string& uuid)
{
    auto path = findCharacteristic(objs, devicePath, uuid);
    if (path.empty())
        return std::nullopt;
    try {
        std::vector<uint8_t> v;
        sdbus::createProxy(bus, SERVICE, sdbus::ObjectPath{path})
            ->callMethod("ReadValue").onInterface(CHAR_IFACE).withArguments(Props{}).storeResultsTo(v);
        return v;
    } catch (const sdbus::Error& e) {
        log("read of ", uuid, " failed: ", e.getMessage());
        return std::nullopt;
    }
}

SensorInfo readSensorInfo(sdbus::IConnection& bus, const ManagedObjects& objs, const std::string& devicePath)
{
    SensorInfo info;
    if (auto v = readCharacteristic(bus, objs, devicePath, BATTERY_LEVEL_UUID); v && !v->empty())
        info.battery = (*v)[0];
    if (auto v = readCharacteristic(bus, objs, devicePath, CSC_FEATURE_UUID); v && v->size() >= 2) {
        uint16_t features = le16(*v, 0);
        if (features & 0x01)
            info.kind = "speed"; // wheel revolutions; a sensor with both is used as a speed sensor
        else if (features & 0x02)
            info.kind = "cadence";
    }
    return info;
}

} // namespace purr::logger
