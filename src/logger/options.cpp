#include "logger/options.h"

#include <print>

namespace purr::logger {

std::optional<Options> parseOptions(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        std::string* field = a == "--db"                ? &o.paths.db
                             : a == "--sensor-log-dir" ? &o.paths.sensorLogDir
                             : a == "--sensor-file"    ? &o.paths.sensorFile
                             : a == "--config"       ? &o.paths.config
                             : a == "--overrides"    ? &o.paths.overrides
                             : a == "--profiles"     ? &o.paths.profiles
                             : a == "--scan-request" ? &o.paths.scanRequest
                                                     : nullptr;
        if (a == "--selftest") {
            o.selftest = true;
        } else if (field && i + 1 < argc) {
            *field = argv[++i];
        } else {
            std::println(stderr, "usage: purrdometer-logger [--db PATH] [--sensor-log-dir PATH] [--sensor-file PATH]\n"
                                 "       [--config PATH] [--overrides PATH] [--profiles PATH] [--scan-request PATH]\n"
                                 "       [--selftest]");
            return std::nullopt;
        }
    }
    return o;
}

} // namespace purr::logger
