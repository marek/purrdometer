// The Waveshare 2.13" V4 e-paper panel (SSD1680) over SPI: full and partial
// refreshes of panel RAM, and deep sleep.
#pragma once

#include "display/gpio.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace purr::display {

// The panel's RAM: portrait, rows of PANEL_ROW_BYTES, MSB first, 1 = white.
constexpr int PANEL_W = 122, PANEL_H = 250;
constexpr int PANEL_ROW_BYTES = (PANEL_W + 7) / 8;

class EPaper {
public:
    EPaper(Gpio& gpio, const std::string& spidev = "/dev/spidev0.0");
    ~EPaper();
    EPaper(const EPaper&) = delete;
    EPaper& operator=(const EPaper&) = delete;

    void full(const std::vector<uint8_t>& ram);    // flashes; clears ghosting; sets the partial base
    void partial(const std::vector<uint8_t>& ram); // fast; falls back to full() after sleep
    void sleep();
    bool asleep() const { return asleep_; }

private:
    void command(uint8_t c, std::initializer_list<uint8_t> data = {});
    void data(const uint8_t* bytes, size_t n);
    void waitIdle();
    void reset(std::chrono::milliseconds hold);
    void window();

    Gpio& gpio_;
    int fd_ = -1;
    bool asleep_ = true;
};

} // namespace purr::display
