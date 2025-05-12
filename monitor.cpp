#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <iostream>

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

void parseCscMeasurement(const std::vector<uint8_t>& data)
{
    uint8_t flags = data[0];
    int index = 1;

    if (flags & 0x02)
    {
        uint16_t crank_revs = data[index] | (data[index+1]<<8);
        uint16_t crank_event_time = data[index+2] | (data[index+3]<<8);

        std::cout << "[Cadence sensor] Crank revs: " << crank_revs
                  << ", Event time: " << crank_event_time << std::endl;
    }
}

int main()
{
    auto connection = sdbus::createSystemBusConnection();

    std::string mac;
    if (!loadDeviceMac(mac)) {
        std::cerr << "No device MAC found in " << DEVICE_FILE << std::endl;
        return 1;
    }

    std::string devicePath = macToDbusPath(mac);
    auto device = sdbus::createProxy(*connection, BLUEZ_BUS, devicePath);
    auto manager = sdbus::createProxy(*connection, BLUEZ_BUS, "/");

    bool notificationStarted = false;
    std::string measurementCharPath;

    while(true)
    {
        try {
            bool isConnected = device->getProperty("Connected")
                .onInterface("org.bluez.Device1").get<bool>();

            if (!isConnected)
            {
                std::cout << "Device disconnected. Attempting reconnection..." << std::endl;
                try {
                    device->callMethod("Connect").onInterface("org.bluez.Device1");
                } catch(const std::exception &e){
                    std::cerr << "Reconnect attempt error: " << e.what() << std::endl;
                }
                notificationStarted = false;
                measurementCharPath.clear();
            }
            else if (!notificationStarted)
            {
                std::cout << "Connected. Setting up notifications..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2)); // for services discovery

                if (measurementCharPath.empty())
                {
                    std::map<sdbus::ObjectPath,
                             std::map<std::string, std::map<std::string, sdbus::Variant>>>
                    objects;

                    manager->callMethod("GetManagedObjects")
                        .onInterface("org.freedesktop.DBus.ObjectManager")
                        .storeResultsTo(objects);

                    for (const auto& [path, interfaces] : objects) {
                        auto charIter = interfaces.find("org.bluez.GattCharacteristic1");
                        if (charIter != interfaces.end()) {
                            auto uuidVar = charIter->second.find("UUID");
                            if (uuidVar != charIter->second.end()
                                && uuidVar->second.get<std::string>() == CSC_MEASUREMENT_CHAR_UUID) {
                                measurementCharPath = path;
                                break;
                            }
                        }
                    }

                    if (measurementCharPath.empty())
                    {
                        std::cerr << "CSC Measurement characteristic not found yet." << std::endl;
                        continue;
                    }
                }

                auto characteristic = sdbus::createProxy(*connection, BLUEZ_BUS, measurementCharPath);
                characteristic->callMethod("StartNotify").onInterface("org.bluez.GattCharacteristic1");
                characteristic->registerSignalHandler("org.freedesktop.DBus.Properties",
                                                      "PropertiesChanged",
                [](sdbus::Signal& signal)
                {
                    std::string iface;
                    std::map<std::string, sdbus::Variant> changed;
                    std::vector<std::string> invalidated;
                    signal >> iface >> changed >> invalidated;

                    if (changed.count("Value")) {
                        auto data = changed["Value"].get<std::vector<uint8_t>>();

                        std::cout << "🔎 Raw notification data (size=" << data.size() << "): ";
                        for (uint8_t byte : data)
                            printf("%02X ", byte);
                        std::cout << std::endl;

                        parseCscMeasurement(data);
                    }
                });

                characteristic->finishRegistration();
                notificationStarted = true;
                std::cout << "Notifications enabled." << std::endl;
            }
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        connection->processPendingRequest();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
