// Tells when any of a few files was created, changed or removed, by their
// modification times: how the programs notice settings and requests without
// talking to each other.
#pragma once

#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace purr {

inline long long modifiedNs(const std::string& path)
{
    struct stat st{};
    return stat(path.c_str(), &st) == 0 ? (long long)st.st_mtim.tv_sec * 1'000'000'000 + st.st_mtim.tv_nsec : -1;
}

class FileWatch {
public:
    explicit FileWatch(std::vector<std::string> paths) : paths_(std::move(paths)), stamps_(stamp()) {}

    // True once after any of the files changed since the last call.
    bool changed()
    {
        auto now = stamp();
        if (now == stamps_)
            return false;
        stamps_ = std::move(now);
        return true;
    }

    // True once after the file was touched; a missing file never counts.
    bool touched()
    {
        auto now = stamp();
        bool touched = now != stamps_ && now[0] >= 0;
        stamps_ = std::move(now);
        return touched;
    }

private:
    std::vector<long long> stamp() const
    {
        std::vector<long long> s;
        for (auto& p : paths_)
            s.push_back(modifiedNs(p));
        return s;
    }

    std::vector<std::string> paths_;
    std::vector<long long> stamps_;
};

} // namespace purr
