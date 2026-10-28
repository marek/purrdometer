#include "web/save_lock.h"

namespace purr::web {

namespace {

// One for the process: the files are shared by every route, whichever Site names them.
std::mutex& saves()
{
    static std::mutex m;
    return m;
}

} // namespace

SaveLock::SaveLock() : lock_(saves()) {}

} // namespace purr::web
