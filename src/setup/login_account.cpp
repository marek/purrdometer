#include "setup/login_account.h"

#include "setup/authorized_keys.h"
#include "setup/command.h"

#include <print>
#include <pwd.h>

namespace purr::setup {

std::optional<LoginAccount> LoginAccount::find()
{
    std::optional<LoginAccount> found;
    setpwent();
    while (passwd* p = getpwent()) {
        if (p->pw_uid < 1000 || p->pw_uid >= 60000)
            continue;
        found.emplace();
        found->name_ = p->pw_name;
        found->home_ = p->pw_dir;
        found->shell_ = p->pw_shell;
        found->uid_ = p->pw_uid;
        found->gid_ = p->pw_gid;
        break;
    }
    endpwent();
    return found;
}

bool LoginAccount::setPassword(const std::string& password)
{
    // chpasswd reads "name:password" lines: a line break would be another account's.
    if (password.find_first_of("\r\n") != std::string::npos || password.find('\0') != std::string::npos) {
        std::println(stderr, "login password for {}: refused, it has a line break", name_);
        return false;
    }
    auto r = runCommand({"chpasswd"}, name_ + ":" + password + "\n");
    std::println("login password for {}: {}", name_, r.ok() ? "set" : r.why());
    if (!r.ok())
        return false;
    if (shell_.ends_with("nologin")) {
        auto shell = runCommand({"usermod", "-s", "/bin/bash", name_});
        if (!shell.ok())
            std::println(stderr, "cannot give {} a shell: {}", name_, shell.why());
        return shell.ok();
    }
    return true;
}

bool LoginAccount::addSshKey(const std::string& key)
{
    auto added = addAuthorizedKey(home_, uid_, gid_, key);
    if (!added) {
        std::println(stderr, "cannot give {} its SSH key: {}", name_, added.error());
        return false;
    }
    std::println("SSH key for {}: {}", name_, *added ? "added" : "already there");
    return true;
}

bool LoginAccount::enableSsh()
{
    auto r = runCommand({"systemctl", "enable", "--now", "ssh"});
    if (!r.ok())
        std::println(stderr, "cannot turn SSH on: {}", r.why());
    return r.ok();
}

std::vector<std::string> applyLogin(const Apply& a)
{
    std::vector<std::string> failures;
    if (a.loginPassword.empty() && a.sshKey.empty() && !a.ssh)
        return failures;
    auto account = LoginAccount::find();
    if (!account) {
        failures.push_back("login account");
        return failures;
    }
    if (!a.loginPassword.empty() && !account->setPassword(a.loginPassword))
        failures.push_back("login password");
    if (!a.sshKey.empty() && !account->addSshKey(a.sshKey))
        failures.push_back("SSH key");
    if (a.ssh && !LoginAccount::enableSsh())
        failures.push_back("SSH on");
    return failures;
}

} // namespace purr::setup
