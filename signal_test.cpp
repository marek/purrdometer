#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <iostream>

int main()
{
    std::cout << "Testing D-Bus signal reception..." << std::endl;

    auto connection = sdbus::createSystemBusConnection();
    auto proxy = sdbus::createProxy(*connection, "org.bluez", "/");

    std::cout << "Registering signal handler..." << std::endl;

    proxy->registerSignalHandler("org.freedesktop.DBus.Properties", "PropertiesChanged",
    [](sdbus::Signal& signal)
    {
        std::string iface;
        std::map<std::string, sdbus::Variant> changed;
        std::vector<std::string> invalidated;

        signal >> iface >> changed >> invalidated;

        std::cout << "📡 Signal received - Interface: " << iface << std::endl;
        for (const auto& [key, value] : changed) {
            std::cout << "    Property: " << key << std::endl;
        }
    });

    proxy->finishRegistration();
    std::cout << "Signal handler registered. Waiting for signals..." << std::endl;

    // Wait and process signals
    for (int i = 0; i < 30; i++) {
        connection->processPendingRequest();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (i % 4 == 0) {
            std::cout << "." << std::flush;
        }
    }

    std::cout << "\nTest complete." << std::endl;
    return 0;
}