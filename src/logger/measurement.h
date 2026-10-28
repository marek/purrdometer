// The revolutions a CSC Measurement carries (docs/intents/ble/sensor.md#the-measurement):
// cadence sensors count crank revolutions, speed sensors wheel revolutions;
// either is one turn of the cat wheel.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace purr::logger {

enum class Counter { Crank, Wheel }; // 16-bit crank revolutions, 32-bit wheel revolutions

const char* counterName(Counter c);

struct Sample {
    Counter counter = Counter::Crank;
    uint32_t revs = 0;      // cumulative revolutions, wrapping at the counter's width
    uint16_t eventTime = 0; // time of the last revolution, 1/1024 s (wraps every 64 s)
};

std::optional<Sample> parseMeasurement(const std::vector<uint8_t>& d, Counter wanted);

inline uint16_t le16(const std::vector<uint8_t>& d, size_t i) { return uint16_t(d[i] | (d[i + 1] << 8)); }
inline uint32_t le32(const std::vector<uint8_t>& d, size_t i) { return le16(d, i) | (uint32_t(le16(d, i + 2)) << 16); }

} // namespace purr::logger
