// The first seconds after start (docs/intents/network/setup.md#triggers):
// "hold anywhere to set up WiFi". A hold under way keeps the window open until
// it fills or lets go.
#pragma once

#include "display/refresher.h"
#include "display/touch_input.h"

#include <atomic>
#include <chrono>
#include <string>

namespace purr::display {

class BootWindow {
public:
    static constexpr auto LENGTH = std::chrono::seconds(10);

    BootWindow(Fonts& fonts, TouchInput& touch, Refresher& screen, std::string setupRequest)
        : fonts_(fonts), touch_(touch), screen_(screen), setupRequest_(std::move(setupRequest))
    {
    }

    void run(const std::atomic<bool>& stopping);

private:
    Fonts& fonts_;
    TouchInput& touch_;
    Refresher& screen_;
    std::string setupRequest_;
};

} // namespace purr::display
