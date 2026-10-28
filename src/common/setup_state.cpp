#include "common/setup_state.h"

#include "common/kv_file.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace purr::setup {

namespace {

std::string get(const KeyValues& kv, const std::string& key)
{
    auto it = kv.find(key);
    return it == kv.end() ? std::string() : it->second;
}

std::time_t getTime(const KeyValues& kv, const std::string& key)
{
    try {
        return std::time_t(std::stoll(get(kv, key)));
    } catch (const std::exception&) {
        return 0;
    }
}

} // namespace

const char* phaseName(Phase p)
{
    switch (p) {
    case Phase::Off: return "off";
    case Phase::Starting: return "starting";
    case Phase::Hotspot: return "hotspot";
    case Phase::Joining: return "joining";
    case Phase::Joined: return "joined";
    }
    return "off";
}

Phase phaseFrom(const std::string& s)
{
    for (Phase p : {Phase::Starting, Phase::Hotspot, Phase::Joining, Phase::Joined})
        if (s == phaseName(p))
            return p;
    return Phase::Off;
}

State readState(const std::string& path)
{
    auto kv = readKeyValues(path);
    State s;
    s.phase = phaseFrom(get(kv, "phase"));
    s.ssid = get(kv, "ssid");
    s.password = get(kv, "password");
    s.network = get(kv, "network");
    s.message = get(kv, "message");
    s.deadline = getTime(kv, "deadline");
    s.until = getTime(kv, "until");
    return s;
}

void writeState(const State& s, const std::string& path)
{
    writeKeyValues(path, {{"phase", phaseName(s.phase)},
                          {"ssid", s.ssid},
                          {"password", s.password},
                          {"network", s.network},
                          {"message", s.message},
                          {"deadline", std::to_string(s.deadline)},
                          {"until", std::to_string(s.until)}});
}

void requestSetup(const std::string& path)
{
    std::ofstream(path, std::ios::trunc) << std::time(nullptr) << '\n';
}

std::vector<Network> readNetworks(const std::string& path)
{
    std::vector<Network> out;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        auto a = line.find('\t');
        auto b = a == std::string::npos ? a : line.find('\t', a + 1);
        if (b == std::string::npos)
            continue;
        out.push_back({percentDecode(line.substr(b + 1)), std::atoi(line.substr(0, a).c_str()),
                       percentDecode(line.substr(a + 1, b - a - 1))});
    }
    return out;
}

void writeNetworks(const std::vector<Network>& networks, const std::string& path)
{
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        for (auto& n : networks)
            out << n.signal << '\t' << percentEncode(n.security) << '\t' << percentEncode(n.ssid) << '\n';
    }
    std::rename(tmp.c_str(), path.c_str());
}

} // namespace purr::setup
