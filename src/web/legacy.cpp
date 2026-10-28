#include "web/legacy.h"

#include <format>

namespace purr::web {

void addLegacyRedirect(int dashboardPort, httplib::Server& server)
{
    auto redirect = [dashboardPort](const httplib::Request& req, httplib::Response& res) {
        std::string host = req.get_header_value("Host");
        host = host.substr(0, host.find(':'));
        if (host.empty())
            host = "localhost";
        std::string port = dashboardPort == 80 ? "" : std::format(":{}", dashboardPort);
        res.set_redirect(std::format("http://{}{}{}", host, port, req.target), 301);
    };
    server.Get(".*", redirect);
    server.Post(".*", redirect);
}

} // namespace purr::web
