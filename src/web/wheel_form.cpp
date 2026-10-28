#include "web/wheel_form.h"

#include <stdexcept>

namespace purr::web {

Profile profileFromForm(const httplib::Request& req, Profile p, const std::string& units,
                        std::vector<std::string>& problems)
{
    p.name = req.get_param_value("name");
    std::string preset = req.get_param_value("preset");
    p.preset = preset.empty() ? std::nullopt : std::optional(toLower(preset));
    std::string diameter = req.get_param_value("diameter");
    p.diameterM.reset();
    if (!diameter.empty()) {
        try {
            p.diameterM = parseDiameter(diameter, units);
        } catch (const std::invalid_argument& e) {
            problems.push_back(e.what());
        }
    }
    return p;
}

} // namespace purr::web
