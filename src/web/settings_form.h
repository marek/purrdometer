// The settings form as posted, into the values it changes, and a profile's
// diameter as the wheel forms show it.
#pragma once

#include "common/config.h"

#include <httplib.h>

namespace purr::web {

// The posted fields over `current`; bad numbers become -1 for validate() to report.
Editable editableFromForm(const httplib::Request& req, Editable current);

// A profile's measured diameter as the form's number field shows it: a bare number in `units`.
std::string diameterForForm(const std::optional<double>& metres, const std::string& units);

} // namespace purr::web
