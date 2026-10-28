// BlueZ's D-Bus names and the shapes of what it returns, plus the GATT UUIDs
// a CSC sensor has (docs/intents/ble/sensor.md).
#pragma once

#include <sdbus-c++/sdbus-c++.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace purr::logger::bluez {

inline const sdbus::ServiceName SERVICE{"org.bluez"};
inline const std::string ADAPTER_PATH = "/org/bluez/hci0";
inline const std::string ADAPTER_IFACE = "org.bluez.Adapter1";
inline const std::string DEVICE_IFACE = "org.bluez.Device1";
inline const std::string CHAR_IFACE = "org.bluez.GattCharacteristic1";
inline const std::string PROPS_IFACE = "org.freedesktop.DBus.Properties";
inline const std::string OM_IFACE = "org.freedesktop.DBus.ObjectManager";
inline const std::string CSC_SERVICE_UUID = "00001816-0000-1000-8000-00805f9b34fb";
inline const std::string CSC_MEASUREMENT_UUID = "00002a5b-0000-1000-8000-00805f9b34fb";
inline const std::string CSC_FEATURE_UUID = "00002a5c-0000-1000-8000-00805f9b34fb";
inline const std::string BATTERY_LEVEL_UUID = "00002a19-0000-1000-8000-00805f9b34fb";

using Props = std::map<std::string, sdbus::Variant>;
using ManagedObjects = std::map<sdbus::ObjectPath, std::map<std::string, Props>>;

// "AA:BB:..." -> "/org/bluez/hci0/dev_AA_BB_..."
inline std::string devicePath(const std::string& mac)
{
    std::string path = ADAPTER_PATH + "/dev_";
    for (char c : mac)
        path += (c == ':') ? '_' : c;
    return path;
}

inline bool hasCsc(const Props& props)
{
    auto it = props.find("UUIDs");
    return it != props.end() && std::ranges::contains(it->second.get<std::vector<std::string>>(), CSC_SERVICE_UUID);
}

inline std::string deviceName(const Props& props)
{
    for (const char* key : {"Name", "Alias"})
        if (auto it = props.find(key); it != props.end())
            return it->second.get<std::string>();
    return "";
}

} // namespace purr::logger::bluez
