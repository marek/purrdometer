#include "web/sensor_logs.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>

namespace purr::web {
namespace {

constexpr size_t MAX_DOWNLOAD = 64 << 20;

bool validDay(const std::string& day)
{
    if (day.size() != 10)
        return false;
    for (size_t i = 0; i < day.size(); ++i)
        if ((i == 4 || i == 7) ? day[i] != '-' : !std::isdigit(static_cast<unsigned char>(day[i])))
            return false;
    return true;
}

} // namespace

std::vector<std::string> sensorLogDays(const std::string& directory)
{
    namespace fs = std::filesystem;
    std::vector<std::string> out;
    std::error_code error;
    fs::directory_iterator entries(directory, error);
    if (error)
        return out;
    for (const auto& entry : entries) {
        std::string name = entry.path().filename().string();
        if (name.size() == 14 && name.substr(10) == ".log" && validDay(name.substr(0, 10)) &&
            fs::is_regular_file(entry.symlink_status(error)) && !error)
            out.push_back(name.substr(0, 10));
        error.clear();
    }
    std::sort(out.rbegin(), out.rend());
    return out;
}

std::optional<std::string> readSensorLog(const std::string& directory, const std::string& day)
{
    if (!validDay(day))
        return std::nullopt;
    std::string path = directory + "/" + day + ".log";
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0)
        return std::nullopt;
    struct stat st{};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 || uint64_t(st.st_size) > MAX_DOWNLOAD) {
        close(fd);
        return std::nullopt;
    }
    std::string out(size_t(st.st_size), '\0');
    size_t done = 0;
    while (done < out.size()) {
        ssize_t n = read(fd, out.data() + done, out.size() - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            close(fd);
            return std::nullopt;
        }
        done += size_t(n);
    }
    close(fd);
    return out;
}

} // namespace purr::web
