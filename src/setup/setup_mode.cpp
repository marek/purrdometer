#include "setup/setup_mode.h"

#include "common/setup_form.h"
#include "setup/login_account.h"
#include "setup/password.h"

#include <format>
#include <print>

namespace purr::setup {

namespace {

void applyLogin(const Apply& a)
{
    auto account = LoginAccount::find();
    if (!account)
        return;
    if (!a.loginPassword.empty())
        account->setPassword(a.loginPassword);
    if (!a.sshKey.empty())
        account->addSshKey(a.sshKey);
    if (a.ssh)
        LoginAccount::enableSsh();
}

} // namespace

SetupMode::SetupMode(NetworkManager& wifi, int hotspotMinutes, std::string statePath, std::string requestPath,
                     std::string applyPath)
    : wifi_(wifi), hotspotMinutes_(hotspotMinutes), statePath_(std::move(statePath)), applyPath_(applyPath),
      request_({std::move(requestPath)}), form_({std::move(applyPath)})
{
}

void SetupMode::begin()
{
    if (readState(statePath_).active())
        wifi_.stopHotspot(); // left over from before a restart
    state_ = State{};
    publish();
    if (!wifi_.hasWifiProfile()) {
        std::println("no WiFi configured: setup until one is joined");
        untilJoined_ = true;
        start();
    }
}

void SetupMode::tick(std::time_t now)
{
    if (request_.touched() && !state_.active()) {
        std::println("setup requested");
        start();
    }
    if (form_.touched() && state_.phase == Phase::Hotspot) {
        auto a = takeApply(applyPath_);
        if (a && validate(*a, a->loginPassword).empty()) {
            apply(*a);
        } else {
            state_.message = "the setup form was not complete";
            publish();
        }
    }
    if (state_.phase == Phase::Hotspot && state_.deadline && now > state_.deadline)
        timeOut();
    if (state_.phase == Phase::Joined && now > state_.until) {
        state_ = State{};
        publish();
    }
}

void SetupMode::start()
{
    state_ = State{.phase = Phase::Starting, .ssid = wifi_.hotspotName(), .password = newPassword()};
    publish();
    writeNetworks(wifi_.scan()); // before the hotspot: the radio cannot scan while it is one
    openHotspot(std::time(nullptr));
}

void SetupMode::openHotspot(std::time_t now)
{
    wifi_.startHotspot(state_.ssid, state_.password);
    state_.phase = Phase::Hotspot;
    state_.deadline = untilJoined_ ? 0 : now + hotspotMinutes_ * 60;
    publish();
    std::println("hotspot {} up", state_.ssid);
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
        state_.message = std::format("could not join {}: {}", a.ssid, joined.error());
        openHotspot(std::time(nullptr));
        return;
    }
    applyLogin(a);
    untilJoined_ = false;
    state_.phase = Phase::Joined;
    state_.message = *joined;
    state_.deadline = 0;
    state_.until = std::time(nullptr) + JOINED_SHOWN_S;
    publish();
    std::println("joined {} as {}", a.ssid, *joined);
}

void SetupMode::timeOut()
{
    std::println("setup unused for {} minutes: back to the WiFi configured", hotspotMinutes_);
    wifi_.stopHotspot();
    wifi_.reconnect();
    state_ = State{};
    publish();
}

void SetupMode::publish() { writeState(state_, statePath_); }

} // namespace purr::setup
