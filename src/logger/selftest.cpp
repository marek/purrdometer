// purrdometer-logger --selftest: accounting, parsing and the store, with no
// Bluetooth.

#include "logger/accounting.h"
#include "logger/measurement.h"
#include "logger/store.h"

#include <cmath>
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
    check(a.reset && a.delta == 0, "counter going backwards = reset");
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

    Store mem(":memory:");
    mem.record(120.5, {Counter::Crank, 5, 0}, 3);
    mem.record(150.0, {Counter::Wheel, 7, 0}, 2);
    check(mem.state("last_wall").value().starts_with("150") && mem.state("last_counter") == "wheel", "state persisted");
    mem.forgetCounter();
    check(!mem.state("last_revs") && !mem.state("last_counter"), "switching sensor forgets the counter");
    mem.sensorHeard("AA:BB:CC:DD:EE:FF", "SPD-BLE1", -60);
    mem.sensorHeard("AA:BB:CC:DD:EE:FF", "SPD-BLE1", std::nullopt, "speed");
    check(true, "sensors recorded");
    std::cout << (failures ? "FAILED\n" : "all passed\n");
    return failures ? 1 : 0;
}
