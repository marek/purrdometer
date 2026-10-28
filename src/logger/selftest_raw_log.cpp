// The optional raw sensor log: lines, timestamps, disabled behaviour and retention.

#include "logger/raw_log.h"
#include "logger/selftest.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace purr::logger;

void selftestRawLog(purr::Check& check)
{
    std::string dir = std::string(std::getenv("PURR_TMP") ? std::getenv("PURR_TMP") : "tmp") +
                      "/sensor-log-selftest";
    std::filesystem::remove_all(dir);
    std::tm tm{};
    tm.tm_year = 2026 - 1900;
    tm.tm_mon = 9;
    tm.tm_mday = 28;
    tm.tm_hour = 12;
    tm.tm_isdst = -1;
    RawLog::TimePoint now = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    RawLog raw(dir, false, 2, [&] { return now; });
    raw.event("connected", "AA:BB:CC:DD:EE:FF");
    check(!std::filesystem::exists(dir), "raw log off writes nothing");

    std::filesystem::create_directories(dir);
    std::ofstream(dir + "/2026-10-24.log") << "old\n";
    std::ofstream(dir + "/notes.txt") << "keep\n";
    raw.configure(true, 2);
    raw.event("connected", "AA:BB:CC:DD:EE:FF");
    raw.measurement({0x02, 0x0b, 0x00, 0x00, 0x04}, Sample{Counter::Crank, 11, 1024},
                    Accounting{.delta = 1, .rpm = 60}, 2);
    raw.measurement({0x01}, std::nullopt, Accounting{}, 0);
    std::ifstream in(dir + "/2026-10-28.log");
    std::string text((std::istreambuf_iterator<char>(in)), {});
    size_t stamp = text.find("2026-10-28T12:00:00.000");
    check(stamp != std::string::npos && text.size() > stamp + 28 &&
              (text[stamp + 23] == '+' || text[stamp + 23] == '-') &&
              text.find("connected AA:BB:CC:DD:EE:FF") != std::string::npos &&
              text.find("measurement 02 0b 00 00 04 crank revs=11 event=1024 +1 estimated=2 rpm=60") !=
                  std::string::npos &&
              text.find("measurement 01 garbage") != std::string::npos,
          "raw log writes local time with an offset, events, exact bytes and accounting");
    check(!std::filesystem::exists(dir + "/2026-10-24.log") && std::filesystem::exists(dir + "/notes.txt"),
          "raw log removes old days and nothing else");

    raw.configure(false, 2);
    auto before = std::filesystem::file_size(dir + "/2026-10-28.log");
    raw.event("disconnected");
    check(std::filesystem::file_size(dir + "/2026-10-28.log") == before,
          "turning the raw log off keeps its files and writes nothing more");
    std::ofstream(dir + "/2026-10-27.log") << "yesterday\n";
    raw.configure(false, 1);
    check(!std::filesystem::exists(dir + "/2026-10-27.log") &&
              std::filesystem::exists(dir + "/2026-10-28.log"),
          "changing retention prunes old days while logging is off");
    std::filesystem::remove_all(dir);
}
