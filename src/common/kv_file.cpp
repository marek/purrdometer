#include "common/kv_file.h"

#include <cctype>
#include <cstdio>
#include <format>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>

namespace purr {

std::string percentEncode(const std::string& s)
{
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '@' || c == ' ' || c == ',')
            out += char(c);
        else
            out += std::format("%{:02X}", c);
    }
    return out;
}

std::string percentDecode(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            out += char(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

KeyValues readKeyValues(const std::string& path)
{
    KeyValues kv;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq != std::string::npos)
            kv[line.substr(0, eq)] = percentDecode(line.substr(eq + 1));
    }
    return kv;
}

void writeKeyValues(const std::string& path, const KeyValues& kv, unsigned mode)
{
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out)
            throw std::runtime_error("cannot write " + tmp);
        chmod(tmp.c_str(), mode);
        for (auto& [k, v] : kv)
            out << k << '=' << percentEncode(v) << '\n';
        if (!out.flush())
            throw std::runtime_error("cannot write " + tmp);
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        throw std::runtime_error("cannot replace " + path);
    }
}

} // namespace purr
