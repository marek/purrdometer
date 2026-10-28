// Overlapping saves: two requests at once, as the server's two workers take
// them, and files replaced under a reader.

#include "common/atomic_file.h"
#include "common/config.h"
#include "common/profiles.h"
#include "web/save_guard.h"
#include "web/save_lock.h"
#include "web/selftest.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace purr;
namespace fs = std::filesystem;

namespace {

constexpr size_t FILE_BYTES = 100000;

std::string slurp(const std::string& path)
{
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Files in the folder that are not `keep`: the leftovers of a write.
int strays(const fs::path& dir, const std::string& prefix, const std::set<std::string>& keep)
{
    int n = 0;
    for (auto& e : fs::directory_iterator(dir)) {
        std::string name = e.path().filename().string();
        n += name.starts_with(prefix) && !keep.contains(name);
    }
    return n;
}

} // namespace

void selftestWrites(Fixture& f)
{
    Check& check = f.check;
    const std::string prefix = std::string(web::TOKEN_COOKIE) + "=";

    // The same file replaced by many writers while a reader reads it: never a partial file, never a failure.
    {
        std::string path = f.tmpDir + "/selftest-replaced.txt";
        auto hammer = [&](auto&& replace) {
            std::remove(path.c_str());
            replaceFile(path, std::string(FILE_BYTES, 'a'));
            std::atomic<bool> stop{false};
            std::atomic<int> torn{0}, failed{0};
            std::thread reader([&] {
                while (!stop) {
                    std::string text = slurp(path);
                    if (text.size() != FILE_BYTES || text.find_first_not_of(text[0]) != std::string::npos)
                        ++torn;
                }
            });
            std::vector<std::thread> writers;
            for (char c : std::string("bcdefgh"))
                writers.emplace_back([&, c] {
                    for (int i = 0; i < 100; ++i) {
                        try {
                            replace(std::string(FILE_BYTES, c), c);
                        } catch (const std::exception&) {
                            ++failed;
                        }
                    }
                });
            for (auto& w : writers)
                w.join();
            stop = true;
            reader.join();
            return std::pair{torn.load(), failed.load()};
        };
        // What the filesystem does by itself: files written whole elsewhere, then renamed over.
        auto [fsTorn, fsFailed] = hammer([&](const std::string& text, char c) {
            std::string own = path + ".own-" + c;
            std::ofstream(own) << text;
            if (std::rename(own.c_str(), path.c_str()) != 0)
                throw std::runtime_error("rename");
        });
        auto [torn, failed] = hammer([&](const std::string& text, char) { replaceFile(path, text); });
        check(failed == 0 && slurp(path).size() == FILE_BYTES, "a file replaced by many writers is never lost");
        if (fsTorn == 0 && fsFailed == 0)
            check(torn == 0, "and is whole every time it is read");
        else
            check(true, "and is whole every time it is read (skipped: this filesystem tears reads of a renamed file)");
        check(strays(f.tmpDir, "selftest-replaced.txt.", {}) == 0, "and no temporary file is left");
        std::remove(path.c_str());
        bool refused = false;
        try {
            replaceFile(f.tmpDir + "/no-such-folder/file", "x");
        } catch (const std::runtime_error&) {
            refused = true;
        }
        check(refused, "a file that cannot be written is an error, not a silent loss");
        fs::path inTheWay = fs::path(f.tmpDir) / "selftest-in-the-way";
        fs::create_directories(inTheWay);
        bool notReplaced = false;
        try {
            replaceFile(inTheWay.string(), "x");
        } catch (const std::runtime_error&) {
            notReplaced = true;
        }
        check(notReplaced && fs::is_directory(inTheWay) && strays(f.tmpDir, "selftest-in-the-way.", {}) == 0,
              "a file that cannot be renamed into place leaves nothing beside it");
        fs::remove(inTheWay);
    }

    // Overlapping requests through the server: none lost, none refused.
    std::ofstream(f.conf) << "[wheel]\nmodel = ferris-g5\n";
    std::remove(f.prof.c_str());
    std::remove(f.over.c_str());
    httplib::Client first("127.0.0.1", f.port);
    httplib::Result page;
    for (int i = 0; i < 50 && !(page = first.Get("/settings")); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string cookie = page ? page->get_header_value("Set-Cookie") : "";
    std::string token = cookie.starts_with(prefix) ? cookie.substr(prefix.size(), 32) : "";
    httplib::Headers withCookie = {{"Cookie", prefix + token}};
    constexpr int EACH = 12;
    std::atomic<int> refused{0}, wrong{0};
    auto addWheels = [&](const std::string& who) {
        httplib::Client client("127.0.0.1", f.port);
        for (int i = 0; i < EACH; ++i) {
            auto r = client.Post("/settings/wheels", withCookie,
                                 httplib::Params{{"token", token}, {"do", "new"}, {"name", std::format("{} {}", who, i)},
                                                 {"preset", "ferris-g5"}, {"diameter", ""}});
            if (!r || r->status != 303 || r->get_header_value("Location").find("wheel_saved") == std::string::npos)
                ++refused;
        }
    };
    auto saveSettings = [&] {
        httplib::Client client("127.0.0.1", f.port);
        for (int i = 0; i < EACH; ++i) {
            auto r = client.Post("/settings", withCookie,
                                 httplib::Params{{"token", token}, {"units", "cm"}, {"active_refresh_s", "5"},
                                                 {"idle_refresh_s", "60"}, {"full_refresh_every", "20"},
                                                 {"wake_turns", std::to_string(i % 10)}, {"raw_log_days", "2"}});
            if (!r || r->status != 303)
                ++wrong;
        }
    };
    std::thread a(addWheels, "A"), b(addWheels, "B"), c(saveSettings);
    a.join();
    b.join();
    c.join();
    Config after = f.srv.config();
    std::set<std::string> names;
    std::set<int> ids;
    for (auto& p : after.profiles.list) {
        names.insert(p.name);
        ids.insert(p.id);
    }
    check(page && token.size() == 32 && refused == 0 && wrong == 0, "overlapping saves are all accepted");
    check(after.profiles.list.size() == 1 + 2 * EACH && names.size() == size_t(1 + 2 * EACH) &&
              ids.size() == size_t(1 + 2 * EACH) && after.errors.empty(),
          "two requests adding wheels at once lose none and give no two the same number");
    check(after.overridden && after.editable.units == "cm" && after.editable.wakeTurns == (EACH - 1) % 10,
          "settings saved alongside them are whole");
    check(strays(fs::path(f.prof).parent_path(), fs::path(f.prof).filename().string() + ".", {}) == 0 &&
              strays(fs::path(f.over).parent_path(), fs::path(f.over).filename().string() + ".", {}) == 0,
          "no temporary file is left beside them");

    // A save that waits for another reads what its form leaves out only once it may write: here the
    // other one changes wake_turns while this one waits, and this one's form does not mention it.
    {
        auto held = std::make_unique<web::SaveLock>();
        std::atomic<int> status{0};
        std::thread waiting([&] {
            httplib::Client client("127.0.0.1", f.port);
            auto r = client.Post("/settings", withCookie,
                                 httplib::Params{{"token", token}, {"units", "in"}, {"active_refresh_s", "5"},
                                                 {"idle_refresh_s", "60"}, {"full_refresh_every", "20"},
                                                 {"raw_log_days", "2"}});
            status = r ? r->status : -1;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        Editable other = loadEditable(f.conf, f.over);
        other.wakeTurns = 9;
        saveOverrides(other, f.over);
        held.reset();
        waiting.join();
        Editable now = loadEditable(f.conf, f.over);
        check(status == 303 && now.wakeTurns == 9 && now.units == "in",
              "a save that waited does not write back what it read before the other one saved");
    }
}
