// Held while a request changes the settings files (docs/intents/web/settings.md#saving).
// The server answers two requests at once, and a change to the wheels reads
// the file, edits it and writes it back: without one lock over the whole
// change, two overlapping requests could each write their edit over the
// other's. Authorise first, then take the lock and read what is there.
#pragma once

#include <mutex>

namespace purr::web {

class SaveLock {
public:
    SaveLock();

private:
    std::unique_lock<std::mutex> lock_;
};

} // namespace purr::web
