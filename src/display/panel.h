// The Waveshare 2.13" Touch e-Paper HAT: the V4 panel (SSD1680) over SPI and
// the GT1151 touch controller over I2C, with their pins through libgpiod.
//
// The panel's RAM is 122x250 portrait; screens are drawn 250x122 landscape.
// packPanel() turns a landscape canvas into panel RAM, and touchToScreen()
// turns the touch controller's panel coordinates into landscape ones; the
// touch strip is along the bottom of the landscape screen, on the same edge
// as the button row of Waveshare's demo. rotate180 flips both.
#pragma once

#include "display/canvas.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct gpiod_chip;
struct gpiod_line_request;
struct gpiod_edge_event_buffer;

namespace purr::display {

constexpr int PANEL_W = 122, PANEL_H = 250;
constexpr int PANEL_ROW_BYTES = (PANEL_W + 7) / 8;

// Landscape canvas -> panel RAM bytes: rows of PANEL_ROW_BYTES, MSB first, 1 = white.
std::vector<uint8_t> packPanel(const Canvas& landscape, bool rotate180);
// Touch controller (tx, ty) -> landscape (x, y).
std::pair<int, int> touchToScreen(int tx, int ty, bool rotate180);

// BCM pin numbers on the HAT.
struct Pins {
    static constexpr unsigned RST = 17, DC = 25, BUSY = 24, TOUCH_RST = 22, TOUCH_INT = 27;
};

class Gpio {
public:
    Gpio(); // the chip labelled pinctrl-bcm2835
    ~Gpio();
    Gpio(const Gpio&) = delete;
    Gpio& operator=(const Gpio&) = delete;

    void set(unsigned pin, bool high);
    bool get(unsigned pin);
    int touchFd();                 // readable when TOUCH_INT falls
    void drainTouchEvents();

private:
    gpiod_chip* chip_ = nullptr;
    gpiod_line_request* lines_ = nullptr;
    gpiod_edge_event_buffer* events_ = nullptr;
};

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

class Touch {
public:
    Touch(Gpio& gpio, const std::string& i2cdev = "/dev/i2c-1");
    ~Touch();
    Touch(const Touch&) = delete;
    Touch& operator=(const Touch&) = delete;

    std::string productId();
    // Reads the controller; returns a tap's panel coordinates on a new touch.
    std::optional<std::pair<int, int>> poll();
    bool fingerDown() const { return down_; }

private:
    std::vector<uint8_t> read(uint16_t reg, size_t n);
    void write(uint16_t reg, uint8_t value);

    Gpio& gpio_;
    int fd_ = -1;
    bool down_ = false;
    std::chrono::steady_clock::time_point lastReport_{};
};

} // namespace purr::display
