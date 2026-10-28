// Wheel profiles (docs/intents/data/wheels.md#profiles): your wheels, each a
// number, a name and a preset or a measured diameter; which one is current.
// Rotations are stored under a profile's number, so numbers are never reused.
#pragma once

#include "common/paths.h"
#include "common/wheel.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace purr {

constexpr int FIRST_PROFILE = 1;

struct Profile {
    int id = 0;
    std::string name{};
    std::optional<std::string> preset{}; // a catalogue key
    std::optional<double> diameterM{};   // measured; wins over the preset's
};

struct Profiles {
    std::vector<Profile> list; // by number
    int current = FIRST_PROFILE;

    const Profile* find(int id) const;
    int nextId() const;        // one past the highest ever written
    int highest = 0;           // kept in the file, so a deleted number is not reused
};

// The file, or nullopt when there is none. Problems go to errors.
std::optional<Profiles> readProfiles(const std::string& path, std::vector<std::string>& errors);
// Written whole and renamed into place; diameters in cm. Throws std::runtime_error.
void writeProfiles(const Profiles& p, const std::string& path);
// Profile 1 from [wheel] model and diameter, for a Pi without the file.
Profiles seedProfiles(const std::optional<std::string>& model, const std::optional<std::string>& diameter,
                      const std::string& units, const WheelPresets& presets, std::vector<std::string>& errors);

// The wheel a profile runs on; throws std::invalid_argument for an unknown preset.
Wheel wheelOf(const Profile& p, const WheelPresets& presets);

// The problems with a profile as the settings page submits it; empty when valid.
std::vector<std::string> validate(const Profile& p, const WheelPresets& presets);

// [profiles] current alone, for the logger: FIRST_PROFILE without the file.
int currentProfile(const std::string& path = PROFILES_PATH);

// "123.19cm": how a diameter is written to the file.
std::string writtenDiameter(double metres);

} // namespace purr
