/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The canvas's UI-thread timers (Canvas::after(), every(), cancel()), on a
 *  stepped clock (setTimeSource), the frame's time base+frame*(1.0/60): a
 *  timer fires once, on the first update whose time is at or past its
 *  deadline, the same frame every run; timers due in one update fire by
 *  deadline and then in the order they were added, and one a callback adds
 *  waits for the next update, while one a post adds with no delay fires in
 *  the post's update; a repeating timer's k-th deadline is its first plus k
 *  periods (no drift over 10 s of 1/10 s periods), and after a gap of 10.5
 *  periods it fires once and goes on from the next multiple; cancelling from
 *  outside, from its own callback and from another timer's, its function
 *  destroyed once it no longer runs; deleting the owner, outright or
 *  delayed, cancels its timers; a waiting timer runs no animation, enters no
 *  control, lays out and renders nothing and writes no slot; and the wake
 *  handler is called for a timer earlier than every pending one, never for
 *  a later one or from inside the timers' run.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    int frameNumber=0;
    double base=10.0;
    //! seconds added to the clock on top of the frames (a gap)
    double jump=0.0;

    Scene()
    {
        platform::setTimeSource([this]() { return now(); });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        frame();
    }

    ~Scene()
    {
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    double now() const { return clockAt(frameNumber)+jump; }
    double clockAt(int frame) const { return base+double(frame)*(1.0/60.0); }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Steps the clock 1/60 s and renders a frame.
    void step()
    {
        ++frameNumber;
        frame();
    }

    //! The first frame from \p from whose time is at or past \p deadline.
    int firstFrameAtOrPast(double deadline, int from) const
    {
        int f=from;

        while(clockAt(f)+jump < deadline)
            ++f;
        return f;
    }
};

} // namespace

TEST_CASE("Timers: after() fires once, on the first update at or past its deadline")
{
    std::vector<int> first;

    for(int run=0; run < 2; ++run)
    {
        Scene scene;
        std::vector<int> fired;
        const double deadline=scene.now()+0.5;
        controls::TimerHandle timer=scene.canvas->after(0.5, [&]() { fired.push_back(scene.frameNumber); });

        REQUIRE(timer.valid());
        CHECK(scene.canvas->timerPending(timer));

        const int expected=scene.firstFrameAtOrPast(deadline, scene.frameNumber+1);

        while(scene.frameNumber < expected+60)
        {
            scene.step();
            if(scene.frameNumber == expected-1)
                CHECK(fired.empty());
        }

        REQUIRE(fired.size() == 1);
        CHECK(fired[0] == expected);
        CHECK_FALSE(scene.canvas->timerPending(timer));
        CHECK(scene.canvas->timerCount() == 0);
        // a fired one cancels as nothing
        CHECK_FALSE(scene.canvas->cancel(timer));
        first.push_back(fired[0]);
    }
    // the same frame every run
    CHECK(first[0] == first[1]);
}

TEST_CASE("Timers: due together they fire by deadline, then in the order added")
{
    Scene scene;
    std::vector<std::string> log;

    scene.canvas->after(0.2, [&]() { log.push_back("b 0.2"); });
    scene.canvas->after(0.1, [&]() { log.push_back("a 0.1"); });
    scene.canvas->after(0.2, [&]() { log.push_back("c 0.2"); });
    scene.canvas->after(0.0, [&]() { log.push_back("now"); });

    // one update a second later runs them all
    scene.jump=1.0;
    scene.frame();
    CHECK(log == std::vector<std::string>{"now", "a 0.1", "b 0.2", "c 0.2"});
}

TEST_CASE("Timers: a timer a callback adds waits for the next update, a post's fires in its own")
{
    Scene scene;
    std::vector<std::string> log;

    scene.canvas->after(0.0, [&]() {
        log.push_back("first");
        scene.canvas->after(0.0, [&]() { log.push_back("added by a timer"); });
    });
    scene.frame();
    CHECK(log == std::vector<std::string>{"first"});
    scene.frame();
    CHECK(log == std::vector<std::string>{"first", "added by a timer"});

    // the posts run before the timers, so a timer a post adds with no delay
    // is due in the post's update
    log.clear();
    scene.canvas->post([&]() {
        scene.canvas->after(0.0, [&]() { log.push_back("added by a post"); });
    });
    scene.frame();
    CHECK(log == std::vector<std::string>{"added by a post"});

    // a period of 0 fires once each update
    int runs=0;
    controls::TimerHandle each=scene.canvas->every(0.0, [&]() { ++runs; });
    for(int i=0; i < 4; ++i)
        scene.frame();
    CHECK(runs == 4);
    CHECK(scene.canvas->cancel(each));
}

