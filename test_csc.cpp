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

int main()
{
    std::string deviceMac;
    if (!loadDeviceMac(deviceMac)) {
        std::cerr << "No device MAC found. Run pair first." << std::endl;
        return 1;
    }

    std::string devicePath = macToDbusPath(deviceMac);
    std::cout << "Testing CSC polling on device: " << deviceMac << std::endl;

    auto connection = sdbus::createSystemBusConnection();
    auto manager = sdbus::createProxy(*connection, BLUEZ_BUS, "/");

    // Find CSC characteristic
    std::string cscCharPath;
    std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;
    manager->callMethod("GetManagedObjects").onInterface("org.freedesktop.DBus.ObjectManager").storeResultsTo(objects);

    for (const auto& [path, interfaces] : objects) {
        if (path.find(devicePath) != std::string::npos && interfaces.count("org.bluez.GattCharacteristic1")) {
            auto charProps = interfaces.at("org.bluez.GattCharacteristic1");
            auto uuidVar = charProps.find("UUID");
            if (uuidVar != charProps.end()) {
                std::string uuid = uuidVar->second.get<std::string>();
                if (uuid == CSC_MEASUREMENT_CHAR_UUID) {
                    cscCharPath = path;
                    std::cout << "Found CSC characteristic: " << path << std::endl;
                    break;
                }
            }
        }
    }

    if (cscCharPath.empty()) {
        std::cout << "CSC characteristic not found!" << std::endl;
        return 1;
    }

    auto cscChar = sdbus::createProxy(*connection, BLUEZ_BUS, cscCharPath);

    // Try to enable notifications first
    try {
        cscChar->callMethod("StartNotify").onInterface("org.bluez.GattCharacteristic1");
        std::cout << "✅ CSC notifications enabled" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "❌ Failed to enable notifications: " << e.what() << std::endl;
    }

    // Poll the characteristic directly
    std::cout << "🔄 Starting CSC polling. Rotate the sensor..." << std::endl;

    for (int i = 0; i < 30; i++) {
        try {
            std::vector<uint8_t> data;
            cscChar->callMethod("ReadValue").onInterface("org.bluez.GattCharacteristic1").storeResultsTo(data);

            if (!data.empty()) {
                std::cout << "📡 CSC Data (size=" << data.size() << "): ";
                for (uint8_t byte : data) printf("%02X ", byte);
                std::cout << std::endl;

                // Parse CSC data
                if (data.size() >= 5 && (data[0] & 0x02)) {
                    uint16_t crank_revs = data[1] | (data[2] << 8);
                    uint16_t crank_event_time = data[3] | (data[4] << 8);
                    std::cout << "🚴 Cadence: " << crank_revs << " revs, time: " << crank_event_time << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "❌ Read failed: " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}