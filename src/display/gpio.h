// The HAT's pins through libgpiod: reset, data/command, busy, and the touch
// controller's reset and interrupt.
#pragma once

struct gpiod_chip;
struct gpiod_line_request;
struct gpiod_edge_event_buffer;

namespace purr::display {

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

} // namespace purr::display
