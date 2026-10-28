// runCommand: input and output, failures, and a deadline that holds however the program behaves.

#include "setup/command.h"
#include "setup/selftest.h"

#include <chrono>
#include <csignal>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

using namespace purr::setup;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace {

bool gone(pid_t pid)
{
    // Gone, or a zombie waiting for a parent that does not reap.
    for (int i = 0; i < 50; ++i) {
        if (kill(pid, 0) != 0)
            return true;
        std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
        std::string line;
        std::getline(stat, line);
        auto close = line.rfind(')');
        if (close != std::string::npos && close + 2 < line.size() && line[close + 2] == 'Z')
            return true;
        std::this_thread::sleep_for(20ms);
    }
    return false;
}

} // namespace

void selftestCommand(purr::Check& check)
{
    auto echo = runCommand({"cat"}, "fed through stdin");
    check(echo.ok() && echo.out == "fed through stdin", "commands run without a shell, with input");

    std::string big(3 << 20, 'x'); // many times a pipe's buffer, both ways
    auto through = runCommand({"cat"}, big);
    check(through.ok() && through.out.size() == MAX_OUTPUT_BYTES && through.out == big.substr(0, MAX_OUTPUT_BYTES),
          "input larger than a pipe is fed in full, and output is cut at its cap");

    auto three = runCommand({"sh", "-c", "echo out; echo err >&2; exit 3"});
    check(three.status == 3 && !three.ok() && three.out == "out\n" && three.err == "err\n" && three.why() == "err",
          "exit status, stdout and stderr apart");

    auto missing = runCommand({"purrdometer-no-such-program"});
    check(missing.status == -1 && !missing.timedOut && missing.why().contains("cannot run"),
          "a program that does not exist is reported, not an exit status");
    check(runCommand({}).status == -1, "no command at all");

    auto quitter = runCommand({"sh", "-c", "exit 0"}, std::string(1 << 20, 'y'));
    check(quitter.ok(), "a program that never reads its input does not stop the helper (SIGPIPE)");

    auto begin = Clock::now();
    auto sleeper = runCommand({"sleep", "30"}, "", 300ms);
    auto took = Clock::now() - begin;
    check(sleeper.timedOut && sleeper.status == -1 && sleeper.why() == "timed out" && took < 5s,
          "a silent program is killed at the deadline");

    begin = Clock::now();
    auto closed = runCommand({"sh", "-c", "exec >&- 2>&-; sleep 30"}, "", 300ms);
    took = Clock::now() - begin;
    check(closed.timedOut && took < 5s, "a program that closes its output and runs on is killed at the deadline");

    begin = Clock::now();
    auto group = runCommand({"sh", "-c", "sleep 30 & echo $!; wait"}, "", 500ms);
    took = Clock::now() - begin;
    pid_t child = group.out.empty() ? 0 : pid_t(std::stol(group.out));
    check(group.timedOut && took < 5s && child > 1 && gone(child),
          "the deadline ends what the program started too, and keeps what it said");

    begin = Clock::now();
    auto left = runCommand({"sh", "-c", "sleep 3 & echo done"}, "", 20s);
    took = Clock::now() - begin;
    check(left.ok() && !left.timedOut && left.out == "done\n" && took < 2s,
          "a program that exits is not waited on for what it left running");

    begin = Clock::now();
    auto chatty = runCommand({"sh", "-c", "while true; do echo spam; done"}, "", 400ms);
    took = Clock::now() - begin;
    check(chatty.timedOut && took < 5s && chatty.out.size() <= MAX_OUTPUT_BYTES,
          "a program that never stops talking is killed at the deadline");

    begin = Clock::now();
    auto yes = runCommand({"yes"}, "", 400ms);
    took = Clock::now() - begin;
    check(yes.timedOut && took < 3s && yes.out.size() == MAX_OUTPUT_BYTES,
          "a program writing as fast as it can is cut at the output cap and at its deadline");

    // With SIGCHLD ignored the kernel reaps children itself and waitpid has nothing to report: that is
    // not an exit status, least of all 0.
    auto before = std::signal(SIGCHLD, SIG_IGN);
    auto unwaited = runCommand({"sh", "-c", "echo said; exit 3"});
    auto unwaitedOk = runCommand({"true"});
    std::signal(SIGCHLD, before == SIG_ERR ? SIG_DFL : before);
    check(unwaited.status == -1 && !unwaited.ok() && unwaited.out == "said\n" && unwaited.why().contains("could not wait"),
          "a child that waitpid cannot report on is not a success, and what it said is kept");
    check(unwaitedOk.status == -1 && !unwaitedOk.ok(), "not even when it would have exited 0");

    auto instant = runCommand({"true"}, "", 0ms);
    check(instant.timedOut || instant.ok(), "a deadline that has already come ends the run whichever way it fell");
    auto exited = runCommand({"sh", "-c", "exit 4"}, "", 5s);
    check(exited.status == 4 && !exited.timedOut, "an exit status is not mistaken for a timeout");
}
