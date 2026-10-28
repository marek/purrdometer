#include "web/server.h"

#include "web/dashboard_routes.h"
#include "web/captive.h"
#include "web/legacy.h"
#include "web/setup_routes.h"
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
    addSetupRoutes(site, server);
    addCaptiveRedirect(site, server);
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
    int legacy = site.legacyPort.value_or(cfg.legacyPort);
    httplib::Server old;
    std::jthread oldThread;
    if (legacy > 0 && legacy != port) {
        old.new_task_queue = [] { return new httplib::ThreadPool(1); };
        addLegacyRedirect(port, old);
        if (old.bind_to_port(bind, legacy)) {
            oldThread = std::jthread([&old] { old.listen_after_bind(); });
            std::println("port {} redirects to port {}", legacy, port);
        } else {
            std::println(stderr, "port {} is taken: no redirect from it", legacy);
        }
    }
    if (!server.listen(bind, port)) {
        old.stop();
        std::println(stderr, "cannot listen on {}:{}", bind, port);
        return 1;
    }
    return 0;
}

} // namespace purr::web
