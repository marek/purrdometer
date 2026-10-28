// The logger's accounting, measurement parsing and missed-turn estimates.

#include "logger/accounting.h"
#include "logger/measurement.h"
#include "logger/missed_turns.h"
#include "logger/selftest.h"

#include <cmath>

using namespace purr::logger;

void selftestAccounting(purr::Check& check)
{
    const Sample crank{Counter::Crank, 10, 0};
    auto a = account(std::nullopt, 0, crank, 100);
    check(a.delta == 0 && !a.reset, "first sample adds nothing");
    a = account(crank, 100, {Counter::Crank, 11, 1024}, 101);
    check(a.delta == 1 && std::lround(a.rpm) == 60, "1 rev in 1 s = 60 rpm");
    a = account(Sample{Counter::Crank, 65535, 64000}, 100, {Counter::Crank, 1, 1000}, 101);
    check(a.delta == 2 && !a.reset, "16-bit crank counter wraps");
    a = account(Sample{Counter::Crank, 100, 0}, 100, {Counter::Crank, 5, 0}, 101);
    check(a.reset && a.delta == 5, "a restarted sensor's count is added");
    a = account(Sample{Counter::Crank, 509, 0}, 100, {Counter::Crank, 8, 0}, 100 + 600);
    check(a.reset && a.delta == 8, "509 -> 8 ten minutes later adds 8");
    a = account(Sample{Counter::Crank, 10, 41334}, 100, {Counter::Crank, 5, 10855}, 100 + 14.6 * 3600);
    check(a.reset && a.delta == 5, "10 -> 5 after a night asleep: the sensor restarted, 5 turns, not 65,531");
    a = account(Sample{Counter::Wheel, 10, 0}, 100, {Counter::Wheel, 5, 0}, 100 + 30 * 86400);
    check(a.reset && a.delta == 5, "a wheel counter going back after a month: 5 turns");
    a = account(Sample{Counter::Crank, 65000, 0}, 100, {Counter::Crank, 200, 0}, 100 + 3600);
    check(!a.reset && a.delta == 736, "a real 16-bit wrap still counts across it");
    a = account(Sample{Counter::Crank, 509, 0}, 100, {Counter::Crank, 300, 0}, 101);
    check(a.reset && a.delta == 0, "a restart count too high for the time is refused");
    a = account(Sample{Counter::Crank, 100, 0}, 100, {Counter::Crank, 1100, 0}, 101);
    check(a.reset, "implausible jump = reset");
    a = account(Sample{Counter::Crank, 100, 0}, 100, {Counter::Crank, 1100, 0}, 100 + 3600);
    check(a.delta == 1000 && !a.reset, "revs counted while logger was offline are kept");
    a = account(Sample{Counter::Crank, 100, 500}, 100, {Counter::Crank, 100, 500}, 101);
    check(a.delta == 0 && a.rpm == 0, "no motion = 0 rpm");
    a = account(Sample{Counter::Wheel, 0xFFFFFFFF, 0}, 100, {Counter::Wheel, 2, 2048}, 101);
    check(a.delta == 3 && std::lround(a.rpm) == 90, "32-bit wheel counter wraps");
    a = account(Sample{Counter::Wheel, 70000, 0}, 100, {Counter::Wheel, 70004, 1024}, 101);
    check(a.delta == 4, "wheel counter past 16 bits");
    a = account(Sample{Counter::Crank, 100, 0}, 100, {Counter::Wheel, 5000, 0}, 101);
    check(a.delta == 0 && !a.reset, "another kind of counter is a new baseline");

    // flags, wheel revs (le32), wheel time (le16), crank revs (le16), crank time (le16)
    std::vector<uint8_t> both = {0x03, 0x10, 0x27, 0x01, 0x00, 0x00, 0x04, 0x2A, 0x00, 0x00, 0x08};
    auto w = parseMeasurement(both, Counter::Wheel), c = parseMeasurement(both, Counter::Crank);
    check(w && w->revs == 0x12710 && w->eventTime == 0x0400, "a speed sensor's wheel revolutions");
    check(c && c->revs == 42 && c->eventTime == 0x0800, "a cadence sensor's crank revolutions");
    check(!parseMeasurement({0x02, 0x2A, 0x00, 0x00, 0x08}, Counter::Wheel), "no wheel data for a speed sensor");
    check(!parseMeasurement({0x01, 0x01}, Counter::Wheel), "truncated measurement");

    MissedTurns m;
    m.configure(2);
    m.connected(false);
    check(m.estimate(0, false) == 0 && m.estimate(1, false) == 2, "wake turns at the first counted turn");
    check(m.estimate(1, false) == 2, "asking again changes nothing until the turn is stored");
    m.counted(0);
    check(m.estimate(1, false) == 2, "a measurement that counted nothing ends no wake");
    m.counted(1);
    check(m.estimate(1, false) == 0, "none after it, however long the stop");
    m.connected(true);
    check(m.estimate(1, false) == 0, "a logger restart under an awake sensor adds nothing");
    m.connected(true);
    check(m.estimate(1, true) == 2, "the first connection after start, with the count back: a wake");
    m.configure(0);
    m.connected(false);
    check(m.estimate(1, false) == 0, "off at 0");
}
