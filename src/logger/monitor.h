// The logger's loop: scan, connect, start notifications, and follow the
// settings and scan requests. BlueZ's signals arrive on sdbus's event-loop
// thread; this loop runs on the main thread and returns to the top every
// half second, so a change is seen even while the sensor sleeps.
#pragma once

#include "common/file_watch.h"
#include "logger/adapter.h"
#include "logger/options.h"
#include "logger/revolution_counter.h"
#include "logger/sensor_choice.h"
#include "logger/sensor_link.h"
#include "logger/store.h"

#include <chrono>
#include <memory>

namespace purr::logger {

class Monitor {
public:
    static constexpr auto TICK = std::chrono::milliseconds(500);
    static constexpr auto SCAN_ON_REQUEST = std::chrono::seconds(20);

    Monitor(Store& store, const Paths& paths);
    ~Monitor();

    [[noreturn]] void run();

private:
    void step();
    void follow(const std::optional<std::string>& mac);
    void followSettings();
    void serveScanRequest();     // a scan the settings page asked for, connected or not
    void scanStep();             // one look at what the scan has heard
    void recordHeard(const bluez::ManagedObjects& objs);
    bool deviceExists();
    void startNotify();
    void publishConnected(bool c);

    Store& store_;
    std::unique_ptr<sdbus::IConnection> bus_;
    Adapter adapter_;
    RevolutionCounter counter_;
    SensorChoice choice_;
    FileWatch scanRequest_;
    std::unique_ptr<SensorLink> link_;
    bool found_ = false; // heard advertising: worth a Connect
};

} // namespace purr::logger
