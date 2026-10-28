#include "display/screens/qr.h"

#include <qrencode.h>

#include <algorithm>

namespace purr::display {

namespace {

// QR text needs \ ; , : and " escaped.
std::string qrEscape(const std::string& s)
{
    std::string out;
    for (char ch : s) {
        if (ch == '\\' || ch == ';' || ch == ',' || ch == ':' || ch == '"')
            out += '\\';
        out += ch;
    }
    return out;
}

} // namespace

std::string wifiQrText(const std::string& ssid, const std::string& password)
{
    return "WIFI:T:WPA;S:" + qrEscape(ssid) + ";P:" + qrEscape(password) + ";;";
}

int drawQr(Canvas& c, int x, int y, int size, const std::string& text)
{
    QRcode* qr = QRcode_encodeString(text.c_str(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!qr)
        return 0;
    const int quiet = 2, modules = qr->width + 2 * quiet, px = std::max(1, size / modules), side = modules * px;
    for (int row = 0; row < qr->width; ++row)
        for (int col = 0; col < qr->width; ++col)
            if (qr->data[row * qr->width + col] & 1)
                c.fill(x + (col + quiet) * px, y + (row + quiet) * px, x + (col + quiet + 1) * px - 1,
                       y + (row + quiet + 1) * px - 1);
    QRcode_free(qr);
    return side;
}

} // namespace purr::display
