#include "web/save_guard.h"

#include "web/access.h"
#include "web/cookies.h"

#include <chrono>
#include <thread>

namespace purr::web {

bool maySave(const Config& cfg, const std::string& addr, std::string& whyNot)
{
    switch (cfg.access) {
    case SettingsAccess::Open:
    case SettingsAccess::Pin:
        return true;
    case SettingsAccess::Lan:
        if (isLanAddress(addr))
            return true;
        whyNot = "Saving is allowed from the home network only, and " + addr + " is not on it.";
        return false;
    case SettingsAccess::Off:
        whyNot = "Saving from the web is off ([web] settings_access = off): edit "
                 "/etc/purrdometer/purrdometer.conf on the Pi.";
        return false;
    }
    return false;
}

std::string formToken(const httplib::Request& req, httplib::Response& res, const std::string& path)
{
    std::string token = cookie(req, TOKEN_COOKIE);
    if (token.size() != 32)
        token = newToken();
    res.set_header("Set-Cookie", std::string(TOKEN_COOKIE) + "=" + token + "; Path=" + path + "; SameSite=Strict; HttpOnly");
    return token;
}

bool tokenAndPinOk(const httplib::Request& req, httplib::Response& res, const Config& cfg,
                   std::vector<std::string>& problems)
{
    if (!sameSecret(cookie(req, TOKEN_COOKIE), req.get_param_value("token"))) {
        res.status = 403;
        problems.push_back("the form had expired; reload the page and try again");
        return false;
    }
    if (cfg.access == SettingsAccess::Pin && !sameSecret(cfg.pin, req.get_param_value("pin"))) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // slows guessing
        res.status = 403;
        problems.push_back("wrong PIN");
        return false;
    }
    return true;
}

bool authorised(const httplib::Request& req, httplib::Response& res, const Config& cfg,
                std::vector<std::string>& problems)
{
    std::string whyNot;
    if (!maySave(cfg, req.remote_addr, whyNot)) {
        res.status = 403;
        problems.push_back(whyNot);
        return false;
    }
    return tokenAndPinOk(req, res, cfg, problems);
}

} // namespace purr::web
