#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <filesystem>

const std::string BLUEZ_BUS = "org.bluez";
const std::string ADAPTER_PATH = "/org/bluez/hci0";
const std::string DEVICE_FILE = "cadence_device.txt";
const std::string CSC_SERVICE_UUID = "00001816-0000-1000-8000-00805f9b34fb";

// Save MAC to file
void saveDevice(const std::string& mac)
{
    std::ofstream file(DEVICE_FILE);
    file << mac;
    std::cout << "Saved device MAC: " << mac << " to file\n";
}

// Load MAC from file, if exists
bool loadDevice(std::string& mac)
{
    if (!std::filesystem::exists(DEVICE_FILE)) return false;
    std::ifstream file(DEVICE_FILE);
    std::getline(file, mac);
    return !mac.empty();
}

// Convert MAC address to DBus device path
std::string macToDbusPath(const std::string& mac)
{
    std::string path = ADAPTER_PATH + "/dev_";
    for (auto c : mac) path += (c == ':') ? '_' : c;
    return path;
}

// Start scanning using BlueZ
void startDiscovery(sdbus::IProxy& adapter)
{
    try { adapter.callMethod("StartDiscovery").onInterface("org.bluez.Adapter1"); }
    catch (...) {}
}

// Stop scanning
void stopDiscovery(sdbus::IProxy& adapter)
{
    try { adapter.callMethod("StopDiscovery").onInterface("org.bluez.Adapter1"); }
    catch (...) {}
}

// Connect without pairing
bool connectDevice(const std::string& devicePath, sdbus::IConnection& conn)
{
    auto device = sdbus::createProxy(conn, "org.bluez", devicePath);
    try {
        // Set trusted but don't pair
        device->setProperty("Trusted").onInterface("org.bluez.Device1").toValue(true);
        device->callMethod("Connect").onInterface("org.bluez.Device1");
        std::cout << "Connected without pairing.\n";
        return true;
    } catch (const sdbus::Error& e) {
        std::cerr << "Connection error: " << e.getMessage() << std::endl;
        return false;
    }
}

int main()
{
    auto conn = sdbus::createSystemBusConnection();
    auto adapter = sdbus::createProxy(*conn, BLUEZ_BUS, ADAPTER_PATH);

    adapter->callMethod("SetDiscoveryFilter").onInterface("org.bluez.Adapter1").withArguments(
        std::map<std::string, sdbus::Variant>{{"UUIDs", std::vector<std::string>{CSC_SERVICE_UUID}}}
    );

    std::string mac;
    if (loadDevice(mac)) {
        std::string path = macToDbusPath(mac);
        auto device = sdbus::createProxy(*conn, "org.bluez", path);
        try {
            bool connected = device->getProperty("Connected").onInterface("org.bluez.Device1").get<bool>();
            if (connected) {
                std::cout << "Device already connected: " << mac << "\n";
                return 0;
            } else {
                std::cout << "Attempting to reconnect to saved device.\n";
                if (connectDevice(path, *conn)) return 0;
            }
        } catch (...) {
            std::cout << "Saved device not available, will search again.\n";
        }
    } else {
        std::cout << "No saved device, starting discovery.\n";
    }

    startDiscovery(*adapter);
    bool found = false;

    while (!found) {
        auto mgr = sdbus::createProxy(*conn, BLUEZ_BUS, "/");
        std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> objs;
        mgr->callMethod("GetManagedObjects").onInterface("org.freedesktop.DBus.ObjectManager").storeResultsTo(objs);

        for (const auto& [path, ifaces] : objs) {
            if (ifaces.count("org.bluez.Device1")) {
                auto props = ifaces.at("org.bluez.Device1");
                auto uuidsVar = props.find("UUIDs");
                if (uuidsVar != props.end()) {
                    auto uuids = uuidsVar->second.get<std::vector<std::string>>();
                    if (std::find(uuids.begin(), uuids.end(), CSC_SERVICE_UUID) != uuids.end()) {
                        std::string deviceAddress = props.at("Address").get<std::string>();
                        std::cout << "Found cadence device: " << deviceAddress << "\n";
                        saveDevice(deviceAddress);
                        stopDiscovery(*adapter);
                        if (connectDevice(path, *conn))
                            found = true;
                        else
                            std::cout << "Retrying...\n";
                        break;
                    }
                }
            }
        }

        if (!found) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    return 0;
}