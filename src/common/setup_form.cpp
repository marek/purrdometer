#include "common/setup_form.h"

#include "common/kv_file.h"

#include <cstdio>
#include <fstream>

namespace purr::setup {

std::optional<Apply> readApply(const std::string& path)
{
    auto kv = readKeyValues(path);
    auto get = [&](const char* key) {
        auto it = kv.find(key);
        return it == kv.end() ? std::string() : it->second;
    };
    if (get("ssid").empty())
        return std::nullopt;
    return Apply{get("ssid"), get("wifi_password"), get("hidden") == "1", get("login_password"), get("ssh") == "1",
                 get("ssh_key")};
}

std::optional<Apply> takeApply(const std::string& path)
{
    auto a = readApply(path);
    if (std::ofstream wipe{path, std::ios::in | std::ios::out}) // overwritten before it goes
        wipe << std::string(4096, '\0');
    std::remove(path.c_str());
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
    if (a.loginPassword != loginConfirm)
        problems.push_back("the two login passwords differ");
    if (!a.sshKey.empty() &&
        !(a.sshKey.starts_with("ssh-") || a.sshKey.starts_with("ecdsa-") || a.sshKey.starts_with("sk-")))
        problems.push_back("an SSH key starts with ssh-, ecdsa- or sk-: paste the public key (the .pub file)");
    if (a.sshKey.find('\n') != std::string::npos)
        problems.push_back("paste one SSH key, on one line");
    return problems;
}

} // namespace purr::setup
