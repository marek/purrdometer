#include "common/kv_file.h"

#include "common/atomic_file.h"

#include <cctype>
#include <cstdio>
#include <format>
#include <fstream>
#include <iterator>
#include <sstream>
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
    std::ifstream in(path);
    return parseKeyValues(std::string(std::istreambuf_iterator<char>(in), {}));
}

KeyValues parseKeyValues(const std::string& text)
{
    KeyValues kv;
    std::istringstream in(text);
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
    std::string text;
    for (auto& [k, v] : kv)
        text += k + '=' + percentEncode(v) + '\n';
    replaceFile(path, text, mode);
}

} // namespace purr
