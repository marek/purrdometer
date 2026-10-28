#include "web/server.h"

#include "web/dashboard_routes.h"
#include "web/portal.h"
#include "web/settings_routes.h"
#include "web/wheel_routes.h"

#include <print>
#include <thread>

namespace purr::web {

namespace {

constexpr int WORKER_THREADS = 2;

} // namespace

void addRoutes(const Site& site, httplib::Server& server)
{
    server.new_task_queue = [] { return new httplib::ThreadPool(WORKER_THREADS); };
    addDashboardRoutes(site, server);
    addSettingsRoutes(site, server);
    addWheelRoutes(site, server);
    server.set_exception_handler([](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            std::println(stderr, "{}: {}", req.path, e.what());
        }
        res.status = 500;
        res.set_content("internal error\n", "text/plain");
    });
}

int serve(const Site& site)
{
    Config cfg = site.config();
    std::string bind = site.bind.value_or(cfg.bind);
    int port = site.port.value_or(cfg.port);
    ensureProfiles(site);
    httplib::Server server;
    addRoutes(site, server);
    std::println("serving on http://{}:{}/ from {} ({}); settings: {}", bind, port, site.paths.db,
                 describe(cfg.wheel, cfg.editable.units), accessName(cfg.access));
    httplib::Server portal;
    std::jthread portalThread;
    if (site.portalPort > 0) {
        portal.new_task_queue = [] { return new httplib::ThreadPool(1); };
        addPortalRoutes(site, portal);
        if (portal.bind_to_port(bind, site.portalPort)) {
            portalThread = std::jthread([&portal] { portal.listen_after_bind(); });
            std::println("setup portal on port {}", site.portalPort);
        } else {
            std::println(stderr, "no setup portal: cannot listen on port {}", site.portalPort);
        }
    }
    if (!server.listen(bind, port)) {
        portal.stop();
        std::println(stderr, "cannot listen on {}:{}", bind, port);
        return 1;
    }
    return 0;
}

} // namespace purr::web
