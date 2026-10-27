#include "web/access.h"

#include <arpa/inet.h>

#include <format>
#include <fstream>

namespace purr::web {

bool isLanAddress(const std::string& addr)
{
    std::string a = addr;
    if (a.starts_with("::ffff:"))
        a = a.substr(7); // IPv4-mapped IPv6
    in_addr v4{};
    if (inet_pton(AF_INET, a.c_str(), &v4) == 1) {
        uint32_t ip = ntohl(v4.s_addr);
        auto in = [&](uint32_t net, int bits) { return (ip >> (32 - bits)) == (net >> (32 - bits)); };
        return in(0x7F000000, 8)        // 127.0.0.0/8
               || in(0x0A000000, 8)     // 10.0.0.0/8
               || in(0xAC100000, 12)    // 172.16.0.0/12
               || in(0xC0A80000, 16)    // 192.168.0.0/16
               || in(0xA9FE0000, 16);   // 169.254.0.0/16
    }
    in6_addr v6{};
    if (inet_pton(AF_INET6, a.c_str(), &v6) == 1) {
        const unsigned char* b = v6.s6_addr;
        bool loopback = true;
        for (int i = 0; i < 15; ++i)
            loopback = loopback && b[i] == 0;
        loopback = loopback && b[15] == 1;
        return loopback                              // ::1
               || (b[0] & 0xFE) == 0xFC              // fc00::/7
               || (b[0] == 0xFE && (b[1] & 0xC0) == 0x80); // fe80::/10
    }
    return false;
}

std::string newToken()
{
    unsigned char bytes[16] = {};
    std::ifstream("/dev/urandom", std::ios::binary).read(reinterpret_cast<char*>(bytes), sizeof bytes);
    std::string hex;
    for (unsigned char c : bytes)
        hex += std::format("{:02x}", c);
    return hex;
}

bool sameSecret(const std::string& a, const std::string& b)
{
    if (a.size() != b.size() || a.empty())
        return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

} // namespace purr::web
