// Runs a program with an argument list, never through a shell, so a network
// name or password is passed as it is and cannot become a command.
#pragma once

#include <string>
#include <vector>

namespace purr::setup {

struct CommandResult {
    int status = -1;                // the exit status; -1 if it did not exit by itself
    std::string out, err;
    bool ok() const { return status == 0; }
    std::string why() const;        // stderr, else stdout, trimmed
};

// argv[0] is looked up on PATH; input is fed to stdin. Killed after timeoutS.
CommandResult runCommand(const std::vector<std::string>& argv, const std::string& input = "", int timeoutS = 60);

} // namespace purr::setup
