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
    e.rotate180 = req.has_param("rotate180");
    number("full_refresh_every", e.fullRefreshEvery);
    number("active_refresh_s", e.activeRefreshS);
    number("idle_refresh_s", e.idleRefreshS);
    e.sensor = optional("sensor");
    if (req.has_param("wake_turns")) // kept as it was when a form leaves it out
        number("wake_turns", e.wakeTurns);
    e.rawLog = req.has_param("raw_log");
    number("raw_log_days", e.rawLogDays);
    return e;
}

std::string diameterForForm(const std::optional<double>& metres, const std::string& units)
{
    if (!metres)
        return "";
    return fixed(units == "in" ? *metres / INCH : *metres * 100, 1);
}

} // namespace purr::web
