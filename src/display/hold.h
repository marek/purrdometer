// Taps and holds from touch downs and ups (docs/intents/network/setup.md).
// A press shorter than TAP_MAX is a tap, acted on when the finger lifts; a
// longer one is a hold, which fills over HOLD_FILL and then fires once.
#pragma once

#include <chrono>
#include <optional>
#include <utility>

namespace purr::display {

class Hold {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto TAP_MAX = std::chrono::milliseconds(700);
    static constexpr auto HOLD_FILL = std::chrono::seconds(3);

    void down(Clock::time_point t, int x, int y)
    {
        downAt_ = t;
        x_ = x;
        y_ = y;
        fired_ = false;
    }

    // A tap's position when the finger lifts soon enough; a hold just ends.
    std::optional<std::pair<int, int>> up(Clock::time_point t)
    {
        if (!downAt_)
            return std::nullopt;
        bool tap = t - *downAt_ < TAP_MAX;
        downAt_.reset();
        return tap ? std::optional(std::pair{x_, y_}) : std::nullopt;
    }

    // Nothing while no finger is down or it is still a tap; then 0 rising to 1.
    std::optional<double> progress(Clock::time_point t) const
    {
        if (!downAt_ || t - *downAt_ < TAP_MAX)
            return std::nullopt;
        double p = std::chrono::duration<double>(t - *downAt_ - TAP_MAX) / std::chrono::duration<double>(HOLD_FILL);
        return p > 1 ? 1.0 : p;
    }

    // True once per hold, when it is full.
    bool fire(Clock::time_point t)
    {
        auto p = progress(t);
        if (!p || *p < 1 || fired_)
            return false;
        fired_ = true;
        return true;
    }

    bool down() const { return downAt_.has_value(); }
    std::pair<int, int> where() const { return {x_, y_}; }

private:
    std::optional<Clock::time_point> downAt_;
    int x_ = 0, y_ = 0;
    bool fired_ = false;
};

} // namespace purr::display
