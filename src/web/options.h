// purrdometer-web's command line.
#pragma once

#include "web/site.h"

#include <optional>

namespace purr::web {

struct Options {
    Site site;
    bool checkConfig = false, selftest = false;
};

// nullopt after printing the usage.
std::optional<Options> parseOptions(int argc, char** argv);

} // namespace purr::web
