#include "display/refresher.h"

#include "display/orientation.h"

#include <print>

namespace purr::display {

void Refresher::configure(bool rotate180, int fullEvery)
{
    if (rotate180 != rotate180_)
        invalidate();
    rotate180_ = rotate180;
    fullEvery_ = fullEvery;
}

void Refresher::invalidate()
{
    last_.reset();
    partials_ = fullEvery_;
}

void Refresher::show(const Canvas& frame, bool forceFull)
{
    if (last_ && *last_ == frame && !forceFull)
        return;
    auto now = Clock::now();
    auto ram = packPanel(frame, rotate180_);
    bool full = forceFull || epaper_.asleep() || partials_ >= fullEvery_ || now - lastFull_ > FULL_AT_LEAST;
    if (full) {
        epaper_.full(ram);
        partials_ = 0;
        lastFull_ = now;
    } else {
        epaper_.partial(ram);
        ++partials_;
    }
    if (verbose_)
        std::println("{} refresh took {} ms", full ? "full" : "partial",
                     std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - now).count());
    last_ = frame;
    lastFrame_ = now;
}

void Refresher::sleepIfIdle()
{
    if (Clock::now() - lastFrame_ > SLEEP_AFTER)
        epaper_.sleep();
}

} // namespace purr::display
