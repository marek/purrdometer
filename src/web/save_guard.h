// The checks a save passes before it touches a file (docs/intents/web/settings.md#who-may-save):
// the access rule for this client, the form token against its cookie
// (double submit), and the PIN when one is required.
#pragma once

#include "common/config.h"

#include <httplib.h>

#include <string>
#include <vector>

namespace purr::web {

constexpr const char* TOKEN_COOKIE = "purr_token";

// Whether this client may save, and if not, what to tell it.
bool maySave(const Config& cfg, const std::string& addr, std::string& whyNot);

// The form token for this page: the client's own, or a new one, set as a cookie on `path`.
std::string formToken(const httplib::Request& req, httplib::Response& res, const std::string& path);

// True when the token matches its cookie (and the PIN, when cfg asks for one); otherwise 403 and why.
bool tokenAndPinOk(const httplib::Request& req, httplib::Response& res, const Config& cfg,
                   std::vector<std::string>& problems);

// maySave and tokenAndPinOk together, as every settings save, scan and reset needs.
bool authorised(const httplib::Request& req, httplib::Response& res, const Config& cfg,
                std::vector<std::string>& problems);

} // namespace purr::web
