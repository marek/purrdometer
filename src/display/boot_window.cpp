#include "display/boot_window.h"

#include "common/setup_state.h"
#include "display/screens/setup_screens.h"

#include <print>

namespace purr::display {

void BootWindow::run(const std::atomic<bool>& stopping)
{
    using Clock = Hold::Clock;
    Hold& hold = touch_.hold();
    screen_.show(renderBoot(fonts_, std::nullopt), true);
    auto end = Clock::now() + LENGTH;
    bool shown = false;
    while (!stopping && (Clock::now() < end || hold.down())) {
        touch_.wait(std::chrono::milliseconds(200));
        auto now = Clock::now();
        if (hold.fire(now)) {
            setup::requestSetup(setupRequest_);
            std::println("setup requested from the boot screen");
            screen_.show(renderStarting(fonts_));
            return;
        }
        if (auto p = hold.progress(now)) {
            screen_.show(renderBoot(fonts_, p));
            shown = true;
        } else if (shown && !hold.down()) {
            screen_.show(renderBoot(fonts_, std::nullopt));
            shown = false;
        }
    }
}

} // namespace purr::display
