/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The platform clock, what platform::getTimeInSeconds() reads with no
 *  setTimeSource() installed, is wall time. On Linux it used to be clock(),
 *  the process's CPU time over all its threads: it stood still while the
 *  process slept and ran several times faster than wall time while llvmpipe
 *  rasterised on several threads, so a window's caret blink, tooltip delay,
 *  animations and the stats overlay's fps and 250 ms refresh ran on it (a
 *  window drawing 130 frames a second showed fps 6). Each case compares the
 *  clock's advance with steady_clock's over the same span, with bounds wide
 *  enough for a loaded host. One read after a long sleep has the whole of it:
 *  GWEN's Windows clock let a read advance at most 0.1 s, so an idle loop
 *  waking at a deadline read a clock short of it (a 5 s timer took about two
 *  minutes). The web build has no threads (and its clock is performance.now()),
 *  so it runs the sleep cases alone.
 */
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace gweni;

namespace
{

using Clock=std::chrono::steady_clock;

double millisSince(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now()-start).count();
}

}//namespace

TEST_CASE("platform clock: advances while the process sleeps")
{
    platform::setTimeSource(nullptr);

    const Clock::time_point wallStart=Clock::now();
    const double start=platform::getTimeInSeconds();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const double advance=(platform::getTimeInSeconds()-start)*1000.0;
    const double wall=millisSince(wallStart);

    INFO("the clock advanced ", advance, " ms over ", wall, " ms of wall time");
    // CPU time stands still here
    CHECK(advance >= 40.0);
    CHECK(advance <= wall+5.0);
}

TEST_CASE("platform clock: one read after a long sleep has the whole of it")
{
    platform::setTimeSource(nullptr);

    const Clock::time_point wallStart=Clock::now();
    const double start=platform::getTimeInSeconds();
    // longer than any per-read cap a clock could have and a frame many times over
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    const double advance=(platform::getTimeInSeconds()-start)*1000.0;
    const double wall=millisSince(wallStart);

    INFO("one read advanced ", advance, " ms over ", wall, " ms of wall time");
    CHECK(advance >= 390.0);
    CHECK(advance <= wall+5.0);
}

#if !defined(__EMSCRIPTEN__)
TEST_CASE("platform clock: no faster than wall time while several threads run")
{
    platform::setTimeSource(nullptr);

    std::atomic<bool> stop{false};
    std::vector<std::thread> spinners;

    const Clock::time_point wallStart=Clock::now();
    const double start=platform::getTimeInSeconds();

    for(int i=0; i<4; ++i)
        spinners.emplace_back([&stop]() { while(!stop.load(std::memory_order_relaxed)) {} });
    while(millisSince(wallStart) < 100.0)
        {}
    stop=true;
    for(std::thread &spinner:spinners)
        spinner.join();

    const double advance=(platform::getTimeInSeconds()-start)*1000.0;
    const double wall=millisSince(wallStart);

    INFO("the clock advanced ", advance, " ms over ", wall, " ms of wall time");
    // CPU time over five busy threads is up to five times the wall time
    CHECK(advance >= 90.0);
    CHECK(advance <= wall+5.0);
}
#endif
