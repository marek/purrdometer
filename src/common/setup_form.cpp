#include "common/setup_form.h"

#include "common/kv_file.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace purr::setup {

namespace {

constexpr size_t MAX_FORM_BYTES = 1 << 16;

// An open form: a descriptor, opened without following a link, on a regular
// file with one name. The path lies in a folder the unprivileged web server
// writes, so it may be anything it likes; only what passes here is read.
class Form {
public:
    explicit Form(const std::string& path)
        : fd_(open(path.c_str(), O_RDWR | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC))
    {
        struct stat st{};
        if (fd_ >= 0 && (fstat(fd_, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1 || st.st_size > off_t(MAX_FORM_BYTES))) {
            close(fd_);
            fd_ = -1;
        }
        size_ = fd_ >= 0 ? size_t(st.st_size) : 0;
    }
    ~Form()
    {
        if (fd_ >= 0)
            close(fd_);
    }
    Form(const Form&) = delete;
    Form& operator=(const Form&) = delete;

    bool ok() const { return fd_ >= 0; }

    std::string read() const
    {
        std::string text(size_, '\0');
        size_t done = 0;
        while (done < size_) {
            ssize_t n = pread(fd_, text.data() + done, size_ - done, off_t(done));
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                break;
            done += size_t(n);
        }
        text.resize(done);
        return text;
    }

    // Passwords are overwritten where they were written before the name goes.
    void wipe() const
    {
        std::string zeros(std::max<size_t>(size_, 4096), '\0');
        [[maybe_unused]] ssize_t n = pwrite(fd_, zeros.data(), zeros.size(), 0);
        fsync(fd_);
        [[maybe_unused]] int t = ftruncate(fd_, 0);
    }

private:
    int fd_;
    size_t size_ = 0;
};

Apply fromKeyValues(const KeyValues& kv)
{
    auto get = [&](const char* key) {
        auto it = kv.find(key);
        return it == kv.end() ? std::string() : it->second;
    };
    return Apply{get("ssid"), get("wifi_password"), get("hidden") == "1", get("login_password"), get("ssh") == "1",
                 get("ssh_key")};
}

} // namespace

std::optional<Apply> readApply(const std::string& path)
{
    Form form(path);
    if (!form.ok())
        return std::nullopt;
    Apply a = fromKeyValues(parseKeyValues(form.read()));
    if (a.ssid.empty())
        return std::nullopt;
    return a;
}

std::optional<Apply> takeApply(const std::string& path)
{
    std::optional<Apply> a;
    {
        Form form(path);
        if (form.ok()) {
            a = fromKeyValues(parseKeyValues(form.read()));
            if (a->ssid.empty())
                a.reset();
            form.wipe();
        }
    }
    unlink(path.c_str()); // the name only: a link is removed, never followed
    return a;
}

void writeApply(const Apply& a, const std::string& path)
{
    writeKeyValues(path,
                   {{"ssid", a.ssid},
                    {"wifi_password", a.wifiPassword},
                    {"hidden", a.hidden ? "1" : "0"},
                    {"login_password", a.loginPassword},
                    {"ssh", a.ssh ? "1" : "0"},
                    {"ssh_key", a.sshKey}},
                   0600);
}

std::vector<std::string> validate(const Apply& a, const std::string& loginConfirm)
{
    std::vector<std::string> problems;
    if (a.ssid.empty() || a.ssid.size() > 32)
        problems.push_back("choose a WiFi network, or type its name (up to 32 characters)");
    if (!a.wifiPassword.empty() && (a.wifiPassword.size() < 8 || a.wifiPassword.size() > 63))
        problems.push_back("a WiFi password is 8 to 63 characters (leave it empty for an open network)");
    if (!a.loginPassword.empty() && a.loginPassword.size() < 8)
        problems.push_back("the login password needs at least 8 characters");
    if (a.loginPassword.find_first_of("\r\n") != std::string::npos)
        problems.push_back("the login password cannot contain a line break");
    if (a.loginPassword != loginConfirm)
        problems.push_back("the two login passwords differ");
    if (!a.sshKey.empty() &&
        !(a.sshKey.starts_with("ssh-") || a.sshKey.starts_with("ecdsa-") || a.sshKey.starts_with("sk-")))
        problems.push_back("an SSH key starts with ssh-, ecdsa- or sk-: paste the public key (the .pub file)");
    if (std::ranges::any_of(a.sshKey, [](unsigned char c) { return c < 0x20 || c == 0x7f; }))
        problems.push_back("paste one SSH key, on one line");
    return problems;
}

} // namespace purr::setup
