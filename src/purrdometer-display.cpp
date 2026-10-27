// purrdometer-display — the e-paper screen: the logger's history on a Waveshare
// 2.13" Touch e-Paper HAT, with a touch bar of four tabs.
//
// Redraw policy (docs/intents/display/refresh.md): a frame is sent only when it
// changed; partial refreshes, a full one every N partials and at least hourly;
// every few seconds while the wheel turns; deep sleep after a quiet minute.
// The touch controller's interrupt wakes the loop through poll().
//
// Usage: purrdometer-display [--config PATH] [--overrides PATH] [--wheels PATH] [--db PATH]
//                            [--demo-db PATH] [--preview DIR] [--now UNIX_TIME]
//                            [--list-wheels] [--check-config] [--selftest] [-v]

#include "common/config.h"
#include "common/demo.h"
#include "common/stats.h"
#include "common/wheel.h"
#include "display/canvas.h"
#include "display/panel.h"
#include "display/png.h"
#include "display/screens.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <format>
#include <iostream>
#include <poll.h>
#include <print>
#include <string>
#include <sys/stat.h>

namespace {

using namespace purr;
using namespace purr::display;
using Clock = std::chrono::steady_clock;

constexpr auto FULL_REFRESH_AT_LEAST = std::chrono::hours(1);
constexpr auto SLEEP_AFTER = std::chrono::seconds(60);
constexpr auto TOUCH_POLL_WHILE_DOWN = std::chrono::milliseconds(50); // to see the finger lift
constexpr auto TOUCH_POLL_FALLBACK = std::chrono::seconds(1);          // in case an edge was missed

std::atomic<bool> stopping{false};

struct Options {
    std::string config = CONFIG_PATH, overrides = OVERRIDES_PATH, wheels = WHEELS_PATH;
    std::string db = "/var/lib/purrdometer/purrdometer.db";
    std::optional<std::string> demoDb, preview;
    std::optional<std::time_t> now;
    bool listWheels = false, checkConfig = false, selftest = false, verbose = false;
};

std::time_t nowOr(const Options& o) { return o.now.value_or(std::time(nullptr)); }

long long mtime(const std::string& path)
{
    struct stat st{};
    return stat(path.c_str(), &st) == 0 ? (long long)st.st_mtim.tv_sec * 1'000'000'000 + st.st_mtim.tv_nsec : -1;
}

std::string slug(const char* view) // "30 DAYS" -> "30days"
{
    std::string s;
    for (const char* p = view; *p; ++p)
        if (*p != ' ')
            s += char(std::tolower(static_cast<unsigned char>(*p)));
    return s;
}

int preview(const Options& o, const WheelPresets& presets)
{
    Config cfg = loadConfig(presets, o.config, o.overrides);
    Fonts fonts;
    std::time_t now = nowOr(o);
    Stats stats = loadStats(o.db, cal::localDay(now));
    std::filesystem::create_directories(*o.preview);
    for (int i = 0; i < int(VIEWS.size()); ++i) {
        std::string path = std::format("{}/{}-{}.png", *o.preview, i, slug(VIEWS[i]));
        writePng(render(fonts, stats, cfg.wheel, i, now), path);
        std::println("{}", path);
    }
    return 0;
}

class App {
public:
    App(const Options& o, const WheelPresets& presets)
        : o_(o), presets_(presets), cfg_(loadConfig(presets, o.config, o.overrides)), epaper_(gpio_), touch_(gpio_)
    {
        stamp_ = configStamp();
        std::println("touch controller {}; {}", touch_.productId(), describe(cfg_.wheel, cfg_.editable.units));
    }

