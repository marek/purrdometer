// Runs a program with an argument list, never through a shell, so a network
// name or password is passed as it is and cannot become a command.
#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace purr::setup {

struct CommandResult {
    int status = -1;                // the exit status; -1 if it did not exit by itself or did not run
    bool timedOut = false;          // killed, with its whole process group, at the deadline
    std::string out, err;           // each cut at MAX_OUTPUT_BYTES
    bool ok() const { return status == 0; }
    std::string why() const;        // stderr, else stdout, trimmed; "timed out" when that is all there is
};

constexpr size_t MAX_OUTPUT_BYTES = 1 << 20;

// argv[0] is looked up on PATH; input is fed to stdin. The deadline covers the
// whole run, however the program behaves: it is killed at it. Never blocks past
// the deadline and the end of the program; SIGPIPE is ignored for the process.
CommandResult runCommand(const std::vector<std::string>& argv, const std::string& input = "",
                         std::chrono::milliseconds timeout = std::chrono::seconds(60));

// How a caller runs commands; tests substitute their own.
using Runner = std::function<CommandResult(const std::vector<std::string>& argv, const std::string& input,
                                           std::chrono::milliseconds timeout)>;

} // namespace purr::setup
