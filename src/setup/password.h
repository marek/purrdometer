// The hotspot's password: new each time setup starts, from letters and digits
// that cannot be misread on the e-paper.
#pragma once

#include <string>

namespace purr::setup {

constexpr std::string_view PASSWORD_ALPHABET = "abcdefghjkmnpqrstuvwxyzACDEFGHJKLMNPQRTUVWXY34679";
constexpr int PASSWORD_LENGTH = 10;

std::string newPassword(); // throws std::runtime_error without /dev/urandom

} // namespace purr::setup
