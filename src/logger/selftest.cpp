// purrdometer-logger --selftest: accounting, parsing and the store, with no
// Bluetooth.

#include "logger/accounting.h"
#include "logger/measurement.h"
#include "logger/missed_turns.h"
#include "logger/store.h"

#include "common/calendar.h"
#include "common/stats.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using namespace purr::logger;

int selftest()
{
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        std::cout << (ok ? "ok   " : "FAIL ") << what << '\n';
        failures += !ok;
    };
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
    check(m.estimate(1, false) == 0, "none after it, however long the stop");
    m.connected(true);
    check(m.estimate(1, false) == 0, "a logger restart under an awake sensor adds nothing");
    m.connected(true);
    check(m.estimate(1, true) == 2, "the first connection after start, with the count back: a wake");
    m.configure(0);
    m.connected(false);
    check(m.estimate(1, false) == 0, "off at 0");

    Store mem(":memory:");
    mem.record(120.5, {Counter::Crank, 5, 0}, 3);
    mem.record(150.0, {Counter::Wheel, 7, 0}, 2);
    check(mem.state("last_wall").value().starts_with("150") && mem.state("last_counter") == "wheel", "state persisted");
    mem.forgetCounter();
    check(!mem.state("last_revs") && !mem.state("last_counter"), "switching sensor forgets the counter");
    mem.sensorHeard("AA:BB:CC:DD:EE:FF", "SPD-BLE1", -60);
    mem.sensorHeard("AA:BB:CC:DD:EE:FF", "SPD-BLE1", std::nullopt, "speed");
    check(true, "sensors recorded");
    std::string file = std::string(std::getenv("PURR_TMP") ? std::getenv("PURR_TMP") : "tmp") + "/logger-selftest.db";
    std::remove(file.c_str());
    {
        Store disk(file);
        disk.record(6000, {Counter::Crank, 10, 0}, 10, 2);
        disk.record(6010, {Counter::Crank, 11, 0}, 1, 0);
    }
    purr::Stats st = purr::loadStats(file, purr::cal::localDay(6000));
    check(st.total == 13 && st.estimated == 2, "totals include estimated turns, shown apart");
    std::remove(file.c_str());
    std::cout << (failures ? "FAILED\n" : "all passed\n");
    return failures ? 1 : 0;
}
