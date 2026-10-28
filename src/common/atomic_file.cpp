#include "common/atomic_file.h"

#include <cerrno>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace purr {

namespace {

std::runtime_error failure(const std::string& what, const std::string& path)
{
    return std::runtime_error(what + " " + path + ": " + std::strerror(errno));
}

} // namespace

void replaceFile(const std::string& path, const std::string& content, mode_t mode)
{
    std::string pattern = path + ".XXXXXX";
    std::vector<char> tmp(pattern.begin(), pattern.end());
    tmp.push_back('\0');
    int fd = mkstemp(tmp.data());
    if (fd < 0)
        throw failure("cannot write", pattern);
    std::string name = tmp.data();
    auto fail = [&](const std::string& what, const std::string& on) {
        auto error = failure(what, on);
        close(fd);
        std::remove(name.c_str());
        return error;
    };
    if (fchmod(fd, mode) != 0)
        throw fail("cannot set the mode of", name);
    for (size_t done = 0; done < content.size();) {
        ssize_t n = ::write(fd, content.data() + done, content.size() - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            throw fail("cannot write", name);
        done += size_t(n);
    }
    if (fsync(fd) != 0)
        throw fail("cannot flush", name);
    if (close(fd) != 0) {
        std::remove(name.c_str());
        throw failure("cannot write", name);
    }
    if (std::rename(name.c_str(), path.c_str()) != 0) {
        auto error = failure("cannot replace", path);
        std::remove(name.c_str());
        throw error;
    }
    // The rename is only in the directory once the directory is on disk: without this a power cut can
    // bring the old file back, or neither. A filesystem that cannot sync a directory has nothing to offer.
    auto slash = path.find_last_of('/');
    std::string dir = slash == std::string::npos ? "." : slash == 0 ? "/" : path.substr(0, slash);
    int dirFd = open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0)
        throw failure("cannot open the folder of", path);
    int rc = fsync(dirFd), why = errno;
    close(dirFd);
    if (rc != 0 && why != EINVAL && why != ENOTSUP && why != EROFS) {
        errno = why;
        throw failure("cannot flush the folder of", path);
    }
}

} // namespace purr
