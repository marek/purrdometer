// purrdometer-logger: a BLE Cycling Speed & Cadence sensor's revolutions into
// SQLite (docs/intents/ble/sensor.md, docs/intents/data/storage.md).
//
// Link-layer robustness (long LE supervision timeout, a random address, WiFi
// power-save off) is configured system-wide by the Debian package, not here.
//
// Usage: purrdometer-logger [--db PATH] [--sensor-file PATH] [--config PATH]
//                           [--overrides PATH] [--scan-request PATH] [--selftest]

#include "logger/log.h"
#include "logger/monitor.h"
#include "logger/options.h"
#include "logger/store.h"

int selftest();

int main(int argc, char** argv)
{
    using namespace purr::logger;
    auto options = parseOptions(argc, argv);
    if (!options)
        return 2;
    if (options->selftest)
        return selftest();
    try {
        Store store(options->paths.db);
        Monitor(store, options->paths).run();
    } catch (const std::exception& e) {
        log("fatal: ", e.what());
        return 1;
    }
}
