// purrdometer-display: the logger's history on a Waveshare 2.13" Touch
// e-Paper HAT, with a touch bar of four tabs, and setup mode's screens.
//
// Usage: purrdometer-display [--config PATH] [--overrides PATH] [--profiles PATH] [--wheels PATH] [--db PATH]
//                            [--demo-db PATH] [--preview DIR] [--now UNIX_TIME]
//                            [--list-wheels] [--check-config] [--selftest] [-v]

#include "common/demo.h"
#include "common/live_config.h"
#include "display/app.h"
#include "display/boot_window.h"
#include "display/epaper.h"
#include "display/options.h"
#include "display/preview.h"
#include "display/touch.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <print>

using namespace purr;
using namespace purr::display;

int selftest(const Options& o);

namespace {

std::atomic<bool> stopping{false};

int configCommands(const Options& o, const WheelPresets& presets)
{
    Config cfg = loadConfig(presets, o.config, o.overrides, o.profiles);
    if (o.listWheels)
        for (auto& w : presets.wheels)
            std::println("{:12} {}", w.key, describe(w, cfg.editable.units));
    if (!o.checkConfig)
        return 0;
    const Profile* inUse = cfg.profiles.find(cfg.profiles.current);
    std::println("{}: wheel in use: {} ({}), {}", o.config, inUse ? inUse->name : "?",
                 cfg.profilesSeeded ? "from [wheel], no profiles.conf yet" : o.profiles,
                 describe(cfg.wheel, cfg.editable.units));
    for (auto& e : cfg.errors)
        std::println(stderr, "error: {}", e);
    return cfg.errors.empty() ? 0 : 1;
}

int runScreen(const Options& o, const WheelPresets& presets)
{
    std::signal(SIGTERM, [](int) { stopping = true; });
    std::signal(SIGINT, [](int) { stopping = true; });
    LiveConfig config(presets, o.config, o.overrides, o.profiles);
    Fonts fonts;
    Gpio gpio;
    EPaper epaper(gpio);
    Touch touch(gpio);
    std::println("touch controller {}; {}", touch.productId(), describe(config.get().wheel, config.get().editable.units));
    TouchInput input(gpio, touch, o.verbose);
    Refresher screen(epaper, o.verbose);
    input.setRotated(config.get().editable.rotate180);
    screen.configure(config.get().editable.rotate180, config.get().editable.fullRefreshEvery);
    BootWindow(fonts, input, screen, o.setupRequest).run(stopping);
    App(config, fonts, input, screen, {o.db, o.setupState, o.setupRequest}).run(stopping);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0); // one journal line per message, not per 4 KB
    auto parsed = parseOptions(argc, argv);
    if (!parsed)
        return 2;
    Options o = *parsed;
    try {
        if (o.selftest)
            return selftest(o);
        WheelPresets presets = loadPresets(o.wheels);
        if (o.listWheels || o.checkConfig)
            return configCommands(o, presets);
        if (o.demoDb) {
            makeDemoDb(*o.demoDb);
            o.db = *o.demoDb;
            if (!o.preview)
                return 0; // just the database, for make serve
        }
        if (o.preview)
            return writePreviews(loadConfig(presets, o.config, o.overrides, o.profiles), o.db, *o.preview,
                                 o.now.value_or(std::time(nullptr)));
        return runScreen(o, presets);
    } catch (const std::exception& e) {
        std::println(stderr, "fatal: {}", e.what());
        return 1;
    }
}
