// Minimal INI reader for purrdometer.conf and wheels.ini: [section], key = value,
// whole-line and inline comments starting with # or ;.
#pragma once

#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace purr {

class Ini {
public:
    using Section = std::map<std::string, std::string>;

    static std::optional<Ini> load(const std::string& path)
    {
        std::ifstream in(path);
        if (!in)
            return std::nullopt;
        Ini ini;
        std::string line, section;
        while (std::getline(in, line)) {
            line = trim(stripComment(line));
            if (line.empty())
                continue;
            if (line.front() == '[' && line.back() == ']') {
                section = trim(line.substr(1, line.size() - 2));
                ini.sections_[section];
                ini.order_.push_back(section);
            } else if (auto eq = line.find('='); eq != std::string::npos) {
                ini.sections_[section][trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
            }
        }
        return ini;
    }

    std::optional<std::string> get(const std::string& section, const std::string& key) const
    {
        auto s = sections_.find(section);
        if (s == sections_.end())
            return std::nullopt;
        auto k = s->second.find(key);
        if (k == s->second.end() || k->second.empty())
            return std::nullopt;
        return k->second;
    }

    // True for a key present but empty ("diameter =").
    bool has(const std::string& section, const std::string& key) const
    {
        auto s = sections_.find(section);
        return s != sections_.end() && s->second.count(key);
    }

    const std::vector<std::string>& sections() const { return order_; }

private:
    static std::string trim(const std::string& s)
    {
        auto b = s.find_first_not_of(" \t\r\n");
        auto e = s.find_last_not_of(" \t\r\n");
        return b == std::string::npos ? "" : s.substr(b, e - b + 1);
    }

    // A comment is # or ; at the start of a line, or after whitespace.
    static std::string stripComment(const std::string& s)
    {
        for (size_t i = 0; i < s.size(); ++i)
            if ((s[i] == '#' || s[i] == ';') && (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t'))
                return s.substr(0, i);
        return s;
    }

    std::map<std::string, Section> sections_;
    std::vector<std::string> order_;
};

} // namespace purr
