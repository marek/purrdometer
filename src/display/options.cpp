#include "display/options.h"

#include <print>

namespace purr::display {

namespace {

void usage()
{
    std::println(stderr, "usage: purrdometer-display [--config PATH] [--overrides PATH] [--wheels PATH] [--db PATH]\n"
                         "       [--demo-db PATH] [--preview DIR] [--now UNIX_TIME] [--setup-state PATH]\n"
                         "       [--setup-request PATH] [--list-wheels] [--check-config] [--selftest] [-v]");
}

} // namespace

std::optional<Options> parseOptions(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        bool hasValue = i + 1 < argc;
        auto next = [&] { return std::string(argv[++i]); };
        if (a == "--list-wheels") o.listWheels = true;
        else if (a == "--check-config") o.checkConfig = true;
        else if (a == "--selftest") o.selftest = true;
        else if (a == "-v" || a == "--verbose") o.verbose = true;
        else if (!hasValue) return usage(), std::nullopt;
        else if (a == "--config") o.config = next();
        else if (a == "--overrides") o.overrides = next();
        else if (a == "--wheels") o.wheels = next();
        else if (a == "--db") o.db = next();
        else if (a == "--demo-db") o.demoDb = next();
        else if (a == "--preview") o.preview = next();
        else if (a == "--now") o.now = std::stoll(next());
        else if (a == "--setup-state") o.setupState = next();
        else if (a == "--setup-request") o.setupRequest = next();
        else return usage(), std::nullopt;
    }
    return o;
}

} // namespace purr::display
