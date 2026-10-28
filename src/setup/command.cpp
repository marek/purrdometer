#include "setup/command.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace purr::setup {

namespace {

using Clock = std::chrono::steady_clock;
constexpr int POLL_SLICE_MS = 50;
constexpr size_t MAX_PASS_BYTES = 1 << 16;

std::string trimmed(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

// A pipe both of whose ends close on exec.
struct Pipe {
    int r = -1, w = -1;
    bool open()
    {
        int fds[2];
        if (pipe2(fds, O_CLOEXEC) != 0)
            return false;
        r = fds[0];
        w = fds[1];
        return true;
    }
    void closeBoth()
    {
        for (int* fd : {&r, &w}) {
            if (*fd >= 0)
                close(*fd);
            *fd = -1;
        }
    }
};

void closeFd(int& fd)
{
    if (fd >= 0)
        close(fd);
    fd = -1;
}

CommandResult unrun(const std::string& why)
{
    CommandResult r;
    r.err = why;
    return r;
}

enum class Wait { Running, Exited, Failed };

// Whether the child has exited, without waiting. Failed: waitpid could not say (the child is not ours
// to reap, as when SIGCHLD is ignored), which is not an exit status of any kind.
Wait reaped(pid_t pid, int& status)
{
    pid_t got;
    do
        got = waitpid(pid, &status, WNOHANG);
    while (got < 0 && errno == EINTR);
    if (got == pid)
        return Wait::Exited;
    return got < 0 ? Wait::Failed : Wait::Running;
}

} // namespace

std::string CommandResult::why() const
{
    std::string text = trimmed(err.empty() ? out : err);
    return text.empty() && timedOut ? "timed out" : text;
}

CommandResult runCommand(const std::vector<std::string>& argv, const std::string& input,
                         std::chrono::milliseconds timeout)
{
    if (argv.empty())
        return unrun("no command to run");
    std::signal(SIGPIPE, SIG_IGN); // a program that stops reading its input must not end ours

    std::vector<char*> args;
    for (auto& a : argv)
        args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);

    Pipe in, out, err, exec;
    if (!in.open() || !out.open() || !err.open() || !exec.open()) {
        std::string why = std::string("cannot make pipes: ") + std::strerror(errno);
        for (Pipe* p : {&in, &out, &err, &exec})
            p->closeBoth();
        return unrun(why);
    }
    pid_t pid = fork();
    if (pid < 0) {
        std::string why = std::string("cannot start ") + argv[0] + ": " + std::strerror(errno);
        for (Pipe* p : {&in, &out, &err, &exec})
            p->closeBoth();
        return unrun(why);
    }
    if (pid == 0) {
        std::signal(SIGPIPE, SIG_DFL); // an ignored signal stays ignored across exec
        setpgid(0, 0); // its own group, so a deadline can end it and whatever it started
        dup2(in.r, 0);
        dup2(out.w, 1);
        dup2(err.w, 2);
        execvp(args[0], args.data());
        int why = errno;
        [[maybe_unused]] ssize_t told = ::write(exec.w, &why, sizeof why);
        _exit(127);
    }
    setpgid(pid, pid); // also from this side: whichever runs first wins the race to kill it
    closeFd(in.r);
    closeFd(out.w);
    closeFd(err.w);
    closeFd(exec.w);

    for (int fd : {in.w, out.r, err.r, exec.r})
        fcntl(fd, F_SETFL, O_NONBLOCK);

    CommandResult r;
    auto deadline = Clock::now() + timeout;
    size_t fed = 0;
    if (input.empty())
        closeFd(in.w);
    int status = 0, execErrno = 0;
    Wait waited = Wait::Running;
    char buf[4096];
    // What a pass reads of each pipe is bounded, so a program that never stops writing cannot keep the
    // loop from its deadline; output past the cap is read and dropped.
    auto readOut = [&] {
        for (int i = 0; i < 2; ++i) {
            int& fd = i ? err.r : out.r;
            for (size_t passBytes = 0; fd >= 0 && passBytes < MAX_PASS_BYTES;) {
                ssize_t n = ::read(fd, buf, sizeof buf);
                if (n > 0) {
                    std::string& to = i ? r.err : r.out;
                    to.append(buf, std::min(size_t(n), MAX_OUTPUT_BYTES - std::min(to.size(), MAX_OUTPUT_BYTES)));
                    passBytes += size_t(n);
                } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                    closeFd(fd);
                } else if (errno == EAGAIN) {
                    break;
                }
            }
        }
    };
    // The child's exec closes this pipe; an errno on it means the program never ran.
    auto readExec = [&] {
        while (exec.r >= 0) {
            ssize_t n = ::read(exec.r, &execErrno, sizeof execErrno);
            if (n == ssize_t(sizeof execErrno)) {
                closeFd(exec.r);
                return true;
            }
            if (n < 0 && errno == EINTR)
                continue;
            if (n == 0 || errno != EAGAIN)
                closeFd(exec.r);
            break;
        }
        return execErrno != 0;
    };
    while (true) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        if (left.count() <= 0) {
            // A program that finished as the deadline came has finished: only one still running is late.
            waited = reaped(pid, status);
            r.timedOut = waited == Wait::Running;
            break;
        }
        pollfd fds[4] = {{out.r, POLLIN, 0}, {err.r, POLLIN, 0}, {in.w, POLLOUT, 0}, {exec.r, POLLIN, 0}};
        ::poll(fds, 4, int(std::min<long long>(left.count(), POLL_SLICE_MS)));
        readOut();
        if (readExec())
            break;
        if (in.w >= 0) {
            ssize_t n = ::write(in.w, input.data() + fed, input.size() - fed);
            if (n > 0)
                fed += size_t(n);
            if (fed == input.size() || (n < 0 && errno != EAGAIN && errno != EINTR))
                closeFd(in.w); // a program that stopped reading reports that itself
        }
        waited = reaped(pid, status);
        if (waited != Wait::Running)
            break;
    }
    if (r.timedOut || execErrno != 0) {
        kill(-pid, SIGKILL);
        kill(pid, SIGKILL);
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
    }
    readOut();
    readExec();
    for (int* fd : {&in.w, &out.r, &err.r, &exec.r})
        closeFd(*fd);
    if (execErrno != 0)
        return unrun(std::string("cannot run ") + argv[0] + ": " + std::strerror(execErrno));
    if (waited == Wait::Failed)
        r.err += (r.err.empty() ? "" : "\n") + std::string("could not wait for ") + argv[0] + ": " + std::strerror(errno);
    r.status = waited == Wait::Exited && WIFEXITED(status) && !r.timedOut ? WEXITSTATUS(status) : -1;
    return r;
}

} // namespace purr::setup
