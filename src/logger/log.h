// One journal line per message, safe from the D-Bus thread and the main
// thread; journald adds the timestamps.
#pragma once

#include <chrono>
#include <iostream>
#include <mutex>

namespace purr::logger {

inline std::mutex logMutex;

template <typename... Args>
void log(Args&&... args)
{
    std::scoped_lock lock(logMutex);
    (std::cout << ... << args) << std::endl;
}

// Wall-clock seconds since the epoch, as the database stores them.
inline double nowSeconds()
{
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

} // namespace purr::logger
