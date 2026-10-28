// The logger's SQLite database (docs/intents/data/storage.md):
//
//   minutes(minute PK, revs)        permanent history; minute = unix_time / 60
//   samples(ts, revs, event_time, delta)  raw readings, pruned after 30 days
//   sensors(address PK, name, kind, rssi, last_seen)  every CSC sensor heard
//   state(key PK, value)            live status for the display and the dashboard
//                                   (connected, rpm, last_motion, battery, sensor,
//                                    sensor_kind, last_counter, last_revs,
//                                    last_event_time, last_wall)
#pragma once

#include "logger/measurement.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

struct sqlite3;

namespace purr::logger {

class Store {
public:
    explicit Store(const std::string& path); // creates the tables; prunes old samples
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    void setState(const std::string& key, const std::string& value);
    void clearState(const std::string& key);
    std::optional<std::string> state(const std::string& key);

    // A sensor heard advertising (rssi) or whose features were read (kind).
    void sensorHeard(const std::string& address, const std::string& name, std::optional<int> rssi,
                     std::optional<std::string> kind = std::nullopt);

    // A sample and the revolutions it added, with the counter it leaves.
    void record(double ts, Sample cur, uint32_t delta);

    // A new sensor: nothing is counted across the switch.
    void forgetCounter();

private:
    void exec(const std::string& sql);

    sqlite3* db_ = nullptr;
    std::recursive_mutex mutex_; // called from the D-Bus thread and the main thread
};

} // namespace purr::logger
