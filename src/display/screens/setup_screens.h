// The screens of setup mode (docs/intents/network/setup.md#the-e-paper):
// the boot window's "hold to set up WiFi", "starting", the hotspot with its QR
// code, joining, and joined.
#pragma once

#include "common/setup_state.h"
#include "display/canvas.h"

#include <ctime>
#include <optional>
#include <string>

namespace purr::display {

// The first seconds after start; hold: a hold's progress (0..1).
Canvas renderBoot(Fonts& fonts, std::optional<double> hold);

// "Starting WiFi setup…", while the helper starts the hotspot.
Canvas renderStarting(Fonts& fonts);
// The same when the hotspot did not come up: why, and that setup tries again.
Canvas renderStarting(Fonts& fonts, const std::string& why);

// Setup mode, from the helper's state; webPort: the dashboard's, for the address shown once joined.
Canvas renderSetup(Fonts& fonts, const setup::State& state, std::time_t now, int webPort = 80);

} // namespace purr::display
