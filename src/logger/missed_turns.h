// The turns the sensor misses waking up, estimated
// (docs/intents/data/accounting.md#turns-the-sensor-misses): wake_turns at the
// first turn it counts after waking. Once it counts, it misses none.
#pragma once

namespace purr::logger {

class MissedTurns {
public:
    void configure(int wakeTurns) { wake_ = wakeTurns; }

    // A new connection: the sensor woke, unless the logger just started and
    // the sensor's count did not go back (checked at its first counted turn).
    void connected(bool firstSinceStart)
    {
        awaitingFirstTurn_ = true;
        needRestartProof_ = firstSinceStart;
    }

    // Turns to add for a measurement that counted `delta` turns; restarted:
    // the sensor's count went back since the last measurement.
    int estimate(unsigned delta, bool restarted)
    {
        if (delta == 0 || !awaitingFirstTurn_)
            return 0;
        awaitingFirstTurn_ = false;
        return !needRestartProof_ || restarted ? wake_ : 0;
    }

private:
    int wake_ = 2;
    bool awaitingFirstTurn_ = false, needRestartProof_ = false;
};

} // namespace purr::logger
