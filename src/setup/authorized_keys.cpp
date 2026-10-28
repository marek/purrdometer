#include "setup/authorized_keys.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <print>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace purr::setup {

namespace {

constexpr size_t MAX_FILE_BYTES = 1 << 20;
constexpr mode_t DIR_MODE = 0700, FILE_MODE = 0600;

class Fd {
public:
    explicit Fd(int fd = -1) : fd_(fd) {}
    ~Fd()
    {
        if (fd_ >= 0)
            close(fd_);
    }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}

    int get() const { return fd_; }
    bool ok() const { return fd_ >= 0; }

private:
    int fd_;
};

std::unexpected<std::string> failed(const std::string& what)
{
    return std::unexpected(what + ": " + std::strerror(errno));
}

bool hasControlCharacter(const std::string& s)
{
    for (unsigned char c : s)
        if (c < 0x20 || c == 0x7f)
            return true;
    return false;
}

struct KeyId {
    std::string type, blob;
    bool operator==(const KeyId&) const = default;
};

bool isKeyType(const std::string& word)
{
    return word.starts_with("ssh-") || word.starts_with("ecdsa-") || word.starts_with("sk-");
}

// The key a line of authorized_keys names: its type and the base64 field after it. A line may begin with
// options, whose quoted values hold spaces; whatever follows the key is a comment and names nothing.
std::optional<KeyId> keyOf(const std::string& line)
{
    size_t i = 0;
    auto skipSpace = [&] {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
            ++i;
    };
    auto word = [&] {
        std::string w;
        bool quoted = false;
        for (; i < line.size() && (quoted || (line[i] != ' ' && line[i] != '\t')); ++i) {
            if (line[i] == '"')
                quoted = !quoted;
            w += line[i];
        }
        return w;
    };
    skipSpace();
    if (i >= line.size() || line[i] == '#')
        return std::nullopt;
    for (std::string w = word(); !w.empty(); skipSpace(), w = word()) {
        if (!isKeyType(w))
            continue;
        skipSpace();
        std::string blob = word();
        if (blob.empty())
            return std::nullopt;
        return KeyId{w, blob};
    }
    return std::nullopt;
}

bool listed(const std::string& file, const KeyId& key)
{
    std::istringstream in(file);
    std::string line;
    while (std::getline(in, line))
        if (keyOf(line) == key)
            return true;
    return false;
}

std::expected<std::string, std::string> readAll(int fd)
{
    std::string out;
    char buf[4096];
    while (true) {
        ssize_t n = ::read(fd, buf, sizeof buf);
        if (n < 0)
            return failed("cannot read authorized_keys");
        if (n == 0)
            return out;
        out.append(buf, size_t(n));
        if (out.size() > MAX_FILE_BYTES)
            return std::unexpected("authorized_keys is too large to be a list of keys");
    }
}

bool writeAll(int fd, const std::string& data)
{
    for (size_t done = 0; done < data.size();) {
        ssize_t n = ::write(fd, data.data() + done, data.size() - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        done += size_t(n);
    }
    return true;
}

} // namespace

std::expected<bool, std::string> addAuthorizedKey(const std::string& home, uid_t uid, gid_t gid,
                                                  const std::string& key)
{
    auto id = keyOf(key);
    // A submitted key begins with its type: options (command=, from=) are not ours to grant.
    if (key.empty() || hasControlCharacter(key) || !isKeyType(key) || !id)
        return std::unexpected("the SSH key is not one line of \"type key [comment]\"");

    // The home comes from the account database, which only root writes: it may be a link.
    Fd homeFd(open(home.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (!homeFd.ok())
        return failed("cannot open " + home);
    struct stat st{};
    if (fstat(homeFd.get(), &st) != 0)
        return failed("cannot examine " + home);
    if (st.st_uid != uid)
        return std::unexpected(home + " does not belong to the account");

    bool madeDir = mkdirat(homeFd.get(), ".ssh", DIR_MODE) == 0;
    if (!madeDir && errno != EEXIST)
        return failed("cannot create " + home + "/.ssh");
    Fd sshDir(openat(homeFd.get(), ".ssh", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (!sshDir.ok())
        return failed(home + "/.ssh is a link or not a directory");
    if (fchown(sshDir.get(), uid, gid) != 0 || fchmod(sshDir.get(), DIR_MODE) != 0)
        return failed("cannot give " + home + "/.ssh to the account");

    // Nonblocking, so a FIFO put there opens and is then refused rather than waited for.
    Fd file(openat(sshDir.get(), "authorized_keys", O_RDWR | O_CREAT | O_APPEND | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC,
                   FILE_MODE));
    if (!file.ok())
        return failed(home + "/.ssh/authorized_keys is a link or cannot be opened");
    if (fstat(file.get(), &st) != 0)
        return failed("cannot examine authorized_keys");
    if (!S_ISREG(st.st_mode) || st.st_nlink != 1)
        return std::unexpected(home + "/.ssh/authorized_keys is not an ordinary file with one name");
    if (fchown(file.get(), uid, gid) != 0 || fchmod(file.get(), FILE_MODE) != 0)
        return failed("cannot give authorized_keys to the account");

    auto existing = readAll(file.get());
    if (!existing)
        return std::unexpected(existing.error());
    if (listed(*existing, *id))
        return false;
    std::string add = (existing->empty() || existing->back() == '\n' ? "" : "\n") + key + "\n";
    if (!writeAll(file.get(), add) || fsync(file.get()) != 0) {
        int why = errno;
        if (ftruncate(file.get(), off_t(existing->size())) != 0)
            std::println(stderr, "authorized_keys keeps a partial key it could not remove");
        errno = why;
        return failed("cannot write authorized_keys");
    }
    return true;
}

} // namespace purr::setup
