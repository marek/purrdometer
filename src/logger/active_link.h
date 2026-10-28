// The one sensor link the logger follows. The loop's thread chooses and
// replaces it; BlueZ's removal signal arrives on the D-Bus thread. A removal
// is delivered under the lock that replace() takes to swap the link out, so it
// reaches the link it names or none, and no removal is inside a link when it is
// destroyed (on the loop's thread). This covers the removal only: a link's own
// signal handlers are closed by its destructor (CallbackGuard).
#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace purr::logger {

// Link needs path() and lost().
template <typename Link>
class ActiveLink {
public:
    // The loop's thread only: it is the only one that replaces the link.
    Link* get() const { return link_.get(); }
    Link* operator->() const { return link_.get(); }
    explicit operator bool() const { return bool(link_); }

    // The loop's thread: follow `next` (or nothing) from now on.
    void replace(std::unique_ptr<Link> next)
    {
        {
            std::scoped_lock lock(mutex_);
            std::swap(link_, next);
        }
        // `next` now holds the old link: destroyed here, outside the lock.
    }

    // The D-Bus thread: BlueZ removed the object at `path`.
    void lost(const std::string& path)
    {
        std::scoped_lock lock(mutex_);
        if (link_ && path == link_->path())
            link_->lost();
    }

private:
    std::mutex mutex_;
    std::unique_ptr<Link> link_;
};

} // namespace purr::logger
