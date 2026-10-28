// The SSH key the root helper installs for the login account, against homes
// the account has laid out to mislead it.

#include "setup/authorized_keys.h"
#include "setup/selftest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace purr::setup;
namespace fs = std::filesystem;

namespace {

std::string slurp(const fs::path& p)
{
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

mode_t modeOf(const fs::path& p)
{
    struct stat st{};
    return stat(p.c_str(), &st) == 0 ? st.st_mode & 07777 : 0;
}

} // namespace

void selftestAuthorizedKeys(purr::Check& check)
{
    const uid_t me = geteuid();
    const gid_t myGroup = getegid();
    const std::string key = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIexample me@laptop";
    fs::path root = fs::path(selftestTmp()) / "authorized-keys-selftest";
    fs::remove_all(root);
    auto home = [&](const char* name) {
        fs::create_directories(root / name);
        return root / name;
    };
    auto add = [&](const fs::path& h, const std::string& k = "") {
        return addAuthorizedKey(h.string(), me, myGroup, k.empty() ? key : k);
    };

    fs::path h = home("plain");
    auto first = add(h);
    check(first && *first && slurp(h / ".ssh/authorized_keys") == key + "\n" && modeOf(h / ".ssh") == 0700 &&
              modeOf(h / ".ssh/authorized_keys") == 0600,
          "a first key makes .ssh (0700) and authorized_keys (0600)");
    auto again = add(h, "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIexample another-comment");
    check(again && !*again && slurp(h / ".ssh/authorized_keys") == key + "\n", "the same key under another comment is not added twice");
    auto other = add(h, "ssh-rsa AAAAB3Other key2");
    check(other && *other && slurp(h / ".ssh/authorized_keys") == key + "\nssh-rsa AAAAB3Other key2\n", "another key is appended");

    h = home("loose");
    fs::create_directories(h / ".ssh");
    chmod((h / ".ssh").c_str(), 0777);
    std::ofstream(h / ".ssh/authorized_keys") << "ssh-rsa AAAAB3Old old";
    chmod((h / ".ssh/authorized_keys").c_str(), 0666);
    auto loose = add(h);
    check(loose && *loose && slurp(h / ".ssh/authorized_keys") == "ssh-rsa AAAAB3Old old\n" + key + "\n" &&
              modeOf(h / ".ssh") == 0700 && modeOf(h / ".ssh/authorized_keys") == 0600,
          "modes are tightened, and a last line without its newline is kept apart from the new key");

    h = home("lookalikes");
    fs::create_directories(h / ".ssh");
    std::ofstream(h / ".ssh/authorized_keys")
        << "# ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIexample commented out\n"
           "ssh-rsa AAAAB3Other AAAAC3NzaC1lZDI1NTE5AAAAIexample\n"
           "from=\"10.0.0.1, other host\" ssh-rsa AAAAB3Third a comment\n";
    auto lookalike = add(h);
    check(lookalike && *lookalike, "a key that is only in a comment, or in another key's comment, is not 'already there'");
    auto optioned = add(h, "ssh-rsa AAAAB3Third newer-comment");
    check(optioned && !*optioned, "a key listed with options in front is found by its type and key");
    auto sameBlobOtherType = add(h, "ssh-dss AAAAB3Third");
    check(sameBlobOtherType && *sameBlobOtherType, "the type is part of the key's identity");
    check(!add(h, "command=\"/bin/sh\" ssh-rsa AAAAB3Fourth"), "a submitted key with options in front is refused");

    // Somewhere the helper must never write: a directory and a file of the kind root owns.
    fs::path target = root / "elsewhere";
    fs::create_directories(target);
    chmod(target.c_str(), 0755);
    std::ofstream(target / "secret") << "untouched\n";
    chmod((target / "secret").c_str(), 0640);

    h = home("ssh-is-a-link");
    fs::create_directory_symlink(target, h / ".ssh");
    auto viaDir = add(h);
    check(!viaDir && !fs::exists(target / "authorized_keys") && modeOf(target) == 0755,
          "a .ssh that is a link is refused; its target is not written or changed");

    h = home("file-is-a-link");
    fs::create_directories(h / ".ssh");
    fs::create_symlink(target / "secret", h / ".ssh/authorized_keys");
    auto viaFile = add(h);
    check(!viaFile && slurp(target / "secret") == "untouched\n" && modeOf(target / "secret") == 0640,
          "an authorized_keys that is a link is refused; its target is not written or changed");

    h = home("file-is-a-dangling-link");
    fs::create_directories(h / ".ssh");
    fs::create_symlink(target / "created-by-the-helper", h / ".ssh/authorized_keys");
    check(!add(h) && !fs::exists(target / "created-by-the-helper"), "a link to nowhere is not created through");

    h = home("file-has-two-names");
    fs::create_directories(h / ".ssh");
    std::ofstream(target / "twin") << "twin\n";
    fs::create_hard_link(target / "twin", h / ".ssh/authorized_keys");
    check(!add(h) && slurp(target / "twin") == "twin\n", "an authorized_keys with another name is refused");

    h = home("file-is-a-fifo");
    fs::create_directories(h / ".ssh");
    if (mkfifo((h / ".ssh/authorized_keys").c_str(), 0600) == 0)
        check(!add(h), "a FIFO in its place is refused, not waited on");
    else
        check(true, "a FIFO in its place is refused (skipped: this filesystem makes no FIFOs)");

    h = home("file-is-a-directory");
    fs::create_directories(h / ".ssh/authorized_keys");
    check(!add(h), "a directory in its place is refused");

    h = home("ssh-is-a-file");
    std::ofstream(h / ".ssh") << "not a directory\n";
    check(!add(h) && slurp(h / ".ssh") == "not a directory\n", "a .ssh that is a file is refused");

    h = home("not-theirs");
    auto notTheirs = addAuthorizedKey(h.string(), me + 1, myGroup, key);
    check(!notTheirs && !fs::exists(h / ".ssh"), "a home the account does not own is refused");
    check(!addAuthorizedKey((root / "no-such-home").string(), me, myGroup, key), "a missing home is an error");

    h = home("not-a-key");
    check(!add(h, "ssh-ed25519 AAAA\nssh-rsa BBBB") && !add(h, "ssh-ed25519") && !add(h, "ssh-ed25519 AAAA\rcommand=x") &&
              !fs::exists(h / ".ssh/authorized_keys") && !addAuthorizedKey(h.string(), me, myGroup, ""),
          "a key that is not one line of type and key is refused");

    h = home("big");
    fs::create_directories(h / ".ssh");
    std::ofstream(h / ".ssh/authorized_keys") << std::string(2 << 20, '#');
    check(!add(h), "a list too large to be keys is refused");

    h = home("owned");
    struct stat probe{};
    bool chowns = me == 0 && chown(h.c_str(), 0, 12345) == 0 && stat(h.c_str(), &probe) == 0 && probe.st_gid == 12345;
    if (chowns) {
        auto owned = addAuthorizedKey(h.string(), 0, 12345, key);
        struct stat dir{}, file{};
        stat((h / ".ssh").c_str(), &dir);
        stat((h / ".ssh/authorized_keys").c_str(), &file);
        check(owned && dir.st_gid == 12345 && file.st_gid == 12345, "the account's group owns what was made");
    } else {
        check(true, "the account's group owns what was made (skipped: needs root on a filesystem that keeps owners)");
    }
    fs::remove_all(root);
}
