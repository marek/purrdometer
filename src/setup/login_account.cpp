#include "setup/login_account.h"

#include "setup/command.h"

#include <fstream>
#include <print>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

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
    auto r = runCommand({"chpasswd"}, name_ + ":" + password + "\n");
    std::println("login password for {}: {}", name_, r.ok() ? "set" : r.why());
    if (r.ok() && shell_.ends_with("nologin"))
        runCommand({"usermod", "-s", "/bin/bash", name_});
    return r.ok();
}

bool LoginAccount::addSshKey(const std::string& key)
{
    std::string dir = home_ + "/.ssh", file = dir + "/authorized_keys";
    mkdir(dir.c_str(), 0700);
    if (!(std::ofstream(file, std::ios::app) << key << '\n'))
        return false;
    chmod(file.c_str(), 0600);
    bool owned = chown(dir.c_str(), uid_, gid_) == 0 && chown(file.c_str(), uid_, gid_) == 0;
    if (!owned)
        std::println(stderr, "cannot give {} its SSH key", name_);
    return owned;
}

void LoginAccount::enableSsh() { runCommand({"systemctl", "enable", "--now", "ssh"}); }

} // namespace purr::setup
