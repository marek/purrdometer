// The display's loop: setup mode's screens while setup runs; otherwise the
// tab the touch bar chose, a long press on the bar filling towards setup,
// and a redraw as often as the wheel's state calls for.
#pragma once

#include "common/live_config.h"
#include "display/fonts.h"
#include "display/refresher.h"
#include "display/touch_input.h"

#include <atomic>
#include <chrono>
#include <string>

namespace purr::display {

struct AppPaths {
    std::string db, setupState, setupRequest;
};

class App {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto SETUP_CHECK = std::chrono::seconds(1); // how often setup mode's state is read

    App(LiveConfig& config, Fonts& fonts, TouchInput& touch, Refresher& screen, AppPaths paths);

    void run(const std::atomic<bool>& stopping);

private:
    bool showSetup();                       // setup mode's screen, if setup is running
    void handleTap(std::pair<int, int> at);
    bool followHold();                      // a long press on the bar; true while it is shown
    void drawStats();
    void applyConfig();

    LiveConfig& config_;
    Fonts& fonts_;
    TouchInput& touch_;
    Refresher& screen_;
    AppPaths paths_;
    int view_ = 0;
    bool holdShown_ = false;
    Clock::time_point nextDraw_{};
};

} // namespace purr::display
