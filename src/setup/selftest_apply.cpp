// The setup form the root helper takes from a folder the web server writes:
// whatever sits at its path, nothing but a plain form is read, and nothing
// but that form is overwritten.

#include "common/setup_form.h"
#include "setup/selftest.h"

#include <fcntl.h>
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

} // namespace

void selftestApply(purr::Check& check)
{
    fs::path root = fs::path(selftestTmp()) / "apply-selftest";
    fs::remove_all(root);
    fs::create_directories(root);
    const Apply form{.ssid = "Home", .wifiPassword = "longenough", .loginPassword = "longenough"};
    const std::string secret = "ssid=Stolen\nlogin_password=from-a-root-file\n";
    fs::path target = root / "target";
    auto resetTarget = [&] { std::ofstream(target) << secret; };
    fs::path path = root / "apply";

    resetTarget();
    fs::create_symlink(target, path);
    auto viaLink = takeApply(path.string());
    check(!viaLink && slurp(target) == secret && !fs::is_symlink(path),
          "a form that is a link is not read, its target is not overwritten, and the link is removed");

    fs::create_symlink(target, path);
    check(!readApply(path.string()), "a form that is a link is not read by readApply either");
    fs::remove(path);

    fs::path nowhere = root / "created-by-the-helper";
    fs::create_symlink(nowhere, path);
    check(!takeApply(path.string()) && !fs::exists(nowhere) && !fs::is_symlink(path),
          "a link to nowhere creates nothing through it");

    fs::create_hard_link(target, path);
    check(!takeApply(path.string()) && slurp(target) == secret && !fs::exists(path),
          "a second name for another file is refused, and the other file is neither read nor overwritten");
    fs::remove(path);

    fs::create_directory(path);
    check(!takeApply(path.string()) && fs::is_directory(path), "a directory is not read and is left alone");
    fs::remove(path);

    if (mkfifo(path.c_str(), 0600) == 0) {
        check(!takeApply(path.string()) && !fs::exists(path), "a FIFO is not waited on, and is removed");
    } else {
        check(true, "a FIFO is not waited on (skipped: this filesystem makes no FIFOs)");
    }

    std::ofstream(path) << std::string(200000, 'x');
    check(!takeApply(path.string()) && !fs::exists(path), "a file too large to be a form is refused");

    check(!takeApply((root / "missing").string()), "no form, no result");

    writeApply(form, path.string());
    auto peek = readApply(path.string());
    check(peek && peek->ssid == "Home" && fs::exists(path), "a plain form is read without being taken");
    auto taken = takeApply(path.string());
    check(taken && taken->ssid == "Home" && taken->loginPassword == "longenough" && !fs::exists(path),
          "a plain form is read and removed");

    writeApply(form, path.string());
    int fd = open(path.c_str(), O_RDONLY);
    if (fd >= 0) {
        // The passwords are overwritten in place before the name goes: an open descriptor sees zeros.
        takeApply(path.string());
        char buf[64] = {};
        ssize_t n = pread(fd, buf, sizeof buf, 0);
        close(fd);
        check(n <= 0 || std::string(buf, size_t(n)).find("longenough") == std::string::npos,
              "what was in the form is not left where it was written");
    }
    fs::remove_all(root);
}
