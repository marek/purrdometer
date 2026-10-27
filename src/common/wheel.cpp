#include "common/wheel.h"

#include "common/ini.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <regex>
#include <stdexcept>

namespace purr {

double Wheel::circumferenceM() const { return M_PI * diameterM; }

const Wheel* WheelPresets::find(const std::string& key) const
{
    for (auto& w : wheels)
        if (w.key == key)
            return &w;
    return nullptr;
}

namespace {

// Cat wheels are roughly 0.9-1.6 m across.
constexpr double MIN_DIAMETER_M = 0.5, MAX_DIAMETER_M = 2.0;

double unitScale(const std::string& unit)
{
    if (unit == "cm") return 0.01;
    if (unit == "mm") return 0.001;
    if (unit == "m") return 1.0;
    return INCH; // in, inch, inches, "
}

} // namespace

bool validUnits(const std::string& units) { return units == "cm" || units == "in"; }

double parseLength(const std::string& text, const std::string& defaultUnit)
{
    static const std::regex re(R"(\s*([0-9]+(?:\.[0-9]+)?)\s*(in|inch|inches|"|cm|mm|m)?\s*)");
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    std::smatch m;
    if (!std::regex_match(lower, m, re) || (!m[2].matched && defaultUnit.empty()))
        throw std::invalid_argument("not a length with a unit (cm, in, mm, m): '" + text + "'");
    return std::stod(m[1]) * unitScale(m[2].matched ? std::string(m[2]) : defaultUnit);
}

double parseDiameter(const std::string& text, const std::string& defaultUnit)
{
    double metres = parseLength(text, defaultUnit);
    if (metres >= MIN_DIAMETER_M && metres <= MAX_DIAMETER_M)
        return metres;
    std::string hint;
    static const std::regex bare(R"(\s*([0-9]+(?:\.[0-9]+)?)\s*)");
    std::smatch m;
    if (std::regex_match(text, m, bare)) {
        std::string other = defaultUnit == "cm" ? "in" : "cm";
        double alt = std::stod(m[1]) * unitScale(other);
        if (alt >= MIN_DIAMETER_M && alt <= MAX_DIAMETER_M)
            hint = "; did you mean " + std::string(m[1]) + other + "?";
    }
    char cm[32];
    std::snprintf(cm, sizeof cm, "%.0f cm", metres * 100);
    throw std::invalid_argument("'" + text + "' is " + cm + ", not a cat wheel's diameter" + hint);
}

std::string showLength(double metres, const std::string& units)
{
    char cm[32], in[32];
    std::snprintf(cm, sizeof cm, "%.1f cm", metres * 100);
    std::snprintf(in, sizeof in, "%.1f in", metres / INCH);
    return units == "in" ? std::string(in) + " (" + cm + ")" : std::string(cm) + " (" + in + ")";
}

WheelPresets loadPresets(const std::string& path)
{
    auto ini = Ini::load(path);
    if (!ini)
        throw std::runtime_error("wheel presets not found: " + path);
    WheelPresets presets;
    for (auto& key : ini->sections()) {
        Wheel w;
        w.key = key;
        w.name = ini->get(key, "name").value_or(key);
        auto inner = ini->get(key, "inner");
        if (!inner)
            throw std::runtime_error(path + ": [" + key + "] has no inner diameter");
        w.diameterM = parseDiameter(*inner);
        if (auto v = ini->get(key, "outer"))
            w.outerM = parseLength(*v);
        if (auto v = ini->get(key, "track"))
            w.trackM = parseLength(*v);
        auto est = ini->get(key, "estimated").value_or("false");
        w.estimated = est == "true" || est == "yes" || est == "1" || est == "on";
        w.source = ini->get(key, "source").value_or("");
        presets.wheels.push_back(w);
    }
    return presets;
}

Wheel resolveWheel(const WheelPresets& presets, const std::optional<std::string>& model,
                   const std::optional<std::string>& diameter, const std::string& units)
{
    std::string key = model.value_or("");
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
    const Wheel* base = key.empty() ? nullptr : presets.find(key);
    if (!key.empty() && !base)
        throw std::invalid_argument("unknown wheel model '" + *model + "'");
    if (diameter) {
        Wheel w;
        w.name = base ? base->name + ", measured" : "custom wheel";
        w.diameterM = parseDiameter(*diameter, units);
        if (base) {
            w.outerM = base->outerM;
            w.trackM = base->trackM;
        }
        return w;
    }
    if (!base)
        base = presets.find(DEFAULT_WHEEL);
    if (!base)
        throw std::invalid_argument(std::string("default wheel '") + DEFAULT_WHEEL + "' missing from presets");
    return *base;
}

std::string describe(const Wheel& w, const std::string& units)
{
    std::string text = w.name + ": running diameter " + showLength(w.diameterM, units);
    if (w.outerM) {
        std::string outer = showLength(*w.outerM, units);
        text += ", outer " + outer.substr(0, outer.find(" ("));
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, ", %.3f m per rotation", w.circumferenceM());
    text += buf;
    if (w.estimated)
        text += " (inner diameter estimated: measure it and set diameter for accuracy)";
    return text;
}

} // namespace purr
