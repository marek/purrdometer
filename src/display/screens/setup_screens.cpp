#include "display/screens/setup_screens.h"

#include "display/screens/kit.h"
#include "display/screens/qr.h"

#include <algorithm>
#include <format>

namespace purr::display {

namespace {

// The WiFi symbol: a dot and three arcs above it.
void drawWifi(Canvas& c, int cx, int cy, int r)
{
    c.fill(cx - 2, cy - 2, cx + 2, cy + 2);
    for (int i = 1; i <= 3; ++i)
        c.arc(cx, cy, r * i / 3, 45, 135, 3);
}

} // namespace

Canvas renderBoot(Fonts& fonts, std::optional<double> hold)
{
    Canvas c(W, H);
    drawWifi(c, 40, 74, 30);
    c.text(84, 22, "purrdometer", fonts.get(18, true));
    c.text(84, 48, "Hold anywhere to", fonts.get(11));
    c.text(84, 63, "set up WiFi", fonts.get(11, true));
    c.text(84, 84, "starting in a moment", fonts.get(9));
    if (hold)
        drawStrip(c, 84, 100, W - 8, *hold);
    return c;
}

Canvas renderStarting(Fonts& fonts)
{
    Canvas c(W, H);
    drawWifi(c, 40, 74, 30);
    c.text(84, 40, "Starting", fonts.get(14, true));
    c.text(84, 60, "WiFi setup…", fonts.get(14, true));
    return c;
}

Canvas renderSetup(Fonts& fonts, const setup::State& s, std::time_t now, int webPort)
{
    using setup::Phase;
    Canvas c(W, H);
    Font small = fonts.get(9), label = fonts.get(9, true);
    if (s.phase == Phase::Starting)
        return renderStarting(fonts);
    if (s.phase == Phase::Joining) {
        drawWifi(c, 40, 74, 30);
        c.text(84, 30, "Joining", fonts.get(14, true));
        std::string net = s.network;
        c.text(84, 52, net, fitFont(fonts, net, W - 90, 14));
        c.text(84, 80, "this takes up to a minute", small);
        return c;
    }
    if (s.phase == Phase::Joined) {
        drawWifi(c, 40, 74, 30);
        c.text(84, 14, "Connected to", small);
        c.text(84, 26, s.network, fitFont(fonts, s.network, W - 90, 14));
        c.text(84, 56, "Dashboard:", small);
        std::string url = "http://" + s.message + (webPort == 80 ? "" : std::format(":{}", webPort));
        c.text(84, 70, url, fitFont(fonts, url, W - 90, 12));
        return c;
    }

    // The hotspot: a QR code that joins it, and the same in words.
    std::string qr = wifiQrText(s.ssid, s.password);
    int qrSize = drawQr(c, 2, 2, H - 4, qr);
    const int x = qrSize + 8;
    c.text(x, 1, "WIFI SETUP", label);
    c.text(x, 14, "Join", small);
    c.text(x, 25, s.ssid, fitFont(fonts, s.ssid, W - x - 2, 13));
    c.text(x, 43, "Password", small);
    c.text(x, 54, s.password, fitFont(fonts, s.password, W - x - 2, 16));
    c.text(x, 76, "then open", small);
    c.text(x, 87, std::string("http://") + setup::HOTSPOT_ADDRESS, fitFont(fonts, "http://10.42.0.1", W - x - 2, 11));
    std::string foot = s.message.empty()
                           ? (s.deadline ? std::format("{} min left", std::max<long>(0, (s.deadline - now + 59) / 60))
                                         : "until WiFi is set up")
                           : s.message;
    c.text(x, 108, foot, fitFont(fonts, foot, W - x - 2, 9, false, 7));
    return c;
}

} // namespace purr::display
