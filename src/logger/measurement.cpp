#include "logger/measurement.h"

namespace purr::logger {

const char* counterName(Counter c) { return c == Counter::Wheel ? "wheel" : "crank"; }

std::optional<Sample> parseMeasurement(const std::vector<uint8_t>& d, Counter wanted)
{
    if (d.empty())
        return std::nullopt;
    const uint8_t flags = d[0];
    size_t i = 1;
    std::optional<Sample> wheel, crank;
    if (flags & 0x01) {
        if (d.size() < i + 6)
            return std::nullopt;
        wheel = Sample{Counter::Wheel, le32(d, i), le16(d, i + 4)};
        i += 6;
    }
    if ((flags & 0x02) && d.size() >= i + 4)
        crank = Sample{Counter::Crank, le16(d, i), le16(d, i + 2)};
    return wanted == Counter::Wheel ? wheel : crank;
}

} // namespace purr::logger
