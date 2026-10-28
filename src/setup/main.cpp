// purrdometer-setup: the privileged half of setup mode
// (docs/intents/network/setup.md). Runs as root and only scans, runs the
// hotspot, joins WiFi and sets the login account, on requests the
// unprivileged screen and web server leave in files.
//
// Usage: purrdometer-setup [--hotspot-minutes N] [--selftest]

#include "common/paths.h"
#include "setup/network_manager.h"
#include "setup/setup_mode.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <print>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

int selftest();

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    int minutes = 10;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--selftest")
            return selftest();
        if (a == "--hotspot-minutes" && i + 1 < argc) {
            minutes = std::max(1, std::atoi(argv[++i]));
            continue;
        }
        std::println(stderr, "usage: purrdometer-setup [--hotspot-minutes N] [--selftest]");
        return 2;
    }
    if (geteuid() != 0) {
        std::println(stderr, "purrdometer-setup runs as root");
        return 1;
    }
    try {
        mkdir(purr::SETUP_RUN_DIR, 0755);
        purr::setup::NetworkManager wifi;
        purr::setup::SetupMode setup(wifi, minutes);
        setup.begin();
        while (true) {
            setup.tick(std::time(nullptr));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    } catch (const std::exception& e) {
        std::println(stderr, "fatal: {}", e.what());
        return 1;
    }
}
