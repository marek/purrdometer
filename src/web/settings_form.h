// The settings form as posted, into the values it changes, and the stored
// values as the form shows them.
#pragma once

#include "common/config.h"

#include <httplib.h>

namespace purr::web {

// The posted fields over `current`; bad numbers become -1 for validate() to report.
Editable editableFromForm(const httplib::Request& req, Editable current);

// A stored diameter as the form's number field shows it: a bare number in the current units.
Editable forForm(Editable e);

} // namespace purr::web
