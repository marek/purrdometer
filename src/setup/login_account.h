// The account a person logs in with (the first with uid 1000-59999): its
// password, its shell, its SSH keys, and whether sshd runs.
#pragma once

#include "common/setup_form.h"

#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace purr::setup {

class LoginAccount {
public:
    static std::optional<LoginAccount> find();

    const std::string& name() const { return name_; }
    bool setPassword(const std::string& password); // gives a nologin account a shell too
    bool addSshKey(const std::string& key);        // false when it could not be installed; true if already there
    static bool enableSsh();

private:
    std::string name_, home_, shell_;
    uid_t uid_ = 0;
    gid_t gid_ = 0;
};

// The login part of a submitted form: what could not be applied ("login
// password", "SSH key", "SSH on", "login account" when there is none); empty
// when everything asked for was done.
std::vector<std::string> applyLogin(const Apply& a);

} // namespace purr::setup