TEST_CASE("Timers: a repeating timer does not drift")
{
    Scene scene;
    const double period=0.1;
    const double origin=scene.now()+period;
    std::vector<int> fired;

    controls::TimerHandle timer=scene.canvas->every(period, [&]() { fired.push_back(scene.frameNumber); });

    // 10 s of frames
    while(scene.frameNumber < 600)
        scene.step();

    // the k-th deadline is origin+k*period: each fire on the first frame at
    // or past it, with no error added up over a hundred periods
    REQUIRE(fired.size() >= 99);
    bool onTime=true;

    for(size_t k=0; k < fired.size(); ++k)
    {
        const int expected=scene.firstFrameAtOrPast(origin+double(k)*period, 1);

        if(fired[k] != expected)
        {
            onTime=false;
            INFO("fire ", k, " at frame ", fired[k], ", due at frame ", expected);
            CHECK(fired[k] == expected);
        }
    }
    CHECK(onTime);
    CHECK(fired.size() == size_t(std::floor((scene.now()-origin)/period))+1);
    CHECK(scene.canvas->timerPending(timer));
}

TEST_CASE("Timers: after a gap of several periods a repeating timer fires once and skips ahead")
{
    Scene scene;
    const double period=0.1;
    const double origin=scene.now()+period;
    std::vector<int> fired;

    scene.canvas->every(period, [&]() { fired.push_back(scene.frameNumber); });

    // up to its first fire
    while(fired.empty())
        scene.step();
    REQUIRE(fired.size() == 1);

    // 10.5 periods pass in one update: one fire, not ten
    scene.jump=1.05;
    scene.step();
    CHECK(fired.size() == 2);

    // and it goes on from the next multiple of the period past that update
    const double after=scene.now();
    const double nextDeadline=origin+(std::floor((after-origin)/period)+1.0)*period;
    const int expected=scene.firstFrameAtOrPast(nextDeadline, scene.frameNumber+1);

    while(fired.size() < 3)
        scene.step();
    CHECK(fired[2] == expected);
    CHECK(fired[2]-fired[1] <= 6);
}

TEST_CASE("Timers: cancel from outside, from its own callback and from another's")
{
    Scene scene;

    SUBCASE("from outside")
    {
        bool ran=false;
        auto data=std::make_shared<int>(1);
        std::weak_ptr<int> captured=data;
        controls::TimerHandle timer=scene.canvas->after(0.1, [data, &ran]() { ran=true; });

        data.reset();
        CHECK(scene.canvas->cancel(timer));
        CHECK_FALSE(scene.canvas->cancel(timer));
        CHECK_FALSE(scene.canvas->timerPending(timer));
        // its function is gone at once
        CHECK(captured.expired());
        for(int i=0; i < 30; ++i)
            scene.step();
        CHECK_FALSE(ran);
        CHECK(scene.canvas->timerCount() == 0);
    }

    SUBCASE("from its own callback")
    {
        int runs=0;
        auto data=std::make_shared<int>(1);
        std::weak_ptr<int> captured=data;
        controls::TimerHandle timer;

        timer=scene.canvas->every(0.05, [&scene, &timer, &runs, &captured, data]() {
            ++runs;
            CHECK(scene.canvas->cancel(timer));
            // still alive while it runs
            CHECK_FALSE(captured.expired());
        });
        data.reset();
        for(int i=0; i < 60; ++i)
            scene.step();
        CHECK(runs == 1);
        CHECK_FALSE(scene.canvas->timerPending(timer));
        // destroyed once it returned
        CHECK(captured.expired());
    }

    SUBCASE("from another timer's callback")
    {
        std::vector<std::string> log;
        controls::TimerHandle later;
        controls::TimerHandle sameUpdate;

        scene.canvas->after(0.1, [&]() {
            log.push_back("canceller");
            CHECK(scene.canvas->cancel(sameUpdate));
            CHECK(scene.canvas->cancel(later));
        });
        sameUpdate=scene.canvas->after(0.1, [&]() { log.push_back("same update"); });
        later=scene.canvas->every(0.5, [&]() { log.push_back("later"); });

        for(int i=0; i < 60; ++i)
            scene.step();
        CHECK(log == std::vector<std::string>{"canceller"});
        CHECK(scene.canvas->timerCount() == 0);
    }
}

