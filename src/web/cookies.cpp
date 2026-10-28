#include "web/cookies.h"

namespace purr::web {

std::string cookie(const httplib::Request& req, const std::string& name)
{
    std::string all = req.get_header_value("Cookie");
    for (size_t pos = 0; pos < all.size();) {
        size_t end = all.find(';', pos);
        std::string part = all.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        size_t b = part.find_first_not_of(' ');
        if (b != std::string::npos && part.compare(b, name.size() + 1, name + "=") == 0)
            return part.substr(b + name.size() + 1);
        if (end == std::string::npos)
            break;
        pos = end + 1;
    }
    return {};
}

} // namespace purr::web
