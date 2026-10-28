// Lets a D-Bus callback and the object it belongs to part safely: a callback
// runs only while the guard is open, and close() waits for one that is
// running. Whatever the D-Bus library does when a proxy is destroyed while its
// signal is being delivered, a closed guard means the late delivery touches
// nothing.
#pragma once

#include <mutex>

namespace purr::logger {

class CallbackGuard {
public:
    // Runs f unless the guard is closed; false when it was.
    template <typename F>
    bool run(F&& f)
    {
        std::scoped_lock lock(mutex_);
        if (closed_)
            return false;
        f();
        return true;
    }

    // After this returns no callback is running or will run. Not to be called from inside one.
    void close()
    {
        std::scoped_lock lock(mutex_);
        closed_ = true;
    }

private:
    std::mutex mutex_;
    bool closed_ = false;
};

} // namespace purr::logger
