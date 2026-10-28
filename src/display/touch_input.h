// The touch screen as the app sees it: waits for the controller's interrupt
// or a timeout, turns its coordinates into the screen's, and tells taps from
// holds (Hold).
#pragma once

#include "display/gpio.h"
#include "display/hold.h"
#include "display/touch.h"

#include <chrono>
#include <optional>
#include <utility>

namespace purr::display {

class TouchInput {
public:
    static constexpr auto POLL_WHILE_DOWN = std::chrono::milliseconds(50); // to see the finger lift
    static constexpr auto POLL_FALLBACK = std::chrono::seconds(1);          // in case an edge was missed

    TouchInput(Gpio& gpio, Touch& touch, bool verbose) : gpio_(gpio), touch_(touch), verbose_(verbose) {}

    void setRotated(bool rotate180) { rotate180_ = rotate180; }

    // Waits up to `wait` (less while a finger is down); a tap's screen
    // position once its finger lifts.
    std::optional<std::pair<int, int>> wait(Hold::Clock::duration wait);

    Hold& hold() { return hold_; }

private:
    Gpio& gpio_;
    Touch& touch_;
    bool verbose_;
    bool rotate180_ = false;
    Hold hold_;
};

} // namespace purr::display
