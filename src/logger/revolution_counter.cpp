#include "logger/revolution_counter.h"

#include "logger/accounting.h"
#include "logger/log.h"

#include <string>

namespace purr::logger {

RevolutionCounter::RevolutionCounter(Store& store) : store_(store)
{
    auto counter = store_.state("last_counter"), revs = store_.state("last_revs"), t = store_.state("last_event_time"),
         w = store_.state("last_wall");
    if (!revs || !t || !w)
        return;
    Counter c = counter == std::optional<std::string>("wheel") ? Counter::Wheel : Counter::Crank;
    last_ = Sample{c, uint32_t(std::stoul(*revs)), uint16_t(std::stoul(*t))};
    lastWall_ = std::stod(*w);
    log("restored ", counterName(c), " counter at ", last_->revs);
}

void RevolutionCounter::setKind(Counter c)
{
    std::scoped_lock lock(mutex_);
    counter_ = c;
}

void RevolutionCounter::setWakeTurns(int wakeTurns)
{
    std::scoped_lock lock(mutex_);
    missed_.configure(wakeTurns);
}

void RevolutionCounter::setWheel(int profile)
{
    std::scoped_lock lock(mutex_);
    if (profile != wheel_)
        log("wheel profile ", profile, " from now on");
    wheel_ = profile; // the count carries on: no turn is lost or counted twice across a switch
}

void RevolutionCounter::connected(bool firstSinceStart)
{
    std::scoped_lock lock(mutex_);
    missed_.connected(firstSinceStart);
}

void RevolutionCounter::forget()
{
    std::scoped_lock lock(mutex_);
    last_.reset();
    store_.forgetCounter();
}

void RevolutionCounter::onMeasurement(const std::vector<uint8_t>& bytes)
{
    std::scoped_lock lock(mutex_);
    auto cur = parseMeasurement(bytes, counter_);
    if (!cur)
        return;
    double now = nowSeconds();
    Accounting a = account(last_, lastWall_, *cur, now);
    bool restarted = last_ && last_->counter == cur->counter && cur->revs < last_->revs;
    int estimated = missed_.estimate(a.delta, restarted);
    if (a.reset)
        log("counter discontinuity (", last_->revs, " -> ", cur->revs, "): ",
            a.delta ? "the sensor restarted, its " + std::to_string(a.delta) + " added" : "nothing added");
    try {
        store_.record(now, *cur, a.delta, estimated, wheel_);
        if (a.delta > 0)
            store_.setState("last_motion", std::to_string(now));
        if (a.rpm >= 0)
            store_.setState("rpm", std::to_string(int(a.rpm + 0.5)));
    } catch (const std::exception& e) {
        log("storage error: ", e.what());
    }
    if (a.delta > 0)
        log("revs=", cur->revs, " +", a.delta, " rpm=", int(a.rpm + 0.5),
            estimated ? " (+" + std::to_string(estimated) + " the sensor missed)" : "");
    last_ = *cur;
    lastWall_ = now;
}

} // namespace purr::logger
