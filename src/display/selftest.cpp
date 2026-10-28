// purrdometer-display --selftest: drawing, the panel's byte layout, touch
// mapping, taps and holds, and every screen, with no hardware.

#include "common/calendar.h"
#include "common/demo.h"
#include "common/setup_state.h"
#include "common/stats.h"
#include "common/wheel.h"
#include "display/canvas.h"
#include "display/hold.h"
#include "display/options.h"
#include "display/orientation.h"
#include "display/png.h"
#include "display/screens/layout.h"
#include "display/screens/setup_screens.h"
#include "display/screens/stats_screen.h"
#include "display/screens/touch_bar.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <format>
#include <print>
#include <string>

using namespace purr;
using namespace purr::display;

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
    const std::map<int, double> g5Only{{FIRST_PROFILE, g5.circumferenceM()}};
    Stats demo = loadStats(db, cal::localDay(now), g5Only), none = loadStats(tmp + "/missing.db", cal::localDay(now), g5Only);
    check(demo.total > 0 && demo.live.running(double(now)), "demo history is running now");
    for (int v = 0; v < int(VIEWS.size()); ++v) {
        Canvas full = render(fonts, demo, g5, v, now), blank = render(fonts, none, g5, v, now);
        check(full.width() == W && full.height() == H && !(full == blank), std::format("{} renders", VIEWS[v]));
    }
    using HC = Hold::Clock;
    Hold h;
    auto t0 = HC::now();
    h.down(t0, 10, 115);
    check(!h.progress(t0 + std::chrono::milliseconds(500)) && h.up(t0 + std::chrono::milliseconds(500)) == std::pair{10, 115},
          "a short press is a tap, on release");
    h.down(t0, 10, 115);
    auto mid = h.progress(t0 + Hold::TAP_MAX + std::chrono::milliseconds(1500));
    check(mid && *mid > 0.45 && *mid < 0.55 && !h.fire(t0 + std::chrono::seconds(2)), "a hold fills");
    check(h.fire(t0 + Hold::TAP_MAX + Hold::HOLD_FILL) && !h.fire(t0 + std::chrono::seconds(6)), "a full hold fires once");
    check(!h.up(t0 + std::chrono::seconds(6)), "letting go of a hold is not a tap");
    Canvas held = render(fonts, demo, g5, 0, now, 0.5), plain = render(fonts, demo, g5, 0, now);
    check(!(held == plain), "the touch bar shows a hold");
    setup::State st;
    st.phase = setup::Phase::Hotspot;
    st.ssid = "purrdometer-372a";
    st.password = "abcdefghjk";
    st.deadline = now + 540;
    Canvas stats0 = render(fonts, demo, g5, 0, now), setupScreen = renderSetup(fonts, st, now);
    int qrInk = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < 100; ++x)
            qrInk += setupScreen.black(x, y);
    check(qrInk > 1500 && !(setupScreen == stats0), "the setup screen and its QR code");
    for (auto phase : {setup::Phase::Starting, setup::Phase::Joining, setup::Phase::Joined}) {
        st.phase = phase;
        st.network = "Home WiFi";
        st.message = "192.168.1.196";
        check(!(renderSetup(fonts, st, now) == Canvas(W, H)), std::format("setup screen: {}", setup::phaseName(phase)));
    }
    st.phase = setup::Phase::Joined;
    Canvas joined = renderSetup(fonts, st, now);
    st.warning = "login password, SSH key";
    Canvas warned = renderSetup(fonts, st, now);
    setup::State stuck{.phase = setup::Phase::Starting, .message = "could not join Cafe: wrong password?; the hotspot did not start: busy; trying again"};
    Canvas stuckScreen = renderSetup(fonts, stuck, now);
    check(!(stuckScreen == renderStarting(fonts)) && !(stuckScreen == Canvas(W, H)),
          "a hotspot that would not start shows why on the starting screen");
    writePng(stuckScreen, tmp + "/starting-why.png");
    check(!(warned == joined), "a joined screen says what of the form was not applied");
    writePng(warned, tmp + "/joined-warning.png");
    check(!(renderBoot(fonts, 0.3) == renderBoot(fonts, std::nullopt)), "the boot screen shows a hold");
    writePng(renderSetup(fonts, setup::State{.phase = setup::Phase::Hotspot, .ssid = "purrdometer-372a", .password = "abcdefghjk", .deadline = now + 540}, now),
             tmp + "/setup-screen.png");
    writePng(renderBoot(fonts, 0.5), tmp + "/boot-screen.png");
    writePng(held, tmp + "/hold-bar.png");

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
