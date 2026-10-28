// Diameters, wheel presets, calendar days, number formats and the access rules.

#include "common/calendar.h"
#include "common/format.h"
#include "common/wheel.h"
#include "web/access.h"
#include "web/html.h"
#include "web/selftest.h"

#include <string>

using namespace purr;

void selftestWheels(Check& check, const WheelPresets& presets)
{
    auto throws = [](auto&& f) {
        try {
            f();
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };

    for (const char* t : {"48in", "48\"", "48 inches", "121.92cm", "1219.2 mm", "1.2192m"})
        check(near(parseDiameter(t), 1.2192), std::string("diameter ") + t);
    for (const char* t : {"48ft", "5in", "", "abc", "3m"})
        check(throws([&] { parseDiameter(t); }), std::string("rejects diameter '") + t + "'");
    check(near(parseDiameter("123"), 1.23), "a bare number is cm by default");
    check(near(parseDiameter("48.5", "in"), 48.5 * INCH), "a bare number in inches when units = in");
    check(near(parseDiameter("48.5in", "cm"), 48.5 * INCH), "an explicit unit wins");
    auto hint = [](const std::string& t, const std::string& u) {
        try {
            parseDiameter(t, u);
        } catch (const std::invalid_argument& e) {
            return std::string(e.what());
        }
        return std::string();
    };
    check(hint("48.5", "cm").find("did you mean 48.5in?") != std::string::npos, "hints at inches");
    check(hint("123", "in").find("did you mean 123cm?") != std::string::npos, "hints at centimetres");
    check(throws([&] { parseLength("48"); }), "no unit and none chosen");

    check(presets.find("ferris-g5") && near(presets.find("ferris-g5")->circumferenceM(), 3.8303),
          "Ferris G5 runs on its 48\" inner diameter");
    check(presets.find("grand-ferris") && near(presets.find("grand-ferris")->diameterM, 60 * INCH), "Grand Ferris 60\"");
    check(presets.find("one-fast-cat") && near(presets.find("one-fast-cat")->diameterM, 43 * INCH), "One Fast Cat 43\"");
    check(presets.find("star-cat-xl") && presets.find("star-cat-xl")->estimated, "Star Cat inner diameters are estimates");
    Wheel measured = resolveWheel(presets, std::string("ferris-g5"), std::string("48.5in"));
    check(near(measured.diameterM, 48.5 * INCH) && measured.name.find("measured") != std::string::npos,
          "measured diameter overrides the model");
    check(resolveWheel(presets, std::nullopt, std::nullopt).key == DEFAULT_WHEEL, "default wheel");
    check(describe(*presets.find("ferris-g5")).find("running diameter 121.9 cm (48.0 in), outer 129.5 cm") !=
              std::string::npos, "sizes in cm by default");
    check(describe(*presets.find("ferris-g5"), "in").find("running diameter 48.0 in (121.9 cm), outer 51.0 in") !=
              std::string::npos, "sizes in inches when units = in");
    check(throws([&] { resolveWheel(presets, std::string("hamster-3000"), std::nullopt); }), "unknown model");

    check(cal::weekday(cal::fromIso("2026-10-26")) == 0, "2026-10-26 is a Monday");
    check(cal::iso(cal::fromIso("2024-02-29") + 1) == "2024-03-01", "leap day arithmetic");

    check(web::escape("<a href=\"x\">&'") == "&lt;a href=&quot;x&quot;&gt;&amp;&#39;", "HTML escaping");
    check(thousands(223867) == "223,867" && km(4.84) == "4.8" && km(857.4) == "857", "number formats");

    check(web::isLanAddress("192.168.1.20") && web::isLanAddress("10.0.0.5") && web::isLanAddress("172.20.1.1") &&
              web::isLanAddress("127.0.0.1") && web::isLanAddress("::1") && web::isLanAddress("fe80::1") &&
              web::isLanAddress("fd12::3") && web::isLanAddress("::ffff:192.168.1.20"),
          "home-network addresses");
    check(!web::isLanAddress("8.8.8.8") && !web::isLanAddress("172.32.0.1") && !web::isLanAddress("2001:db8::1") &&
              !web::isLanAddress("") && !web::isLanAddress("::ffff:8.8.8.8"),
          "internet addresses");
    check(web::sameSecret("abc", "abc") && !web::sameSecret("abc", "abd") && !web::sameSecret("", ""), "secret compare");
}
