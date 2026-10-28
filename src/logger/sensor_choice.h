// Which sensor to follow: [sensor] address from the settings; without one,
// the first sensor heard, remembered in the sensor file.
#pragma once

#include <optional>
#include <string>

namespace purr::logger {

class SensorChoice {
public:
    SensorChoice(std::string sensorFile, std::optional<std::string> choice)
        : sensorFile_(std::move(sensorFile)), choice_(std::move(choice))
    {
    }

    // The address to follow; nullopt: any CSC sensor, the first one heard.
    std::optional<std::string> target() const;
    // The settings' choice; true when it differs from before.
    bool choose(const std::optional<std::string>& choice);
    // The first sensor heard, when the settings name none.
    void remember(const std::string& address);

    std::string describe() const { return choice_.value_or("the first one heard"); }

private:
    std::string sensorFile_;
    std::optional<std::string> choice_;
};

} // namespace purr::logger
