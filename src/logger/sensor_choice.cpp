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

std::optional<std::string> SensorChoice::target() const { return choice_ ? choice_ : loadMac(sensorFile_); }

bool SensorChoice::choose(const std::optional<std::string>& choice)
{
    if (choice == choice_)
        return false;
    choice_ = choice;
    return true;
}

void SensorChoice::remember(const std::string& address) { std::ofstream(sensorFile_) << address << '\n'; }

} // namespace purr::logger
