#include "logger/raw_log.h"

#include <cctype>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <print>
#include <sstream>
#include <stdexcept>

namespace purr::logger {
namespace {

std::tm localTime(std::time_t value)
{
    std::tm out{};
    localtime_r(&value, &out);
    return out;
}

std::string day(RawLog::TimePoint now)
{
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char out[11]{};
    std::tm tm = localTime(t);
    std::strftime(out, sizeof out, "%Y-%m-%d", &tm);
    return out;
}

std::string timestamp(RawLog::TimePoint now)
{
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = localTime(t);
    char base[32]{}, zone[8]{};
    std::strftime(base, sizeof base, "%Y-%m-%dT%H:%M:%S", &tm);
    std::strftime(zone, sizeof zone, "%z", &tm);
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    if (millis < 0)
        millis += 1000;
    return std::format("{}.{:03}{}", base, millis, zone);
}

std::string oneLine(std::string value)
{
    for (char& c : value)
        if (c == '\n' || c == '\r' || c == '\t' || std::iscntrl(static_cast<unsigned char>(c)))
            c = ' ';
    return value;
}

std::string hex(const std::vector<uint8_t>& bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes.size(); ++i)
        out << (i ? " " : "") << std::setw(2) << unsigned(bytes[i]);
    return out.str();
}

bool logName(const std::string& name)
{
    if (name.size() != 14 || name.substr(10) != ".log")
        return false;
    for (size_t i = 0; i < 10; ++i)
        if ((i == 4 || i == 7) ? name[i] != '-' : !std::isdigit(static_cast<unsigned char>(name[i])))
            return false;
    return true;
}

} // namespace

RawLog::RawLog(std::string directory, bool enabled, int keepDays, Clock clock)
    : directory_(std::move(directory)), clock_(std::move(clock)), enabled_(enabled), keepDays_(keepDays)
{
    if (enabled_)
        pruneLocked(clock_());
}

void RawLog::configure(bool enabled, int keepDays)
{
    std::scoped_lock lock(mutex_);
    TimePoint now = clock_();
    bool changed = enabled != enabled_ || keepDays != keepDays_;
    if (changed && enabled_ && !enabled)
        writeLocked(std::format("settings changed raw_log=off raw_log_days={}", keepDays), now);
    enabled_ = enabled;
    keepDays_ = keepDays;
    reportedFailure_ = false;
    pruneLocked(now);
    if (changed && enabled_)
        writeLocked(std::format("settings changed raw_log=on raw_log_days={}", keepDays_), now);
}

void RawLog::event(const std::string& name, const std::string& detail)
{
    std::scoped_lock lock(mutex_);
    if (!enabled_)
        return;
    writeLocked(oneLine(name) + (detail.empty() ? "" : " " + oneLine(detail)), clock_());
}

void RawLog::measurement(const std::vector<uint8_t>& bytes, const std::optional<Sample>& sample,
                         const Accounting& accounting, int estimated)
{
    std::scoped_lock lock(mutex_);
    if (!enabled_)
        return;
    std::string line = "measurement " + hex(bytes);
    if (!sample) {
        line += " garbage";
    } else {
        line += std::format(" {} revs={} event={} +{}", counterName(sample->counter), sample->revs, sample->eventTime,
                            accounting.delta);
        if (accounting.reset)
            line += accounting.delta ? " restart" : " garbage";
        if (estimated)
            line += " estimated=" + std::to_string(estimated);
        if (accounting.rpm >= 0)
            line += " rpm=" + std::to_string(int(accounting.rpm + 0.5));
    }
    writeLocked(line, clock_());
}

void RawLog::writeLocked(const std::string& message, TimePoint now)
{
    try {
        std::filesystem::create_directories(directory_);
        std::string path = directory_ + "/" + day(now) + ".log";
        bool opened = std::filesystem::exists(path);
        std::ofstream out(path, std::ios::app);
        out << timestamp(now) << ' ' << message << '\n';
        out.flush();
        if (!out)
            throw std::runtime_error("cannot append " + path);
        if (!opened)
            pruneLocked(now);
        reportedFailure_ = false;
    } catch (const std::exception& e) {
        report(e.what());
    }
}

void RawLog::pruneLocked(TimePoint now)
{
    namespace fs = std::filesystem;
    try {
        if (!fs::is_directory(directory_))
            return;
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm cutoff = localTime(t);
        cutoff.tm_mday -= keepDays_ - 1;
        cutoff.tm_hour = cutoff.tm_min = cutoff.tm_sec = 0;
        std::string oldest = day(std::chrono::system_clock::from_time_t(std::mktime(&cutoff)));
        for (const auto& entry : fs::directory_iterator(directory_)) {
            std::string name = entry.path().filename().string();
            if (entry.is_regular_file() && logName(name) && name.substr(0, 10) < oldest)
                fs::remove(entry.path());
        }
    } catch (const std::exception& e) {
        report(e.what());
    }
}

void RawLog::report(const std::string& message)
{
    if (!reportedFailure_)
        std::println(stderr, "raw sensor log: {}", message);
    reportedFailure_ = true;
}

} // namespace purr::logger
