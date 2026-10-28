// What a program's --selftest cases share: one counter of failures, one line
// per check.
#pragma once

#include <print>
#include <string>

namespace purr {

struct Check {
    int failures = 0;

    void operator()(bool ok, const std::string& what)
    {
        std::println("{} {}", ok ? "ok  " : "FAIL", what);
        failures += !ok;
    }

    // The closing line of a self-test; the exit status for main.
    int finish() const
    {
        std::println("{}", failures ? "FAILED" : "all passed");
        return failures ? 1 : 0;
    }
};

} // namespace purr
