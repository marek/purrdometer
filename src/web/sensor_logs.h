// The retained raw sensor-log days shown and downloaded from the settings page
// (docs/intents/ble/raw-log.md#rules).
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace purr::web {

std::vector<std::string> sensorLogDays(const std::string& directory);
std::optional<std::string> readSensorLog(const std::string& directory, const std::string& day);

} // namespace purr::web
