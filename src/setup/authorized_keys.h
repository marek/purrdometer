// A public key into an account's ~/.ssh/authorized_keys, from a root process
// (docs/intents/network/setup.md#the-helper). The account owns its home and
// everything under it, so nothing below the home is trusted: no link is
// followed, the file must be an ordinary one nobody else has a name for, and
// ownership and modes are set on the open descriptors, never on paths.
#pragma once

#include <expected>
#include <string>
#include <sys/types.h>

namespace purr::setup {

// Creates ~/.ssh (0700) and authorized_keys (0600), both owned by uid:gid, as
// needed and adds `key` unless its key is already there under any comment.
// Returns whether it was added; an error says what was refused or failed, and
// then nothing was appended.
std::expected<bool, std::string> addAuthorizedKey(const std::string& home, uid_t uid, gid_t gid,
                                                  const std::string& key);

} // namespace purr::setup
