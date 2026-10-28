// Reading a cookie from a request.
#pragma once

#include <httplib.h>

#include <string>

namespace purr::web {

std::string cookie(const httplib::Request& req, const std::string& name); // empty when absent

} // namespace purr::web
