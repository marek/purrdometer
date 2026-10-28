#include "web/server.h"

#include "web/dashboard_routes.h"
#include "web/settings_routes.h"

#include <print>

namespace purr::web {

namespace {

constexpr int WORKER_THREADS = 2;

} // namespace

void addRoutes(const Site& site, httplib::Server& server)
{
    server.new_task_queue = [] { return new httplib::ThreadPool(WORKER_THREADS); };
    addDashboardRoutes(site, server);
    addSettingsRoutes(site, server);
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
    httplib::Server server;
    addRoutes(site, server);
    std::println("serving on http://{}:{}/ from {} ({}); settings: {}", bind, port, site.paths.db,
                 describe(cfg.wheel, cfg.editable.units), accessName(cfg.access));
    if (!server.listen(bind, port)) {
        std::println(stderr, "cannot listen on {}:{}", bind, port);
        return 1;
    }
    return 0;
}

} // namespace purr::web
