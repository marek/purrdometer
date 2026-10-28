#include "common/config.h"

#include "common/ini.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

namespace purr {

namespace {

struct Range {
    int lo, hi;
};
constexpr Range FULL_REFRESH_EVERY{1, 200};
constexpr Range ACTIVE_REFRESH_S{1, 60};
constexpr Range IDLE_REFRESH_S{10, 3600};
constexpr Range MISSED_TURNS{0, 10};

std::string lower(const std::string& s) { return toLower(s); }

std::optional<bool> parseBool(const std::string& v)
{
    auto s = lower(v);
    if (s == "1" || s == "yes" || s == "true" || s == "on")
        return true;
    if (s == "0" || s == "no" || s == "false" || s == "off")
        return false;
    return std::nullopt;
}

std::optional<int> parseInt(const std::string& v)
{
    int n = 0;
    auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), n);
    if (ec != std::errc{} || end != v.data() + v.size())
        return std::nullopt;
    return n;
}

std::string rangeError(const char* key, Range r, const char* section = "display")
{
    return std::string("[") + section + "] " + key + " must be a whole number from " + std::to_string(r.lo) + " to " +
           std::to_string(r.hi);
}

// Applies one file's values over e; problems go to errors and leave the earlier value.
void apply(const Ini& ini, Editable& e, std::vector<std::string>& errors)
{
    if (auto v = ini.get("wheel", "units")) {
        if (validUnits(lower(*v)))
            e.units = lower(*v);
        else
            errors.push_back("[wheel] units must be cm or in, not '" + *v + "'");
    }
    if (auto v = ini.get("wheel", "model"))
        e.model = lower(*v);
    if (auto v = ini.get("wheel", "diameter"))
        e.diameter = *v;
    else if (ini.has("wheel", "diameter"))
        e.diameter.reset(); // "diameter =" in the overrides clears a measured diameter from /etc
    if (auto v = ini.get("display", "rotate180")) {
        if (auto b = parseBool(*v))
            e.rotate180 = *b;
        else
            errors.push_back("[display] rotate180 must be true or false");
    }
    auto number = [&](const char* key, int& field, Range r, const char* section = "display") {
        if (auto v = ini.get(section, key)) {
            auto n = parseInt(*v);
            if (n && *n >= r.lo && *n <= r.hi)
                field = *n;
            else
                errors.push_back(rangeError(key, r, section));
        }
    };
    number("wake_turns", e.wakeTurns, MISSED_TURNS, "sensor");
    number("full_refresh_every", e.fullRefreshEvery, FULL_REFRESH_EVERY);
    number("active_refresh_s", e.activeRefreshS, ACTIVE_REFRESH_S);
    number("idle_refresh_s", e.idleRefreshS, IDLE_REFRESH_S);
    if (auto v = ini.get("sensor", "address")) {
        if (auto a = normalizeAddress(*v))
            e.sensor = a;
        else
            errors.push_back("[sensor] address is not a Bluetooth address: '" + *v + "'");
    } else if (ini.has("sensor", "address")) {
        e.sensor.reset(); // "address =" in the overrides goes back to the first sensor heard
    }
}

} // namespace

std::optional<std::string> normalizeAddress(const std::string& s)
{
    if (s.size() != 17)
        return std::nullopt;
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = char(std::toupper(static_cast<unsigned char>(s[i])));
        bool ok = i % 3 == 2 ? c == ':' : std::isxdigit(static_cast<unsigned char>(c));
        if (!ok)
            return std::nullopt;
        out += c;
    }
    return out;
}

Editable loadEditable(const std::string& conf, const std::string& overrides)
{
    Editable e;
    std::vector<std::string> ignored;
    if (auto ini = Ini::load(conf))
        apply(*ini, e, ignored);
    if (auto ini = Ini::load(overrides))
        apply(*ini, e, ignored);
    return e;
}

std::optional<std::string> sensorChoice(const std::string& conf, const std::string& overrides)
{
    return loadEditable(conf, overrides).sensor;
}

const char* accessName(SettingsAccess a)
{
    switch (a) {
    case SettingsAccess::Lan: return "lan";
    case SettingsAccess::Pin: return "pin";
    case SettingsAccess::Open: return "open";
    case SettingsAccess::Off: return "off";
    }
    return "lan";
}