TEST_CASE("Timers: deleting the owner cancels its timers")
{
    Scene scene;
    int runs=0;

    SUBCASE("outright")
    {
        controls::Base *owner=scene.canvas->newChild<controls::Base>("owner");
        controls::TimerHandle once=scene.canvas->after(0.1, [&]() { ++runs; }, owner);
        controls::TimerHandle repeat=scene.canvas->every(0.1, [&]() { ++runs; }, owner);
        controls::TimerHandle unowned=scene.canvas->after(0.1, [&]() { runs+=100; });

        delete owner;
        CHECK_FALSE(scene.canvas->timerPending(once));
        CHECK_FALSE(scene.canvas->timerPending(repeat));
        CHECK(scene.canvas->timerPending(unowned));
        for(int i=0; i < 30; ++i)
            scene.step();
        CHECK(runs == 100);
    }

    SUBCASE("delayed, and owning a timer on another canvas")
    {
        controls::Canvas *other=new controls::Canvas(&scene.skin);
        controls::Base *owner=scene.canvas->newChild<controls::Base>("owner");
        controls::TimerHandle here=scene.canvas->every(0.1, [&]() { ++runs; }, owner);
        controls::TimerHandle there=other->after(0.1, [&]() { ++runs; }, owner);

        owner->delayedDelete();
        // pending until the delete happens, at the top of the next doThink()
        CHECK(scene.canvas->timerPending(here));
        scene.step();
        CHECK_FALSE(scene.canvas->timerPending(here));
        CHECK_FALSE(other->timerPending(there));
        for(int i=0; i < 30; ++i)
        {
            scene.step();
            other->updateCanvas();
        }
        CHECK(runs == 0);
        delete other;
    }
}

TEST_CASE("Timers: a waiting timer runs no animation and enters no control")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setBounds(10, 10, 100, 50);
    for(int i=0; i < 5; ++i)
        scene.step();

    int runs=0;
    scene.canvas->after(10.0, [&]() { ++runs; });
    scene.canvas->every(5.0, [&]() { ++runs; }, panel);

    for(int i=0; i < 120; ++i)
    {
        controls::resetChangeStats();
        scene.step();

        const controls::UpdateStats &update=scene.canvas->getUpdateStats();
        CHECK(update.animsRun == 0);
        CHECK(update.zVisited == 0);
        CHECK(scene.canvas->getLayoutStats().passes == 0);
        CHECK(scene.canvas->getLayoutStats().visited == 0);
        CHECK(controls::getChangeStats().visited == 0);
        CHECK(controls::getChangeStats().controls == 0);
        CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
        CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
    }
    CHECK(runs == 0);
    CHECK(scene.canvas->timerCount() == 2);
}

TEST_CASE("Timers: the wake handler is called for a timer earlier than every pending one")
{
    Scene scene;
    int wakes=0;

    scene.canvas->setWakeHandler([&]() { ++wakes; });

    scene.canvas->after(1.0, []() {});
    CHECK(wakes == 1);
    // later than the pending one: the loop already waits for that
    scene.canvas->after(2.0, []() {});
    CHECK(wakes == 1);
    // earlier
    controls::TimerHandle half=scene.canvas->after(0.5, []() {});
    CHECK(wakes == 2);
    // with the earliest cancelled the next is 1.0, and 0.75 is earlier
    CHECK(scene.canvas->cancel(half));
    scene.canvas->every(0.75, []() {});
    CHECK(wakes == 3);

    // one a callback adds during the timers' run wakes nothing: the loop is
    // in an update and looks at the timers again after it
    scene.canvas->after(0.0, [&]() { scene.canvas->after(0.01, []() {}); });
    CHECK(wakes == 4);
    scene.frame();
    CHECK(wakes == 4);
}
