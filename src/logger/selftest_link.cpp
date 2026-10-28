// Who owns the sensor link while BlueZ's removal signal and the loop's switch
// of sensor meet: the removal reaches a live link or none, and a link is
// destroyed only when no removal is inside it.

#include "logger/active_link.h"
#include "logger/callback_guard.h"
#include "logger/selftest.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace purr::logger;

namespace {

// Stands in for SensorLink, which needs a D-Bus connection to exist.
struct FakeLink {
    struct Tally {
        std::atomic<int> lostCalls{0}, usedAfterDestroy{0}, destroyedInside{0};
    };

    FakeLink(std::string where, Tally& t) : where_(std::move(where)), tally_(t) {}
    ~FakeLink()
    {
        if (inside_ > 0)
            ++tally_.destroyedInside;
        alive_ = 0;
    }

    const std::string& path() const { return where_; }
    void lost()
    {
        ++inside_;
        if (alive_ != ALIVE)
            ++tally_.usedAfterDestroy;
        std::this_thread::yield();
        ++tally_.lostCalls;
        --inside_;
    }

    static constexpr unsigned ALIVE = 0xA11FE;
    std::string where_;
    Tally& tally_;
    std::atomic<int> inside_{0}; // removals in progress in this link
    volatile unsigned alive_ = ALIVE;
};

} // namespace

void selftestLink(purr::Check& check)
{
    FakeLink::Tally tally;
    ActiveLink<FakeLink> active;
    active.lost("/a");
    check(!active && tally.lostCalls == 0, "a removal with no link followed reaches nothing");
    active.replace(std::make_unique<FakeLink>("/a", tally));
    active.lost("/b");
    check(active && active->path() == "/a" && tally.lostCalls == 0, "a removal of another device is not this link's");
    active.lost("/a");
    check(tally.lostCalls == 1, "a removal of the device reaches its link");
    active.replace(nullptr);
    check(!active, "following nothing");

    // The D-Bus thread removes while the loop switches sensor over and over.
    std::atomic<bool> stop{false};
    std::thread bus([&] {
        while (!stop) {
            active.lost("/a");
            active.lost("/b");
        }
    });
    for (int i = 0; i < 20000; ++i)
        active.replace(std::make_unique<FakeLink>(i % 2 ? "/a" : "/b", tally));
    active.replace(nullptr);
    stop = true;
    bus.join();
    check(tally.usedAfterDestroy == 0 && tally.destroyedInside == 0 && tally.lostCalls > 1,
          "a removal during a sensor switch never reaches a destroyed link, nor one being destroyed");

    // A link's own signal handlers: closing the guard waits for one that is running, and stops the rest.
    {
        using namespace std::chrono_literals;
        CallbackGuard guard;
        std::atomic<int> inside{0}, calls{0}, afterClose{0};
        std::atomic<bool> closed{false}, stopWorker{false};
        std::thread worker([&] {
            while (!stopWorker) {
                bool ran = guard.run([&] {
                    ++inside;
                    if (closed)
                        ++afterClose;
                    std::this_thread::yield();
                    ++calls;
                    --inside;
                });
                if (!ran)
                    std::this_thread::sleep_for(1ms);
            }
        });
        std::this_thread::sleep_for(50ms);
        guard.close();
        closed = true;
        int atClose = calls, running = inside;
        std::this_thread::sleep_for(50ms);
        stopWorker = true;
        worker.join();
        check(running == 0 && afterClose == 0 && calls == atClose && atClose > 0,
              "a closed guard has no handler running and lets no later one in");
        check(!guard.run([] {}), "a handler delivered after the link is gone does nothing");
    }
}
