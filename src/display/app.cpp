#include "display/app.h"

#include "common/calendar.h"
#include "common/setup_state.h"
#include "common/stats.h"
#include "display/screens/layout.h"
#include "display/screens/setup_screens.h"
#include "display/screens/stats_screen.h"
#include "display/screens/touch_bar.h"

#include <ctime>
#include <print>

namespace purr::display {

App::App(LiveConfig& config, Fonts& fonts, TouchInput& touch, Refresher& screen, AppPaths paths)
    : config_(config), fonts_(fonts), touch_(touch), screen_(screen), paths_(std::move(paths))
{
    applyConfig();
}

void App::run(const std::atomic<bool>& stopping)
{
    nextDraw_ = Clock::now();
    while (!stopping) {
        auto tap = touch_.wait(std::max(Clock::duration::zero(), nextDraw_ - Clock::now()));
        if (showSetup())
            continue;
        if (tap)
            handleTap(*tap);
        if (followHold())
            continue;
        if (Clock::now() < nextDraw_)
            continue;
        if (config_.reload())
            applyConfig();
        drawStats();
    }
}

bool App::showSetup()
{
    setup::State state = setup::readState(paths_.setupState);
    if (!state.active())
        return false;
    screen_.show(renderSetup(fonts_, state, std::time(nullptr)));
    nextDraw_ = Clock::now() + SETUP_CHECK;
    return true;
}

void App::handleTap(std::pair<int, int> at)
{
    if (auto tab = tabAt(at.first, at.second); tab && *tab != view_) {
        view_ = *tab;
        nextDraw_ = Clock::now();
    }
}

bool App::followHold()
{
    Hold& hold = touch_.hold();
    auto now = Clock::now();
    if (hold.down() && hold.where().second >= BAR_Y - TAP_SLACK) {
        if (hold.fire(now)) {
            setup::requestSetup(paths_.setupRequest);
            std::println("setup requested from the touch bar");
            screen_.show(renderStarting(fonts_));
            return true;
        }
        if (auto p = hold.progress(now)) {
            std::time_t t = std::time(nullptr);
            screen_.show(render(fonts_, loadStats(paths_.db, cal::localDay(t), circumferences(config_.get())),
                                config_.get().wheel, view_, t, *p));
            holdShown_ = true;
            return true;
        }
    }
    if (holdShown_ && !hold.down()) {
        holdShown_ = false;
        nextDraw_ = now; // the tabs come back
    }
    return false;
}

void App::drawStats()
{
    const Config& cfg = config_.get();
    std::time_t t = std::time(nullptr);
    Stats stats = loadStats(paths_.db, cal::localDay(t), circumferences(cfg));
    screen_.show(render(fonts_, stats, cfg.wheel, view_, t));
    bool running = stats.live.running(double(t));
    nextDraw_ = Clock::now() + std::chrono::seconds(running ? cfg.editable.activeRefreshS : cfg.editable.idleRefreshS);
    if (!running)
        screen_.sleepIfIdle();
}

void App::applyConfig()
{
    const Editable& e = config_.get().editable;
    touch_.setRotated(e.rotate180);
    screen_.configure(e.rotate180, e.fullRefreshEvery);
    screen_.invalidate(); // the wheel may have changed what every screen shows
}

} // namespace purr::display
