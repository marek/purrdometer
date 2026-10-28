#include "logger/accounting.h"

#include <algorithm>
#include <cmath>

namespace purr::logger {

Accounting account(std::optional<Sample> last, double lastWall, Sample cur, double nowWall)
{
    Accounting a;
    if (!last || last->counter != cur.counter)
        return a; // a baseline: the first sample, or another kind of counter
    const uint32_t dRevs = cur.counter == Counter::Wheel ? cur.revs - last->revs : uint16_t(cur.revs - last->revs);
    const double elapsed = std::max(0.0, nowWall - lastWall);
    const double plausible = std::ceil(elapsed * MAX_REVS_PER_SECOND) + 2;
    if (dRevs > plausible) {
        a.reset = true; // sensor rebooted / battery swap / garbage
        return a;
    }
    a.delta = dRevs;
    const uint16_t dTime = uint16_t(cur.eventTime - last->eventTime);
    if (dRevs > 0 && dTime > 0 && elapsed < 60)
        a.rpm = dRevs * 60.0 * 1024.0 / dTime;
    else if (dRevs == 0)
        a.rpm = 0;
    return a;
}

} // namespace purr::logger
