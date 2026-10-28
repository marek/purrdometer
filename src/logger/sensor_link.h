// One sensor's BlueZ device: whether it is connected and its services are
// resolved (kept current from BlueZ's signals), connecting, disconnecting,
// and the measurement notifications once it is ready.
#pragma once

#include "logger/bluez.h"
#include "logger/callback_guard.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace purr::logger {

class SensorLink {
public:
    // onConnected: from either thread, whenever the connection state changes.
    SensorLink(sdbus::IConnection& bus, std::string mac, std::function<void(bool)> onConnected);
    // Closes the guard its signal handlers run under before the proxies go: a delivery that is
    // running finishes first, and one that comes after does nothing.
    ~SensorLink();

    const std::string& mac() const { return mac_; }
    const std::string& path() const { return path_; }
    bool connected() const { return connected_; }
    bool servicesResolved() const { return servicesResolved_; }
    bool notifying() const { return notifying_; }

    // Reads Connected and ServicesResolved from the device's properties.
    void update(const bluez::Props& props);
    void lost();                // BlueZ removed the device object
    void trust();               // BlueZ keeps a trusted, unbonded device's object
    void connect();             // a failure is logged: the scan finds it once it wakes
    void disconnect();

    // Subscribes to CSC Measurement; false if the characteristic is not there yet.
    bool startNotify(const bluez::ManagedObjects& objs, std::function<void(const std::vector<uint8_t>&)> onValue);

private:
    void setConnected(bool c);

    sdbus::IConnection& bus_;
    std::string mac_, path_;
    std::function<void(bool)> onConnected_;
    std::shared_ptr<CallbackGuard> guard_ = std::make_shared<CallbackGuard>(); // shared with the handlers
    std::unique_ptr<sdbus::IProxy> device_, characteristic_;
    std::atomic<bool> connected_{false}, servicesResolved_{false}, notifying_{false};
};

} // namespace purr::logger
