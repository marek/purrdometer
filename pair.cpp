#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <iostream>

const std::string BLUEZ_BUS = "org.bluez";
const std::string ADAPTER_PATH = "/org/bluez/hci0";
const std::string DEVICE_FILE = "cadence_device.txt";
const std::string CSC_SERVICE_UUID = "00001816-0000-1000-8000-00805f9b34fb";

enum class PairingResult {
    Success,
    AuthenticationFailed,
    ConnectionFailed,
    Timeout,
    UnknownError
};

bool isPaired(const std::string& devicePath, sdbus::IConnection& conn)
{
    auto deviceProxy = sdbus::createProxy(conn, BLUEZ_BUS, devicePath);
    try {
        return deviceProxy->getProperty("Paired").onInterface("org.bluez.Device1").get<bool>();
    } catch (...) { return false; }
}

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

// Attempt pairing/trusting
PairingResult pairDevice(const std::string& devicePath, sdbus::IConnection& conn)
{
    auto device = sdbus::createProxy(conn, "org.bluez", devicePath);
    try {
        device->setProperty("Trusted").onInterface("org.bluez.Device1").toValue(true);
        device->callMethod("Pair").onInterface("org.bluez.Device1");
        std::cout << "Pairing succeeded and device trusted.\n";
        return PairingResult::Success;
    } catch (const sdbus::Error& e) {
        std::cerr << "Pairing D-Bus error: " << e.getName() << " - " << e.getMessage() << std::endl;

        // Clearly distinguish error type by BlueZ error name:
        if (e.getName() == "org.bluez.Error.AuthenticationFailed") {
            return PairingResult::AuthenticationFailed;
        } else if (e.getName() == "org.bluez.Error.ConnectionAttemptFailed") {
            return PairingResult::ConnectionFailed;
        } else if (e.getName() == "org.bluez.Error.AuthenticationTimeout") {
            return PairingResult::Timeout;
        } else {
            return PairingResult::UnknownError;
        }
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
        if (isPaired(path, *conn)) {
            std::cout << "Device already paired: " << mac << "\n";
            return 0;
        } else {
            std::cout << "Device in file not paired, will search again.\n";
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
                        if (pairDevice(path, *conn) == PairingResult::Success)
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