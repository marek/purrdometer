// key=value files with percent-encoded values, written whole and renamed into
// place: how the programs hand each other small records (setup mode's state
// and form), whatever characters a password holds.
#pragma once

#include <map>
#include <string>

namespace purr {

using KeyValues = std::map<std::string, std::string>;

KeyValues readKeyValues(const std::string& path);
void writeKeyValues(const std::string& path, const KeyValues& kv, unsigned mode = 0644); // throws std::runtime_error

std::string percentEncode(const std::string& s);
std::string percentDecode(const std::string& s);

} // namespace purr
