#include "common/profiles.h"

#include "common/ini.h"

#include <algorithm>
#include <cstdio>
#include <format>
#include <fstream>
#include <stdexcept>

namespace purr {

namespace {

constexpr const char* SECTION_PREFIX = "profile.";
constexpr size_t NAME_MAX = 40;

std::optional<int> number(const std::string& s)
{
    try {
        size_t used = 0;
        int n = std::stoi(s, &used);
        return used == s.size() && n > 0 ? std::optional(n) : std::nullopt;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace

const Profile* Profiles::find(int id) const
{
    auto it = std::ranges::find(list, id, &Profile::id);
    return it == list.end() ? nullptr : &*it;
}

int Profiles::nextId() const
{
    int top = highest;
    for (auto& p : list)
        top = std::max(top, p.id);
    return top + 1;
}

std::optional<Profiles> readProfiles(const std::string& path, std::vector<std::string>& errors)
{
    auto ini = Ini::load(path);
    if (!ini)
        return std::nullopt;
    Profiles out;
    out.list.clear();
    for (auto& section : ini->sections()) {
        if (!section.starts_with(SECTION_PREFIX))
            continue;
        auto id = number(section.substr(std::string_view(SECTION_PREFIX).size()));
        if (!id) {
            errors.push_back(path + ": [" + section + "] is not a profile number");
            continue;
        }
        Profile p{.id = *id, .name = ini->get(section, "name").value_or("Wheel " + std::to_string(*id))};
        if (auto v = ini->get(section, "preset"))
            p.preset = toLower(*v);
        if (auto v = ini->get(section, "diameter")) {
            try {
                p.diameterM = parseDiameter(*v, "cm");
            } catch (const std::invalid_argument& e) {
                errors.push_back(path + ": [" + section + "] " + e.what());
            }
        }
        out.list.push_back(p);
    }
    std::ranges::sort(out.list, {}, &Profile::id);
    if (auto v = ini->get("profiles", "highest"); v && number(*v))
        out.highest = *number(*v);
    auto cur = ini->get("profiles", "current");
    out.current = cur && number(*cur) ? *number(*cur) : FIRST_PROFILE;
    if (out.list.empty())
        return std::nullopt;
    if (!out.find(out.current)) {
        errors.push_back(path + ": [profiles] current names no profile; using " + out.list.front().name);
        out.current = out.list.front().id;
    }
    return out;
}

std::string writtenDiameter(double metres) { return std::format("{:.2f}cm", metres * 100); }

void writeProfiles(const Profiles& p, const std::string& path)
{
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out)
            throw std::runtime_error("cannot write " + tmp);
        out << "# Your wheels, written by the purrdometer settings page. Rotations are stored\n"
               "# under a profile's number: change a profile to correct its distance, add one\n"
               "# for a new wheel. Numbers are never reused.\n\n"
            << "[profiles]\ncurrent = " << p.current << "\nhighest = " << p.nextId() - 1 << "\n";
        for (auto& prof : p.list) {
            out << "\n[" << SECTION_PREFIX << prof.id << "]\nname = " << prof.name << "\n";
            if (prof.preset)
                out << "preset = " << *prof.preset << "\n";
            if (prof.diameterM)
                out << "diameter = " << writtenDiameter(*prof.diameterM) << "\n";
        }
        if (!out.flush())
            throw std::runtime_error("cannot write " + tmp);
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        throw std::runtime_error("cannot replace " + path);
    }
}

Profiles seedProfiles(const std::optional<std::string>& model, const std::optional<std::string>& diameter,
                      const std::string& units, const WheelPresets& presets, std::vector<std::string>& errors)
{
    Profile p{.id = FIRST_PROFILE};
    std::string key = toLower(model.value_or(DEFAULT_WHEEL));
    if (presets.find(key))
        p.preset = key;
    else
        errors.push_back("[wheel] unknown wheel model '" + key + "'");
    if (diameter) {
        try {
            p.diameterM = parseDiameter(*diameter, units);
        } catch (const std::invalid_argument& e) {
            errors.push_back(std::string("[wheel] ") + e.what());
        }
    }
    if (!p.preset && !p.diameterM)
        p.preset = DEFAULT_WHEEL;
    const Wheel* base = p.preset ? presets.find(*p.preset) : nullptr;
    p.name = base ? base->name : "My wheel";
    Profiles out;
    out.list.push_back(p);
    return out;
}

Wheel wheelOf(const Profile& p, const WheelPresets& presets)
{
    const Wheel* base = p.preset ? presets.find(*p.preset) : nullptr;
    if (p.preset && !base)
        throw std::invalid_argument("unknown wheel model '" + *p.preset + "'");
    Wheel w = base ? *base : Wheel{.name = "custom wheel"};
    if (p.diameterM) {
        w.diameterM = *p.diameterM;
        w.estimated = false;
        if (base) {
            w.key.clear();
            w.name = base->name + ", measured";
        }
    }
    return w;
}

std::vector<std::string> validate(const Profile& p, const WheelPresets& presets)
{
    std::vector<std::string> problems;
    if (p.name.empty() || p.name.size() > NAME_MAX || p.name.find_first_of("\n[]") != std::string::npos)
        problems.push_back(std::format("give the wheel a name of up to {} characters", NAME_MAX));
    if (p.preset && !presets.find(*p.preset))
        problems.push_back("unknown wheel model '" + *p.preset + "'");
    if (!p.preset && !p.diameterM)
        problems.push_back("choose a wheel model, or give its measured diameter");
    return problems;
}

int currentProfile(const std::string& path)
{
    auto ini = Ini::load(path);
    if (!ini)
        return FIRST_PROFILE;
    auto v = ini->get("profiles", "current");
    return v && number(*v) ? *number(*v) : FIRST_PROFILE;
}

} // namespace purr
