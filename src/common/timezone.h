// The timezone local days are grouped by (docs/intents/data/storage.md#reading-the-history):
// TZ when set, else the zone /etc/localtime names, else UTC. A process reads
// its timezone when it starts, so this is fixed for its lifetime.
#pragma once

#include <cstdlib>
#include <string>
#include <unistd.h>

namespace purr {

inline std::string timezoneKey()
{
    if (const char* tz = std::getenv("TZ"); tz && *tz)
        return tz;
    char buf[256];
    ssize_t n = readlink("/etc/localtime", buf, sizeof buf - 1);
    if (n <= 0)
        return "UTC";
    std::string target(buf, size_t(n));
    auto at = target.find("zoneinfo/");
    return at == std::string::npos ? target : target.substr(at + 9);
}

} // namespace purr
