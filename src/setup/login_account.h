// The account a person logs in with (the first with uid 1000-59999): its
// password, its shell, its SSH keys, and whether sshd runs.
#pragma once

#include <optional>
#include <string>
#include <sys/types.h>

namespace purr::setup {

class LoginAccount {
public:
    static std::optional<LoginAccount> find();

    const std::string& name() const { return name_; }
    bool setPassword(const std::string& password); // gives a nologin account a shell too
    bool addSshKey(const std::string& key);
    static void enableSsh();

private:
    std::string name_, home_, shell_;
    uid_t uid_ = 0;
    gid_t gid_ = 0;
};

} // namespace purr::setup
