#include "web/wheel_routes.h"

#include "common/calendar.h"
#include "common/format.h"
#include "common/stats.h"
#include "web/html.h"
#include "web/save_guard.h"
#include "web/save_lock.h"
#include "web/wheel_form.h"

#include <algorithm>
#include <ctime>
#include <print>

namespace purr::web {

namespace {

// The distance a profile's rotations come to on its wheel.
double profileKm(const Stats& st, int id, const Wheel& wheel)
{
    auto it = st.profileRevs.find(id);
    return it == st.profileRevs.end() ? 0 : double(it->second) * wheel.circumferenceM() / 1000;
}

void refuse(httplib::Response& res, const std::vector<std::string>& problems)
{
    std::string why;
    for (auto& p : problems)
        why += (why.empty() ? "" : "; ") + p;
    res.set_redirect("/settings?wheel_error=" + urlEncode(why) + "#wheels", 303);
}

} // namespace

void ensureProfiles(const Site& site)
{
    Config cfg = site.config();
    if (!cfg.profilesSeeded)
        return;
    writeProfiles(cfg.profiles, site.paths.profiles);
    std::println("wheel profiles written to {}: {}", site.paths.profiles, cfg.profiles.list.front().name);
}

void addWheelRoutes(const Site& site, httplib::Server& server)
{
    server.Post("/settings/wheels", [&](const httplib::Request& req, httplib::Response& res) {
        std::vector<std::string> problems;
        if (!authorised(req, res, site.config(), problems))
            return refuse(res, problems);
        SaveLock saving; // from reading the profiles to writing them back
        ensureProfiles(site);
        Config cfg = site.config();
        Profiles profiles = cfg.profiles;
        std::string action = req.get_param_value("do");
        int id = std::atoi(req.get_param_value("id").c_str());
        const Profile* existing = profiles.find(id);
        Stats st = loadStats(site.paths.db, cal::localDay(std::time(nullptr)), circumferences(cfg));
        std::string notice;

        if (action == "new" || action == "edit") {
            if (action == "edit" && !existing)
                return refuse(res, {"that wheel no longer exists"});
            Profile p = profileFromForm(req, existing ? *existing : Profile{.id = profiles.nextId()},
                                        cfg.editable.units, problems);
            auto invalid = validate(p, site.presets);
            problems.insert(problems.end(), invalid.begin(), invalid.end());
            if (!problems.empty())
                return refuse(res, problems);
            Wheel after = wheelOf(p, site.presets);
            if (action == "new") {
                profiles.list.push_back(p);
                profiles.current = p.id;
                notice = p.name + " is in use: rotations from now on are its";
            } else {
                double before = profileKm(st, p.id, cfg.wheels.at(p.id)), now = profileKm(st, p.id, after);
                *std::ranges::find(profiles.list, p.id, &Profile::id) = p;
                notice = std::format("{} saved: {} km → {} km", p.name, km(before), km(now));
            }
        } else if (action == "use" && existing) {
            profiles.current = id;
            notice = existing->name + " is in use: rotations from now on are its";
        } else if (action == "delete" && existing) {
            if (id == profiles.current || st.profileRevs.contains(id))
                return refuse(res, {existing->name + " has rotations or is in use, so it stays"});
            notice = existing->name + " deleted";
            std::erase_if(profiles.list, [&](const Profile& p) { return p.id == id; });
        } else {
            return refuse(res, {"unknown wheel action"});
        }
        writeProfiles(profiles, site.paths.profiles);
        std::println("wheels from {}: {}", req.remote_addr, notice);
        res.set_redirect("/settings?wheel_saved=" + urlEncode(notice) + "#wheels", 303);
    });
}

} // namespace purr::web
