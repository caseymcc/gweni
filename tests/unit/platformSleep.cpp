/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  platform::sleep(ms) takes milliseconds. On POSIX it used to call ::sleep(),
 *  which takes seconds, so WindowCanvas's idle platform::sleep(10) froze the
 *  app for 10 s on every frame with nothing to redraw.
 */
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <chrono>

using namespace gweni;

TEST_CASE("platform::sleep waits milliseconds, not seconds")
{
    using Clock=std::chrono::steady_clock;

    const Clock::time_point start=Clock::now();
    platform::sleep(20);
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start);

    INFO("platform::sleep(20) took ", elapsed.count(), " ms");
    // generous bounds, so a loaded host does not make this flaky; seconds
    // instead of milliseconds would take 20 s
    CHECK(elapsed.count() >= 15);
    CHECK(elapsed.count() < 500);
}

TEST_CASE("platform::sleep(0) returns at once")
{
    using Clock=std::chrono::steady_clock;

    const Clock::time_point start=Clock::now();
    platform::sleep(0);
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start);

    INFO("platform::sleep(0) took ", elapsed.count(), " ms");
    CHECK(elapsed.count() < 500);
}
