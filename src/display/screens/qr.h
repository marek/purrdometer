// QR codes through libqrencode: the one that joins a WiFi network.
#pragma once

#include "display/canvas.h"

#include <string>

namespace purr::display {

// The WIFI: text a phone camera offers to join, with \ ; , : and " escaped.
std::string wifiQrText(const std::string& ssid, const std::string& password);

// The largest QR code of `text` that fits a `size` square at (x, y), with its
// quiet zone; returns the side it took.
int drawQr(Canvas& c, int x, int y, int size, const std::string& text);

} // namespace purr::display