    void run()
    {
        Canvas frame = draw();
        show(frame, true);
        auto nextData = Clock::now();
        while (!stopping) {
            auto now = Clock::now();
            auto wait = std::max(Clock::duration::zero(), nextData - now);
            wait = std::min<Clock::duration>(wait, touch_.fingerDown() ? TOUCH_POLL_WHILE_DOWN : TOUCH_POLL_FALLBACK);
            pollfd pfd{gpio_.touchFd(), POLLIN, 0};
            int ready = ::poll(&pfd, 1, int(std::chrono::duration_cast<std::chrono::milliseconds>(wait).count()));
            if (ready > 0)
                gpio_.drainTouchEvents();
            if (auto tap = touch_.poll()) {
                auto [x, y] = touchToScreen(tap->first, tap->second, cfg_.editable.rotate180);
                if (o_.verbose)
                    std::println("tap panel=({},{}) screen=({},{})", tap->first, tap->second, x, y);
                if (auto tab = tabAt(x, y); tab && *tab != view_) {
                    view_ = *tab;
                    nextData = Clock::now(); // redraw now
                }
            }
            if (Clock::now() < nextData)
                continue;

            reloadIfChanged();
            std::time_t t = std::time(nullptr);
            Stats stats = loadStats(o_.db, cal::localDay(t));
            show(render(fonts_, stats, cfg_.wheel, view_, t));
            bool running = stats.live.running(double(t));
            nextData = Clock::now() + std::chrono::seconds(running ? cfg_.editable.activeRefreshS : cfg_.editable.idleRefreshS);
            if (!running && Clock::now() - lastUpdate_ > SLEEP_AFTER)
                epaper_.sleep();
        }
    }

private:
    Canvas draw()
    {
        std::time_t t = std::time(nullptr);
        return render(fonts_, loadStats(o_.db, cal::localDay(t)), cfg_.wheel, view_, t);
    }

    void show(const Canvas& frame, bool forceFull = false)
    {
        if (last_ && *last_ == frame && !forceFull)
            return;
        auto now = Clock::now();
        auto ram = packPanel(frame, cfg_.editable.rotate180);
        bool full = forceFull || epaper_.asleep() || partials_ >= cfg_.editable.fullRefreshEvery ||
                    now - lastFull_ > FULL_REFRESH_AT_LEAST;
        if (full) {
            epaper_.full(ram);
            partials_ = 0;
            lastFull_ = now;
        } else {
            epaper_.partial(ram);
            ++partials_;
        }
        if (o_.verbose)
            std::println("{} refresh of {} took {} ms", full ? "full" : "partial", VIEWS[view_],
                         std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - now).count());
        last_ = frame;
        lastUpdate_ = now;
    }

    std::pair<long long, long long> configStamp() const { return {mtime(o_.config), mtime(o_.overrides)}; }

    // A change saved from the web UI, or a hand edit, applies without a restart.
    void reloadIfChanged()
    {
        auto stamp = configStamp();
        if (stamp == stamp_)
            return;
        stamp_ = stamp;
        cfg_ = loadConfig(presets_, o_.config, o_.overrides);
        last_.reset();                            // the wheel or the orientation may have changed
        partials_ = cfg_.editable.fullRefreshEvery; // so the next frame is a full refresh
        std::println("config reloaded: {}", describe(cfg_.wheel, cfg_.editable.units));
        for (auto& e : cfg_.errors)
            std::println("config error: {}", e);
    }

    const Options& o_;
    const WheelPresets& presets_;
    Config cfg_;
    Fonts fonts_;
    Gpio gpio_;
    EPaper epaper_;
    Touch touch_;
    int view_ = 0;
    std::optional<Canvas> last_;
    int partials_ = 0;
    Clock::time_point lastFull_{}, lastUpdate_{};
    std::pair<long long, long long> stamp_;
};

