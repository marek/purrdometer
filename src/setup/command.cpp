#include "setup/command.h"

#include <chrono>
#include <csignal>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace purr::setup {

namespace {

std::string trimmed(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

[[noreturn]] void execChild(const std::vector<std::string>& argv, int in, int out, int err)
{
    dup2(in, 0);
    dup2(out, 1);
    dup2(err, 2);
    std::vector<char*> args;
    for (auto& a : argv)
        args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);
    execvp(args[0], args.data());
    _exit(127);
}

} // namespace

std::string CommandResult::why() const { return trimmed(err.empty() ? out : err); }

CommandResult runCommand(const std::vector<std::string>& argv, const std::string& input, int timeoutS)
{
    int in[2], out[2], err[2];
    if (pipe(in) || pipe(out) || pipe(err))
        return {};
    pid_t pid = fork();
    if (pid == 0) {
        close(in[1]);
        close(out[0]);
        close(err[0]);
        execChild(argv, in[0], out[1], err[1]);
    }
    close(in[0]);
    close(out[1]);
    close(err[1]);
    // A short write leaves the program to complain about its input, which it reports in err.
    [[maybe_unused]] ssize_t fed = input.empty() ? 0 : ::write(in[1], input.data(), input.size());
    close(in[1]);

    CommandResult r;
    pollfd fds[2] = {{out[0], POLLIN, 0}, {err[0], POLLIN, 0}};
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutS);
    int open = 2;
    char buf[4096];
    while (open > 0 && std::chrono::steady_clock::now() < deadline) {
        if (::poll(fds, 2, 500) <= 0)
            continue;
        for (int i = 0; i < 2; ++i) {
            if (fds[i].fd < 0 || !(fds[i].revents & (POLLIN | POLLHUP)))
                continue;
            ssize_t n = ::read(fds[i].fd, buf, sizeof buf);
            if (n > 0) {
                (i ? r.err : r.out).append(buf, size_t(n));
                continue;
            }
            close(fds[i].fd);
            fds[i].fd = -1;
            --open;
        }
    }
    for (auto& f : fds)
        if (f.fd >= 0)
            close(f.fd);
    if (open > 0)
        kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
    r.status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return r;
}

} // namespace purr::setup
