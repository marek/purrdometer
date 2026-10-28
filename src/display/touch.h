// The GT1151 touch controller over I2C: a finger landing and lifting, in
// the controller's panel coordinates.
#pragma once

#include "display/gpio.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace purr::display {

// A finger touching (Down, where it landed) or lifting (Up, where it was last).
struct TouchEvent {
    enum class Kind { Down, Up } kind;
    int x, y; // the touch controller's panel coordinates
};

class Touch {
public:
    Touch(Gpio& gpio, const std::string& i2cdev = "/dev/i2c-1");
    ~Touch();
    Touch(const Touch&) = delete;
    Touch& operator=(const Touch&) = delete;

    std::string productId();
    // Reads the controller: a finger landing or lifting, if either happened.
    std::optional<TouchEvent> poll();
    bool fingerDown() const { return down_; }

private:
    std::vector<uint8_t> read(uint16_t reg, size_t n);
    void write(uint16_t reg, uint8_t value);

    Gpio& gpio_;
    int fd_ = -1;
    bool down_ = false;
    int lastX_ = 0, lastY_ = 0;
    std::chrono::steady_clock::time_point lastReport_{};
};

} // namespace purr::display