std::vector<std::string> validate(const Editable& e, const WheelPresets& presets)
{
    std::vector<std::string> problems;
    if (!validUnits(e.units))
        problems.push_back("units must be cm or in");
    try {
        resolveWheel(presets, e.model, e.diameter, e.units);
    } catch (const std::invalid_argument& ex) {
        problems.push_back(ex.what());
    }
    auto in = [](int v, Range r) { return v >= r.lo && v <= r.hi; };
    if (!in(e.fullRefreshEvery, FULL_REFRESH_EVERY))
        problems.push_back(rangeError("full_refresh_every", FULL_REFRESH_EVERY));
    if (!in(e.activeRefreshS, ACTIVE_REFRESH_S))
        problems.push_back(rangeError("active_refresh_s", ACTIVE_REFRESH_S));
    if (!in(e.idleRefreshS, IDLE_REFRESH_S))
        problems.push_back(rangeError("idle_refresh_s", IDLE_REFRESH_S));
    if (!in(e.wakeTurns, MISSED_TURNS))
        problems.push_back(rangeError("wake_turns", MISSED_TURNS, "sensor"));
    if (e.sensor && !normalizeAddress(*e.sensor))
        problems.push_back("the sensor is not a Bluetooth address: " + *e.sensor);
    return problems;
}

Config loadConfig(const WheelPresets& presets, const std::string& conf, const std::string& overrides)
{
    Config c;
    if (auto ini = Ini::load(conf)) {
        apply(*ini, c.editable, c.errors);
        if (auto v = ini->get("web", "bind"))
            c.bind = *v;
        if (auto v = ini->get("web", "port")) {
            if (auto n = parseInt(*v); n && *n > 0 && *n < 65536)
                c.port = *n;
            else
                c.errors.push_back("[web] port must be a number from 1 to 65535");
        }
        if (auto v = ini->get("web", "settings_access")) {
            auto a = lower(*v);
            if (a == "lan") c.access = SettingsAccess::Lan;
            else if (a == "pin") c.access = SettingsAccess::Pin;
            else if (a == "open") c.access = SettingsAccess::Open;
            else if (a == "off") c.access = SettingsAccess::Off;
            else {
                c.errors.push_back("[web] settings_access must be lan, pin, open or off, not '" + *v + "'");
                c.access = SettingsAccess::Off; // an unreadable access rule never widens access
            }
        }
        if (auto v = ini->get("web", "settings_pin"))
            c.pin = *v;
    }
    if (c.access == SettingsAccess::Pin && c.pin.empty()) {
        // A PIN mode without a PIN would let anyone save.
        c.errors.push_back("[web] settings_access = pin needs settings_pin; saving is off until it is set");
        c.access = SettingsAccess::Off;
    }
    if (auto ini = Ini::load(overrides)) {
        c.overridden = true;
        apply(*ini, c.editable, c.errors);
    }
    try {
        c.wheel = resolveWheel(presets, c.editable.model, c.editable.diameter, c.editable.units);
    } catch (const std::invalid_argument& e) {
        // Keep running on the default wheel; pages and --check-config show the error.
        c.errors.push_back(std::string("[wheel] ") + e.what());
        c.wheel = resolveWheel(presets, std::nullopt, std::nullopt);
    }
    return c;
}

void saveOverrides(const Editable& e, const std::string& path)
{
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp);
        if (!out)
            throw std::runtime_error("cannot write " + tmp);
        out << "# Written by the purrdometer web UI. Overrides /etc/purrdometer/purrdometer.conf;\n"
               "# delete this file (or use Reset in the UI) to go back to it.\n\n"
               "[wheel]\n"
            << "units = " << e.units << "\n"
            << "model = " << e.model << "\n"
            << "diameter = " << e.diameter.value_or("") << "\n\n"
            << "[display]\n"
            << "rotate180 = " << (e.rotate180 ? "true" : "false") << "\n"
            << "full_refresh_every = " << e.fullRefreshEvery << "\n"
            << "active_refresh_s = " << e.activeRefreshS << "\n"
            << "idle_refresh_s = " << e.idleRefreshS << "\n\n"
            << "[sensor]\n"
            << "address = " << e.sensor.value_or("") << "\n"
            << "wake_turns = " << e.wakeTurns << "\n";
        out.flush();
        if (!out)
            throw std::runtime_error("cannot write " + tmp);
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        throw std::runtime_error("cannot replace " + path);
    }
}

} // namespace purr
