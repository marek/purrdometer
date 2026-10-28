#include "setup/nmcli_parse.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace purr::setup {

std::vector<std::string> splitTerse(std::string_view line)
{
    std::vector<std::string> fields(1);
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size())
            fields.back() += line[++i];
        else if (line[i] == ':')
            fields.emplace_back();
        else
            fields.back() += line[i];
    }
    return fields;
}

std::vector<Network> parseScan(const std::string& out)
{
    std::vector<Network> all;
    std::istringstream in(out);
    std::string line;
    while (std::getline(in, line)) {
        auto f = splitTerse(line);
        if (f.size() < 3 || f[0].empty())
            continue;
        all.push_back({f[0], std::atoi(f[1].c_str()), f[2] == "--" ? "" : f[2]});
    }
    std::ranges::stable_sort(all, [](auto& a, auto& b) { return a.signal > b.signal; });
    std::vector<Network> unique;
    for (auto& n : all)
        if (std::ranges::none_of(unique, [&](auto& u) { return u.ssid == n.ssid; }))
            unique.push_back(n);
    return unique;
}

std::vector<std::string> parseWifiProfiles(const std::string& out, std::string_view except)
{
    std::vector<std::string> profiles;
    std::istringstream in(out);
    std::string line;
    while (std::getline(in, line)) {
        auto f = splitTerse(line);
        if (f.size() >= 2 && f[1] == "802-11-wireless" && f[0] != except)
            profiles.push_back(f[0]);
    }
    return profiles;
}

std::string parseAddress(const std::string& out)
{
    std::string a = out.substr(0, out.find_first_of("/|\n"));
    return a;
}

} // namespace purr::setup
