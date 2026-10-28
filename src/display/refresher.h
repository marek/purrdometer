// When a frame goes to the e-paper and how (docs/intents/display/refresh.md):
// only when it changed; partial refreshes, a full one every N partials and at
// least hourly; deep sleep after a quiet minute.
#pragma once

#include "display/canvas.h"
#include "display/epaper.h"

#include <chrono>
#include <optional>

namespace purr::display {

class Refresher {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto FULL_AT_LEAST = std::chrono::hours(1);
    static constexpr auto SLEEP_AFTER = std::chrono::seconds(60);

    Refresher(EPaper& epaper, bool verbose) : epaper_(epaper), verbose_(verbose) {}

    void configure(bool rotate180, int fullEvery);
    void show(const Canvas& frame, bool forceFull = false);
    void invalidate();      // the next frame is sent, as a full refresh
    void sleepIfIdle();     // after SLEEP_AFTER without a frame

private:
    EPaper& epaper_;
    bool verbose_;
    bool rotate180_ = false;
    int fullEvery_ = 20;
    std::optional<Canvas> last_;
    int partials_ = 0;
    Clock::time_point lastFull_{}, lastFrame_{};
};

} // namespace purr::display
