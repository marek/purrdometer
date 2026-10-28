#include "web/options.h"

#include <print>

namespace purr::web {

namespace {

void usage()
{
    std::println(stderr, "usage: purrdometer-web [--config PATH] [--overrides PATH] [--profiles PATH] [--db PATH]\n"
                         "       [--sensor-log-dir PATH] [--wheels PATH] [--bind ADDR] [--port N] [--legacy-port N]\n"
                         "       [--check-config] [--selftest]");
}

} // namespace

std::optional<Options> parseOptions(int argc, char** argv)
{
    Options o;
    Paths& p = o.site.paths;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        bool hasValue = i + 1 < argc;
        auto next = [&] { return std::string(argv[++i]); };
        if (a == "--check-config") o.checkConfig = true;
        else if (a == "--selftest") o.selftest = true;
        else if (!hasValue) return usage(), std::nullopt;
        else if (a == "--config") p.config = next();
        else if (a == "--overrides") p.overrides = next();
        else if (a == "--profiles") p.profiles = next();
        else if (a == "--db") p.db = next();
        else if (a == "--sensor-log-dir") p.sensorLogDir = next();
        else if (a == "--wheels") p.wheels = next();
        else if (a == "--bind") o.site.bind = next();
        else if (a == "--port") o.site.port = std::stoi(next());
        else if (a == "--legacy-port") o.site.legacyPort = std::stoi(next());
        else return usage(), std::nullopt;
    }
    return o;
}

} // namespace purr::web
