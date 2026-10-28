// The Bluetooth adapter: discovery filtered to CSC sensors over LE, BlueZ's
// objects, and word of a device object going away.
#pragma once

#include "logger/bluez.h"

#include <functional>
#include <memory>
#include <string>

namespace purr::logger {

class Adapter {
public:
    // onRemoved: from the D-Bus thread, with the path of an object BlueZ removed.
    Adapter(sdbus::IConnection& bus, std::function<void(const std::string&)> onRemoved);

    bluez::ManagedObjects objects();
    void startDiscovery(const std::string& lookingFor); // logs once per scan
    void stopDiscovery();
    bool discovering() const { return discovering_; }

private:
    std::unique_ptr<sdbus::IProxy> root_, adapter_;
    bool discovering_ = false;
};

} // namespace purr::logger
