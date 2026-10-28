#include "display/touch_input.h"

#include "display/orientation.h"

#include <algorithm>
#include <poll.h>
#include <print>

namespace purr::display {

std::optional<std::pair<int, int>> TouchInput::wait(Hold::Clock::duration wait)
{
    wait = std::min<Hold::Clock::duration>(wait, hold_.down() ? POLL_WHILE_DOWN : POLL_FALLBACK);
    pollfd pfd{gpio_.touchFd(), POLLIN, 0};
    if (::poll(&pfd, 1, int(std::chrono::duration_cast<std::chrono::milliseconds>(wait).count())) > 0)
        gpio_.drainTouchEvents();
    auto event = touch_.poll();
    if (!event)
        return std::nullopt;
    auto [x, y] = touchToScreen(event->x, event->y, rotate180_);
    bool down = event->kind == TouchEvent::Kind::Down;
    if (verbose_)
        std::println("touch {} at screen ({},{})", down ? "down" : "up", x, y);
    auto now = Hold::Clock::now();
    if (down) {
        hold_.down(now, x, y);
        return std::nullopt;
    }
    return hold_.up(now);
}

} // namespace purr::display
