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
    // More than half the counter's range forward is the count going back: a
    // restarted sensor counting from 0, however long it slept (the time alone
    // would let 10 -> 5 pass as 65,531 turns after a night).
    const uint32_t half = cur.counter == Counter::Wheel ? 0x80000000u : 0x8000u;
    if (dRevs > half || dRevs > plausible) {
        // A restarted sensor counts from 0: what it shows now was turned since
        // the restart. A forward jump leaves cur.revs above the limit too, and adds nothing.
        a.reset = true;
        if (cur.revs <= plausible)
            a.delta = cur.revs;
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
