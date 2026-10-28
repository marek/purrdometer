#include "logger/adapter.h"

#include "logger/log.h"

namespace purr::logger {

using namespace bluez;

Adapter::Adapter(sdbus::IConnection& bus, std::function<void(const std::string&)> onRemoved)
{
    root_ = sdbus::createProxy(bus, SERVICE, sdbus::ObjectPath{"/"});
    root_->uponSignal("InterfacesRemoved").onInterface(OM_IFACE).call(
        [onRemoved = std::move(onRemoved)](const sdbus::ObjectPath& path, const std::vector<std::string>&) {
            onRemoved(path);
        });
    adapter_ = sdbus::createProxy(bus, SERVICE, sdbus::ObjectPath{ADAPTER_PATH});
}

ManagedObjects Adapter::objects()
{
    ManagedObjects objs;
    root_->callMethod("GetManagedObjects").onInterface(OM_IFACE).storeResultsTo(objs);
    return objs;
}

void Adapter::startDiscovery(const std::string& lookingFor)
{
    adapter_->callMethod("SetDiscoveryFilter").onInterface(ADAPTER_IFACE).withArguments(
        Props{{"UUIDs", sdbus::Variant{std::vector<std::string>{CSC_SERVICE_UUID}}},
              {"Transport", sdbus::Variant{std::string("le")}}});
    try {
        adapter_->callMethod("StartDiscovery").onInterface(ADAPTER_IFACE);
        if (!discovering_)
            log("scanning for ", lookingFor);
    } catch (const sdbus::Error& e) {
        if (e.getName() != "org.bluez.Error.InProgress")
            throw;
    }
    discovering_ = true;
}

void Adapter::stopDiscovery()
{
    try {
        adapter_->callMethod("StopDiscovery").onInterface(ADAPTER_IFACE);
    } catch (const sdbus::Error&) {
    }
    discovering_ = false;
}

} // namespace purr::logger
