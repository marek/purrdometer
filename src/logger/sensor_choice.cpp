#include "logger/sensor_choice.h"

#include "common/config.h"

#include <fstream>

namespace purr::logger {

namespace {

std::optional<std::string> loadMac(const std::string& file)
{
    std::ifstream in(file);
    std::string mac;
    if (in && std::getline(in, mac) && mac.size() >= 17)
        return normalizeAddress(mac.substr(0, 17));
    return std::nullopt;
}

} // namespace

SensorChoice::SensorChoice(const Paths& paths)
    : paths_(paths), settings_({paths.config, paths.overrides}), choice_(sensorChoice(paths.config, paths.overrides))
{
}

std::optional<std::string> SensorChoice::target() const { return choice_ ? choice_ : loadMac(paths_.sensorFile); }

bool SensorChoice::changed()
{
    if (!settings_.changed())
        return false;
    auto choice = sensorChoice(paths_.config, paths_.overrides);
    if (choice == choice_)
        return false;
    choice_ = choice;
    return true;
}

void SensorChoice::remember(const std::string& address) { std::ofstream(paths_.sensorFile) << address << '\n'; }

} // namespace purr::logger
