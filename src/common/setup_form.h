// What the setup page submits for the helper to apply, and its checks
// (docs/intents/network/setup.md#the-setup-page).
#pragma once

#include "common/paths.h"

#include <optional>
#include <string>
#include <vector>

namespace purr::setup {

constexpr const char* APPLY_PATH = SETUP_APPLY_PATH; // written by the page, read by the helper

struct Apply {
    std::string ssid{}, wifiPassword{};
    bool hidden = false;            // typed in rather than picked from the scan
    std::string loginPassword{};    // empty: unchanged
    bool ssh = false;
    std::string sshKey{};           // empty: none added
};

std::optional<Apply> readApply(const std::string& path = APPLY_PATH);
// The helper's way in: reads the form and removes it from disk at once, since it holds passwords. The path
// is in a folder the unprivileged web server writes, so it is opened without following a link and only a
// regular file with one name is read; the passwords are overwritten through that descriptor, and a path
// that is anything else is neither read nor written, only unlinked if it can be.
std::optional<Apply> takeApply(const std::string& path = APPLY_PATH);
void writeApply(const Apply& a, const std::string& path = APPLY_PATH); // 0600: it holds passwords

// The problems with a submitted form; empty when it can be applied.
std::vector<std::string> validate(const Apply& a, const std::string& loginConfirm);

} // namespace purr::setup
