// A connected sensor's GATT characteristics: finding one by UUID, reading it,
// and what a sensor says about itself (its battery, cadence or speed).
#pragma once

#include "logger/bluez.h"
#include "logger/measurement.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace purr::logger {

// The object path of the device's characteristic, or empty.
std::string findCharacteristic(const bluez::ManagedObjects& objs, const std::string& devicePath, const std::string& uuid);

std::optional<std::vector<uint8_t>> readCharacteristic(sdbus::IConnection& bus, const bluez::ManagedObjects& objs,
                                                       const std::string& devicePath, const std::string& uuid);

struct SensorInfo {
    std::optional<int> battery;     // percent
    std::optional<std::string> kind; // "speed" or "cadence", from CSC Feature
    Counter counter() const { return kind == std::optional<std::string>("speed") ? Counter::Wheel : Counter::Crank; }
};

SensorInfo readSensorInfo(sdbus::IConnection& bus, const bluez::ManagedObjects& objs, const std::string& devicePath);

} // namespace purr::logger
