#include "web/settings_form.h"

#include "common/format.h"
#include "common/wheel.h"

#include <stdexcept>

namespace purr::web {

Editable editableFromForm(const httplib::Request& req, Editable e)
{
    auto optional = [&](const char* key) {
        std::string v = req.get_param_value(key);
        return v.empty() ? std::nullopt : std::optional<std::string>(v);
    };
    auto number = [&](const char* key, int& field) {
        try {
            field = std::stoi(req.get_param_value(key));
        } catch (const std::exception&) {
            field = -1;
        }
    };
    e.units = req.get_param_value("units");
    e.model = req.get_param_value("model");
    e.diameter = optional("diameter");
    e.rotate180 = req.has_param("rotate180");
    number("full_refresh_every", e.fullRefreshEvery);
    number("active_refresh_s", e.activeRefreshS);
    number("idle_refresh_s", e.idleRefreshS);
    e.sensor = optional("sensor");
    if (req.has_param("wake_turns")) // kept as it was when a form leaves it out
        number("wake_turns", e.wakeTurns);
    return e;
}

Editable forForm(Editable e)
{
    if (!e.diameter)
        return e;
    try {
        double m = parseDiameter(*e.diameter, e.units);
        e.diameter = fixed(e.units == "in" ? m / INCH : m * 100, 1);
    } catch (const std::invalid_argument&) {
        // shown as written; the page lists the config error
    }
    return e;
}

} // namespace purr::web
