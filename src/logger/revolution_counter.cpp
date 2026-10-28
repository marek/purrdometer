#include "logger/revolution_counter.h"

#include "logger/accounting.h"
#include "logger/log.h"

#include <string>

namespace purr::logger {
namespace {
RawLog& disabledRawLog()
{
    static RawLog log("");
    return log;
}
} // namespace

RevolutionCounter::RevolutionCounter(Store& store) : RevolutionCounter(store, disabledRawLog()) {}

RevolutionCounter::RevolutionCounter(Store& store, RawLog& rawLog) : store_(store), rawLog_(rawLog)
{
    auto counter = store_.state("last_counter"), revs = store_.state("last_revs"), t = store_.state("last_event_time"),
         w = store_.state("last_wall");
    if (!revs || !t || !w)
        return;
    Counter c = counter == std::optional<std::string>("wheel") ? Counter::Wheel : Counter::Crank;
    last_ = Sample{c, uint32_t(std::stoul(*revs)), uint16_t(std::stoul(*t))};
    lastWall_ = std::stod(*w);
    log("restored ", counterName(c), " counter at ", last_->revs);
    rawLog_.event("counter restored", std::string(counterName(c)) + " revs=" + std::to_string(last_->revs));
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
    if (profile != wheel_) {
        log("wheel profile ", profile, " from now on");
        rawLog_.event("wheel profile", std::to_string(profile));
    }
    wheel_ = profile; // the count carries on: no turn is lost or counted twice across a switch
}

void RevolutionCounter::connected(bool firstSinceStart)
{
    std::scoped_lock lock(mutex_);
    awaitingSensor_ = false;
    missed_.connected(firstSinceStart);
}

void RevolutionCounter::forget()
{
    std::scoped_lock lock(mutex_);
    store_.forgetCounter(); // first: the count in memory goes only once the stored one has
    last_.reset();
    awaitingSensor_ = true;
}

void RevolutionCounter::onMeasurement(const std::vector<uint8_t>& bytes)
{
    std::scoped_lock lock(mutex_);
    if (awaitingSensor_)
        return; // the old sensor's last notifications, still arriving
    auto cur = parseMeasurement(bytes, counter_);
    if (!cur) {
        rawLog_.measurement(bytes, std::nullopt, Accounting{}, 0);
        return;
    }
    double now = nowSeconds();
    Accounting a = account(last_, lastWall_, *cur, now);
    bool restarted = last_ && last_->counter == cur->counter && cur->revs < last_->revs;
    int estimated = missed_.estimate(a.delta, restarted);
    rawLog_.measurement(bytes, cur, a, estimated);
    std::optional<int> rpm;
    if (a.rpm >= 0)
        rpm = int(a.rpm + 0.5);
    try {
        store_.record(now, *cur, a.delta, estimated, wheel_, rpm);
    } catch (const std::exception& e) {
        // Nothing was stored, so the baseline stays: the next measurement counts these turns with its own.
        log("storage error, ", a.delta, " turns carried to the next measurement: ", e.what());
        return;
    }
    missed_.counted(a.delta);
    if (a.reset)
        log("counter discontinuity (", last_->revs, " -> ", cur->revs, "): ",
            a.delta ? "the sensor restarted, its " + std::to_string(a.delta) + " added" : "nothing added");
    if (a.delta > 0)
        log("revs=", cur->revs, " +", a.delta, " rpm=", int(a.rpm + 0.5),
            estimated ? " (+" + std::to_string(estimated) + " the sensor missed)" : "");
    last_ = *cur;
    lastWall_ = now;
}

} // namespace purr::logger
