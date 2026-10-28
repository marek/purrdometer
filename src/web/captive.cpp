#include "web/captive.h"

#include "common/setup_state.h"
#include "web/setup_routes.h"

#include <format>

namespace purr::web {

void addCaptiveRedirect(const Site& site, httplib::Server& server)
{
    server.set_pre_routing_handler([&](const httplib::Request& req, httplib::Response& res) {
        if (req.path.starts_with("/setup") || !onHotspot(req.remote_addr, site.hotspotPrefix) ||
            !setup::readState(site.paths.setupState).active())
            return httplib::Server::HandlerResponse::Unhandled;
        res.set_redirect(std::format("http://{}/setup", setup::HOTSPOT_ADDRESS), 302);
        return httplib::Server::HandlerResponse::Handled;
    });
}

} // namespace purr::web
