#include "web/portal.h"

#include "common/setup_state.h"
#include "web/setup_routes.h"

#include <format>

namespace purr::web {

void addPortalRoutes(const Site& site, httplib::Server& server)
{
    addSetupRoutes(site, server);
    // A phone's connectivity check asks for some other site's page; on the
    // hotspot, every name is the Pi, so the answer sends it to the setup page.
    auto elsewhere = [&](const httplib::Request& req, httplib::Response& res) {
        if (setup::readState(site.paths.setupState).active() && onHotspot(req.remote_addr, site.hotspotPrefix)) {
            res.set_redirect(std::format("http://{}/setup", setup::HOTSPOT_ADDRESS), 302);
            return;
        }
        std::string host = req.get_header_value("Host");
        host = host.substr(0, host.find(':'));
        if (host.empty())
            host = setup::HOTSPOT_ADDRESS;
        res.set_redirect(std::format("http://{}:{}/", host, site.port.value_or(site.config().port)), 302);
    };
    server.Get(R"(/(?!setup).*)", elsewhere);
}

} // namespace purr::web
