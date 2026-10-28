// Wheels: presets from wheels.conf, measured diameters, distance per rotation.
// docs/intents/data/wheels.md owns the rules.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace purr {

constexpr double INCH = 0.0254;

struct Wheel {
    std::string key{};         // preset key, empty for a custom wheel
    std::string name{};
    double diameterM = 0;      // running (inner) diameter
    std::optional<double> outerM{}, trackM{};
    bool estimated = false;    // no published inner diameter
    std::string source{};

    double circumferenceM() const;
    double km(long long rotations) const { return rotations * circumferenceM() / 1000; }
    double kmh(int rpm) const { return rpm * circumferenceM() * 60 / 1000; }
};

struct WheelPresets {
    std::vector<Wheel> wheels; // in file order
    const Wheel* find(const std::string& key) const;
};

constexpr const char* DEFAULT_WHEEL = "ferris-g5";
// [wheel] units: how wheel sizes are shown, and what a bare diameter number means.
constexpr const char* DEFAULT_UNITS = "cm";
bool validUnits(const std::string& units); // "cm" or "in"
std::string toLower(std::string s);

// "48.5in", "48.5\"", "123 cm", "1230mm", "1.23m" -> metres; a bare number takes
// defaultUnit. Throws std::invalid_argument.
double parseLength(const std::string& text, const std::string& defaultUnit = "");
// A wheel diameter: refuses sizes no cat wheel has, hinting at the other unit.
double parseDiameter(const std::string& text, const std::string& defaultUnit = DEFAULT_UNITS);
// "121.9 cm (48.0 in)", the preferred unit first.
std::string showLength(double metres, const std::string& units = DEFAULT_UNITS);

WheelPresets loadPresets(const std::string& path); // throws std::runtime_error

// A measured diameter wins over the model's; the model defaults to DEFAULT_WHEEL.
Wheel resolveWheel(const WheelPresets& presets, const std::optional<std::string>& model,
                   const std::optional<std::string>& diameter,
                   const std::string& units = DEFAULT_UNITS); // throws std::invalid_argument

std::string describe(const Wheel& w, const std::string& units = DEFAULT_UNITS);

} // namespace purr
