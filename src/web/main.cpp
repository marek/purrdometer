// purrdometer-web: the dashboard over HTTP, the same history as the e-paper
// screens, read-only from the logger's database, and the settings page.
// cpp-httplib serves pages pagec compiled into C++ (web/pages/*.page).
//
// Usage: purrdometer-web [--config PATH] [--overrides PATH] [--profiles PATH] [--db PATH]
//                        [--sensor-log-dir PATH] [--wheels PATH] [--bind ADDR] [--port N]
//                        [--legacy-port N] [--check-config] [--selftest]

#include "web/options.h"
#include "web/server.h"

#include <cstdio>
#include <cstdlib>
#include <print>

int selftest(const std::string& wheelsPath, const std::string& tmpDir);

int main(int argc, char** argv)
{
    using namespace purr;
    std::setvbuf(stdout, nullptr, _IOLBF, 0); // one journal line per message
    auto options = web::parseOptions(argc, argv);
    if (!options)
        return 2;
    web::Site& site = options->site;
    try {
        if (options->selftest) {
            const char* tmp = std::getenv("PURR_TMP");
            return selftest(site.paths.wheels, tmp ? tmp : "tmp");
        }
        site.presets = loadPresets(site.paths.wheels);
        if (!options->checkConfig)
            return web::serve(site);
        Config cfg = site.config();
        std::println("{}: {}; settings from the web: {}", site.paths.config, describe(cfg.wheel, cfg.editable.units),
                     accessName(cfg.access));
        for (auto& e : cfg.errors)
            std::println(stderr, "error: {}", e);
        return cfg.errors.empty() ? 0 : 1;
    } catch (const std::exception& e) {
        std::println(stderr, "fatal: {}", e.what());
        return 1;
    }
}
