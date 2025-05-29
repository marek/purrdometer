#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

const std::string BLUEZ_BUS = "org.bluez";
const std::string ADAPTER_PATH = "/org/bluez/hci0";
const std::string DEVICE_FILE = "cadence_device.txt";
const std::string CSC_MEASUREMENT_CHAR_UUID = "00002a5b-0000-1000-8000-00805f9b34fb";

std::string macToDbusPath(const std::string& mac)
{
    std::string path = ADAPTER_PATH + "/dev_";
    for (auto c : mac)
        path += (c == ':') ? '_' : c;
    return path;
}

bool loadDeviceMac(std::string& mac)
{
    if (!std::filesystem::exists(DEVICE_FILE)) {
        return false;
    }
    std::ifstream file(DEVICE_FILE);
    std::getline(file, mac);
    return !mac.empty();
}

static void onPropertiesChanged(sdbus::Signal& signal) {
    try {
        std::string interface;
        std::map<std::string, sdbus::Variant> changedProperties;
        std::vector<std::string> invalidated;

        signal >> interface >> changedProperties >> invalidated;

        std::cout << "🔔 Signal: " << interface << " (path: " << signal.getPath() << ")" << std::endl;

        // Show all properties that changed
        for (const auto& [property, value] : changedProperties) {
            std::cout << "  → " << property;
            if (property == "Value") {
                try {
                    auto data = value.get<std::vector<uint8_t>>();
                    std::cout << " = [";
                    for (size_t i = 0; i < data.size(); ++i) {
                        printf("%02X", data[i]);
                        if (i < data.size() - 1) std::cout << " ";
                    }
                    std::cout << "] (" << data.size() << " bytes)" << std::endl;

                    // Parse CSC data
                    if (data.size() >= 1) {
                        uint8_t flags = data[0];
                        std::cout << "    🚴 CSC Flags: 0x" << std::hex << (int)flags << std::dec;
                        if (flags & 0x01) std::cout << " (wheel)";
                        if (flags & 0x02) std::cout << " (crank)";
                        std::cout << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cout << " (not bytes: " << e.what() << ")" << std::endl;
                }
            } else if (property == "Connected") {
                std::cout << " = " << (value.get<bool>() ? "true" : "false") << std::endl;
            } else {
                std::cout << " = <value>" << std::endl;
            }
        }

        // Show invalidated properties
        if (!invalidated.empty()) {
            std::cout << "  Invalidated: ";
            for (const auto& prop : invalidated) {
                std::cout << prop << " ";
            }
            std::cout << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Signal handler error: " << e.what() << std::endl;
    }
}

int main()
{
    std::string deviceMac;
    if (!loadDeviceMac(deviceMac)) {
        std::cerr << "No device MAC found. Run pair first." << std::endl;
        return 1;
    }

    std::string devicePath = macToDbusPath(deviceMac);
    std::cout << "Monitoring device: " << deviceMac << " (" << devicePath << ")" << std::endl;

    auto connection = sdbus::createSystemBusConnection();
    auto manager = sdbus::createProxy(*connection, BLUEZ_BUS, "/");

    // Set up global signal handler BEFORE connecting
    auto monitorProxy = sdbus::createProxy(*connection, BLUEZ_BUS, "/");
    monitorProxy->registerSignalHandler("org.freedesktop.DBus.Properties", "PropertiesChanged", onPropertiesChanged);
    monitorProxy->finishRegistration();
    std::cout << "📡 Global signal handler registered" << std::endl;

    std::string batteryCharPath;
    bool setupDone = false;

    while (true) {
        try {
            auto device = sdbus::createProxy(*connection, BLUEZ_BUS, devicePath);
            bool connected = device->getProperty("Connected").onInterface("org.bluez.Device1").get<bool>();

            if (!connected) {
                std::cout << "Device disconnected. Attempting reconnection..." << std::endl;
                try {
                    device->callMethod("Connect").onInterface("org.bluez.Device1");
                } catch (const std::exception& e) {
                    std::cout << "Reconnect error: " << e.what() << std::endl;
                }
                setupDone = false;
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
            else if (!setupDone)
            {
                std::cout << "Connected. Setting up CSC notifications (Android style)..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(3));

                // Get all GATT objects
                std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;
                manager->callMethod("GetManagedObjects").onInterface("org.freedesktop.DBus.ObjectManager").storeResultsTo(objects);

                // Enable CSC notifications only (like Android)
                for (const auto& [path, interfaces] : objects) {
                    if (path.find(devicePath) != std::string::npos && interfaces.count("org.bluez.GattCharacteristic1")) {
                        auto charProps = interfaces.at("org.bluez.GattCharacteristic1");
                        auto uuidVar = charProps.find("UUID");
                        auto flagsVar = charProps.find("Flags");

                        if (uuidVar != charProps.end() && flagsVar != charProps.end()) {
                            std::string uuid = uuidVar->second.get<std::string>();
                            auto flags = flagsVar->second.get<std::vector<std::string>>();

                            // Check if characteristic supports notify
                            bool canNotify = std::find(flags.begin(), flags.end(), "notify") != flags.end();

                            // Only enable CSC notifications like Android - write CCCD directly
                            if (canNotify && uuid == CSC_MEASUREMENT_CHAR_UUID) {
                                try {
                                    auto characteristic = sdbus::createProxy(*connection, BLUEZ_BUS, path);
                                    characteristic->callMethod("StartNotify").onInterface("org.bluez.GattCharacteristic1");
                                    std::cout << "✅ CSC notifications enabled with StartNotify" << std::endl;
                                    std::cout << "    Path: " << path << std::endl;
                                    std::cout << "    UUID: " << uuid << std::endl;
                                } catch (const std::exception& e) {
                                    std::cout << "❌ Failed to start CSC notifications: " << e.what() << std::endl;
                                }
                            }
                        }
                    }
                }

                setupDone = true;
                std::cout << "🚴 CSC notifications enabled. Rotate the sensor to generate data..." << std::endl;
                std::cout << "⏳ Waiting for sensor data..." << std::endl;
            }
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        connection->processPendingRequest();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}