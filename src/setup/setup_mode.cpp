#include "setup/setup_mode.h"

#include "common/setup_form.h"
#include "setup/password.h"

#include <format>
#include <print>

namespace purr::setup {

namespace {

std::string commaList(const std::vector<std::string>& parts)
{
    std::string out;
    for (auto& p : parts)
        out += (out.empty() ? "" : ", ") + p;
    return out;
}

} // namespace

SetupMode::SetupMode(NetworkManager& wifi, int hotspotMinutes, SetupPaths paths, LoginApplier login)
    : wifi_(wifi), hotspotMinutes_(hotspotMinutes), paths_(std::move(paths)), request_({paths_.request}),
      form_({paths_.apply}), login_(std::move(login))
{
}

void SetupMode::begin(std::time_t now)
{
    now_ = now;
    if (readState(paths_.state).active())
        wifi_.stopHotspot(); // left over from before a restart
    state_ = State{};
    publish();
    checkProfiles();
}

void SetupMode::checkProfiles()
{
    auto has = wifi_.hasWifiProfile();
    if (!has) {
        // A timeout or a NetworkManager still starting is not an unconfigured Pi: the WiFi it has must not be left.
        std::println(stderr, "cannot tell whether a WiFi is configured ({}): asking again in {} s", has.error(), RETRY_S);
        retryAt_ = now_ + RETRY_S;
        return;
    }
    profilesKnown_ = true;
    retryAt_ = 0;
    if (!*has) {
        std::println("no WiFi configured: setup until one is joined");
        untilJoined_ = true;
        start();
    }
}

void SetupMode::tick(std::time_t now)
{
    now_ = now;
    if (retryAt_ && now >= retryAt_) {
        if (state_.phase == Phase::Starting)
            openHotspot(); // no WiFi to go back to, and the hotspot did not come up: again
        else if (!state_.active() && !profilesKnown_)
            checkProfiles();
    }
    if (request_.touched() && !state_.active()) {
        std::println("setup requested");
        start();
    }
    if (form_.touched() && state_.phase == Phase::Hotspot) {
        auto a = takeApply(paths_.apply);
        if (a && validate(*a, a->loginPassword).empty()) {
            apply(*a);
        } else {
            state_.message = "the setup form was not complete";
            publish();
        }
    }
    if (state_.phase == Phase::Hotspot && state_.deadline && now > state_.deadline)
        timeOut();
    if (state_.phase == Phase::Joined && now > state_.until)
        idle();
}

void SetupMode::start()
{
    joinFailure_.clear();
    state_ = State{.phase = Phase::Starting, .ssid = wifi_.hotspotName(), .password = newPassword()};
    publish();
    try {
        writeNetworks(wifi_.scan(), paths_.networks); // before the hotspot: the radio cannot scan while it is one
    } catch (const std::exception& e) {
        std::println(stderr, "cannot publish the networks scanned: {}", e.what()); // the page offers typing a name
    }
    openHotspot();
}

// Publishes the hotspot only once it is up. When it cannot start, the Pi goes back to the WiFi it had;
// with none to go back to, setup shows why and tries again.
bool SetupMode::openHotspot()
{
    auto up = wifi_.startHotspot(state_.ssid, state_.password);
    if (!up) {
        std::string why = (joinFailure_.empty() ? "" : joinFailure_ + "; ") + "the hotspot did not start: " + up.error();
        std::println(stderr, "setup: {}", why);
        if (untilJoined_) {
            state_.phase = Phase::Starting;
            state_.message = why + "; trying again";
            state_.deadline = 0;
            retryAt_ = now_ + RETRY_S;
            publish();
        } else {
            wifi_.reconnect();
            idle();
        }
        return false;
    }
    retryAt_ = 0;
    state_.phase = Phase::Hotspot;
    state_.message = joinFailure_;
    state_.deadline = untilJoined_ ? 0 : now_ + hotspotMinutes_ * 60;
    publish();
    std::println("hotspot {} up", state_.ssid);
    return true;
}

void SetupMode::idle()
{
    state_ = State{};
    publish();
    retryAt_ = profilesKnown_ ? 0 : now_ + RETRY_S; // a Pi whose WiFi could not be listed is asked about again
}

void SetupMode::apply(const Apply& a)
{
    state_.phase = Phase::Joining;
    state_.network = a.ssid;
    state_.message.clear();
    publish();
    wifi_.stopHotspot();

    auto joined = wifi_.join(a);
    if (!joined) {
        joinFailure_ = std::format("could not join {}: {}", a.ssid, joined.error());
        openHotspot();
        return;
    }
    auto failures = login_(a);
    untilJoined_ = false;
    profilesKnown_ = true; // the profile just joined is one
    retryAt_ = 0;
    joinFailure_.clear();
    state_.phase = Phase::Joined;
    state_.message = *joined;
    state_.warning = commaList(failures);
    state_.deadline = 0;
    state_.until = now_ + JOINED_SHOWN_S;
    publish();
    std::println("joined {} as {}{}", a.ssid, *joined, failures.empty() ? "" : "; not applied: " + state_.warning);
}

void SetupMode::timeOut()
{
    std::println("setup unused for {} minutes: back to the WiFi configured", hotspotMinutes_);
    wifi_.stopHotspot();
    wifi_.reconnect();
    idle();
}

void SetupMode::publish()
{
    try {
        writeState(state_, paths_.state);
    } catch (const std::exception& e) {
        std::println(stderr, "cannot publish the setup state: {}", e.what()); // the next change tries again
    }
}

} // namespace purr::setup