int selftest(const Options& o)
{
    int failures = 0;
    auto check = [&](bool ok, const std::string& what) {
        std::println("{} {}", ok ? "ok  " : "FAIL", what);
        failures += !ok;
    };
    const char* tmpEnv = std::getenv("PURR_TMP");
    std::string tmp = tmpEnv ? tmpEnv : "tmp";
    std::filesystem::create_directories(tmp);

    check(decodeUtf8("a·▶") == std::vector<char32_t>{U'a', U'\u00B7', U'\u25B6'}, "UTF-8 decoding");
    check(decodeUtf8("\xff") == std::vector<char32_t>{U'\uFFFD'}, "invalid UTF-8 becomes U+FFFD");

    Canvas c(W, H);
    c.fill(0, 0, 0, 0);
    auto ram = packPanel(c, false);
    check(ram.size() == size_t(PANEL_ROW_BYTES) * PANEL_H, "panel RAM size");
    // Landscape (0,0) is panel (121,0): the last pixel of row 0, bit 6 of byte 15.
    check(ram[15] == uint8_t(~0x40), "landscape top-left lands on panel (121, 0)");
    check(packPanel(c, true)[size_t(PANEL_H - 1) * PANEL_ROW_BYTES] == 0x7F, "flipped: on panel (0, 249)");
    check(touchToScreen(0, 0, false) == std::pair{249, 0} && touchToScreen(0, 0, true) == std::pair{0, 121},
          "touch mapping");
    check(!tabAt(10, 40) && tabAt(0, 115) == 0 && tabAt(249, 115) == 3 && tabAt(130, 121) == 2, "touch bar tabs");

    Fonts fonts;
    Font f = fonts.get(9, true);
    check(Canvas::textWidth("NOW", f) > 10 && Canvas::textWidth("NOW", fonts.get(18, true)) > Canvas::textWidth("NOW", f),
          "text widths grow with size");
    Canvas t(40, 20);
    t.text(0, 0, "M", fonts.get(14, true));
    int inked = 0;
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 40; ++x)
            inked += t.black(x, y);
    check(inked > 20, "glyphs are drawn");

    WheelPresets presets = loadPresets(o.wheels);
    Wheel g5 = *presets.find("ferris-g5");
    std::string db = tmp + "/display-selftest.db";
    makeDemoDb(db);
    std::time_t now = std::time(nullptr);
    Stats demo = loadStats(db, cal::localDay(now)), none = loadStats(tmp + "/missing.db", cal::localDay(now));
    check(demo.total > 0 && demo.live.running(double(now)), "demo history is running now");
    for (int v = 0; v < int(VIEWS.size()); ++v) {
        Canvas full = render(fonts, demo, g5, v, now), blank = render(fonts, none, g5, v, now);
        check(full.width() == W && full.height() == H && !(full == blank), std::format("{} renders", VIEWS[v]));
    }
    std::string png = tmp + "/display-selftest.png";
    writePng(render(fonts, demo, g5, 0, now), png);
    std::FILE* fp = std::fopen(png.c_str(), "rb");
    unsigned char sig[8] = {};
    bool isPng = fp && std::fread(sig, 1, 8, fp) == 8 && sig[1] == 'P' && sig[2] == 'N' && sig[3] == 'G';
    if (fp)
        std::fclose(fp);
    check(isPng, "PNG output");
    std::remove(db.c_str());
    std::remove(png.c_str());

    std::println("{}", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}

void usage()
{
    std::println(stderr, "usage: purrdometer-display [--config PATH] [--overrides PATH] [--wheels PATH] [--db PATH]\n"
                         "       [--demo-db PATH] [--preview DIR] [--now UNIX_TIME] [--list-wheels] [--check-config]\n"
                         "       [--selftest] [-v]");
}

} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0); // one journal line per message, not per 4 KB
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                usage();
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--config") o.config = next();
        else if (a == "--overrides") o.overrides = next();
        else if (a == "--wheels") o.wheels = next();
        else if (a == "--db") o.db = next();
        else if (a == "--demo-db") o.demoDb = next();
        else if (a == "--preview") o.preview = next();
        else if (a == "--now") o.now = std::stoll(next());
        else if (a == "--list-wheels") o.listWheels = true;
        else if (a == "--check-config") o.checkConfig = true;
        else if (a == "--selftest") o.selftest = true;
        else if (a == "-v" || a == "--verbose") o.verbose = true;
        else {
            usage();
            return 2;
        }
    }
    try {
        if (o.selftest)
            return selftest(o);
        WheelPresets presets = loadPresets(o.wheels);
        if (o.listWheels || o.checkConfig) {
            Config cfg = loadConfig(presets, o.config, o.overrides);
            if (o.listWheels)
                for (auto& w : presets.wheels)
                    std::println("{:12} {}", w.key, describe(w, cfg.editable.units));
            if (o.checkConfig) {
                std::println("{}: {}", o.config, describe(cfg.wheel, cfg.editable.units));
                for (auto& e : cfg.errors)
                    std::println(stderr, "error: {}", e);
                return cfg.errors.empty() ? 0 : 1;
            }
            return 0;
        }
        if (o.demoDb) {
            makeDemoDb(*o.demoDb);
            o.db = *o.demoDb;
            if (!o.preview)
                return 0; // just the database, for make serve
        }
        if (o.preview)
            return preview(o, presets);

        std::signal(SIGTERM, [](int) { stopping = true; });
        std::signal(SIGINT, [](int) { stopping = true; });
        App(o, presets).run();
        return 0;
    } catch (const std::exception& e) {
        std::println(stderr, "fatal: {}", e.what());
        return 1;
    }
}
