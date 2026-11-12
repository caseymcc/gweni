/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  platform::IdleLoop, the scheduling of input::GlfwLoop with no window system
 *  in it, driven over a real canvas by a fake clock and a fake wait (one that
 *  moves the clock on by its timeout, or to the next scripted event): over a
 *  simulated idle minute it draws no frame, and after a post exactly one; with
 *  a focused TextBox it draws only on the caret's edges, 2 a second, each
 *  flipping the caret; an event that changes nothing is waited past with no
 *  frame; a post from another thread ends a blocked wait (through the
 *  canvas's wake handler) and is drawn by the next frame; LoopMode::Continuous
 *  draws every iteration, and a loop that cannot draw (an iconified window)
 *  waits for an event; the verify mode draws a frame wherever it would have
 *  skipped one and fails it, with its number, when it wrote a slot, as when
 *  the canvas's work is not reported; and a web loop's tick() draws only when
 *  work is due or a frame was wanted.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/textBox.h>
#include <gweni/platforms/idleLoop.h>
#include <gweni/platforms/input/glfwLoop.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace gweni;

namespace
{

using platform::IdleLoop;
using platform::LoopMode;
using platform::LoopOptions;

//! A StubControl that draws a rect into each of its slots when its control
//! has a state change, so the StubRenderer counts them as written.
class DrawingControl: public test::StubControl
{
public:
    using StubControl::StubControl;

    void update(renderer::Base *renderer, controls::Base *control) override
    {
        if(control->getStateChange() == controls::StateChange_Nothing)
            return;
        for(size_t id : getPrimitives())
            renderer->drawFilledRect(id, control->getRenderBounds(), control->getZIndex());
    }
};

class DrawingSkin: public test::StubSkin
{
public:
    using StubSkin::StubSkin;

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        const bool takesText=typeName == test::StubText::getTypeNameStatic();

        return std::make_unique<DrawingControl>(this, takesText, std::move(typeName));
    }
};

struct Scene
{
    test::StubRenderer renderer;
    DrawingSkin skin{&renderer};
    controls::Canvas *canvas;
    double clock=10.0;
    //! input the fake window system delivers once the clock reaches its time
    std::vector<std::pair<double, std::function<void()>>> events;
    //! where a wait with no timeout and no event leaves the clock: the end of
    //! the simulated span
    double end=std::numeric_limits<double>::infinity();
    std::vector<double> frameTimes;
    std::vector<std::pair<size_t, std::string>> failures;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        KeyboardFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void deliver()
    {
        for(size_t i=0; i < events.size();)
        {
            if(events[i].first <= clock)
            {
                std::function<void()> event=std::move(events[i].second);
                events.erase(events.begin()+long(i));
                event();
            }
            else
                ++i;
        }
    }

    double nextEvent() const
    {
        double next=std::numeric_limits<double>::infinity();

        for(const auto &event:events)
            next=std::min(next, event.first);
        return next;
    }

    IdleLoop::Hooks hooks()
    {
        IdleLoop::Hooks hooks;

        hooks.now=[this]() { return clock; };
        hooks.nextWorkTime=[this]() { return canvas->nextWorkTime(); };
        hooks.poll=[this]() { deliver(); };
        hooks.wait=[this](double seconds) {
            // an event ends the wait early, as one does in glfwWaitEvents()
            const double until=std::min(clock+seconds, nextEvent());

            clock=std::isinf(until)?std::max(clock, end):until;
            deliver();
        };
        hooks.drawFrame=[this]() {
            frameTimes.push_back(clock);
            canvas->updateCanvas();
            canvas->renderCanvas();
        };
        hooks.frameWasIdle=[this](std::string &why) {
            why=input::GlfwLoop::frameWrites(renderer.frameStats());
            return why.empty();
        };
        hooks.verifyFailed=[this](size_t frame, const std::string &why) {
            failures.push_back({frame, why});
        };
        return hooks;
    }

    //! Iterations until the clock reaches \p until (at most \p cap).
    void runUntil(IdleLoop &loop, double until, int cap=100000)
    {
        for(int i=0; i < cap && clock < until; ++i)
            loop.iterate();
        REQUIRE(clock >= until);
    }

    //! Iterations until the loop waits with nothing due (the canvas settled).
    void settle(IdleLoop &loop)
    {
        for(int i=0; i < 20 && (canvas->nextWorkTime() <= clock); ++i)
            loop.iterate();
        REQUIRE(canvas->nextWorkTime() > clock);
    }
};

} // namespace

TEST_CASE("Idle loop: no frame over an idle minute, exactly one after a post")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setBounds(10, 10, 100, 50);

    IdleLoop loop(scene.hooks(), LoopOptions());

    // the new tree is drawn, then the loop settles
    loop.iterate();
    CHECK(loop.frames() == 1);
    scene.settle(loop);

    const size_t settled=loop.frames();
    const double start=scene.clock;

    scene.end=start+60.0;
    scene.runUntil(loop, start+60.0);
    CHECK(loop.frames() == settled);
    CHECK(loop.waits() >= 1);

    // a post: the next iteration draws it, the one after waits again
    bool ran=false;
    scene.canvas->post([&]() {
        ran=true;
        panel->addStateChange(controls::StateChange_Background);
    });
    loop.iterate();
    CHECK(ran);
    CHECK(loop.frames() == settled+1);
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten >= 1);

    scene.end=scene.clock+60.0;
    scene.runUntil(loop, scene.end);
    CHECK(loop.frames() == settled+1);
}

TEST_CASE("Idle loop: an event that changes nothing is waited past with no frame")
{
    Scene scene;
    IdleLoop loop(scene.hooks(), LoopOptions());

    scene.settle(loop);
    const size_t settled=loop.frames();

    // the mouse moved over nothing on an empty canvas, five times a second
    for(int i=1; i <= 50; ++i)
        scene.events.push_back({scene.clock+0.2*i, []() {}});
    scene.end=scene.clock+20.0;
    scene.runUntil(loop, scene.end);
    CHECK(loop.frames() == settled);
    CHECK(loop.waits() >= 50);
}

TEST_CASE("Idle loop: a focused TextBox is drawn only on its caret's edges")
{
    Scene scene;
    controls::TextBox *box=scene.canvas->newChild<controls::TextBox>("box");
    box->setText("hello");

    IdleLoop loop(scene.hooks(), LoopOptions());

    scene.settle(loop);
    box->focus();
    scene.settle(loop);

    // past the 1.5 s the caret stays lit after the focus
    scene.runUntil(loop, scene.clock+2.0);
    scene.frameTimes.clear();

    std::vector<bool> lit;
    const double start=scene.clock;

    scene.events.clear();
    for(int i=0; i < 100000 && scene.clock < start+10.0; ++i)
    {
        const size_t before=scene.frameTimes.size();
        const bool wasLit=box->isCaretVisible();

        loop.iterate();
        if(scene.frameTimes.size() != before)
        {
            // every frame drawn is an edge
            CHECK(box->isCaretVisible() != wasLit);
            lit.push_back(box->isCaretVisible());
        }
    }

    // 2 a second, 0.5 s apart
    INFO("frames at ", scene.frameTimes.size(), " times over 10 s");
    CHECK(scene.frameTimes.size() == 20);
    for(size_t i=1; i < scene.frameTimes.size(); ++i)
        CHECK(scene.frameTimes[i]-scene.frameTimes[i-1] == doctest::Approx(0.5));
}

#if !defined(__EMSCRIPTEN__)
TEST_CASE("Idle loop: a post from another thread ends a blocked wait and is drawn next")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setBounds(10, 10, 100, 50);

    // a wait that blocks for real until the wake handler (or a 10 s backstop)
    std::mutex mutex;
    std::condition_variable woken;
    bool wake=false;
    bool timedOut=false;

    scene.canvas->setWakeHandler([&]() {
        std::lock_guard<std::mutex> lock(mutex);
        wake=true;
        woken.notify_all();
    });

    IdleLoop::Hooks hooks=scene.hooks();
    hooks.wait=[&](double seconds) {
        REQUIRE(std::isinf(seconds));
        std::unique_lock<std::mutex> lock(mutex);
        if(!woken.wait_for(lock, std::chrono::seconds(10), [&]() { return wake; }))
            timedOut=true;
        wake=false;
    };

    IdleLoop loop(hooks, LoopOptions());

    scene.settle(loop);
    const size_t settled=loop.frames();

    bool ran=false;
    std::thread worker([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        scene.canvas->post([&]() {
            ran=true;
            panel->addStateChange(controls::StateChange_Background);
        });
    });

    // blocks until the worker's post wakes it
    loop.iterate();
    worker.join();
    CHECK_FALSE(timedOut);
    CHECK(loop.frames() == settled);
    CHECK_FALSE(ran);

    // and the next iteration draws the post's change
    loop.iterate();
    CHECK(ran);
    CHECK(loop.frames() == settled+1);
    const std::vector<size_t> &written=scene.renderer.writtenPrimitiveIds();
    CHECK_FALSE(written.empty());
    scene.canvas->setWakeHandler(nullptr);
}
#endif

TEST_CASE("Idle loop: continuous draws every iteration, a window that cannot draw waits")
{
    Scene scene;
    LoopOptions continuous;
    continuous.mode=LoopMode::Continuous;

    IdleLoop loop(scene.hooks(), continuous);

    for(int i=0; i < 10; ++i)
        loop.iterate();
    CHECK(loop.frames() == 10);
    CHECK(loop.waits() == 0);

    // iconified: work due, but the loop waits for an event
    bool drawable=false;
    IdleLoop::Hooks hooks=scene.hooks();
    hooks.canDraw=[&]() { return drawable; };
    scene.end=scene.clock+5.0;

    IdleLoop idle(hooks, LoopOptions());
    scene.canvas->redraw();
    idle.iterate();
    CHECK(idle.frames() == 0);
    CHECK(idle.waits() == 1);
    drawable=true;
    idle.iterate();
    CHECK(idle.frames() == 1);
}

TEST_CASE("Idle loop: the verify mode draws the frames it would skip and fails one that writes")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setBounds(10, 10, 100, 50);

    LoopOptions verify;
    verify.verify=true;

    SUBCASE("a settled canvas: verify frames that write nothing")
    {
        IdleLoop loop(scene.hooks(), verify);

        scene.settle(loop);
        scene.end=scene.clock+10.0;
        scene.runUntil(loop, scene.end);
        CHECK(loop.verifyFrames() >= 1);
        CHECK(loop.failures() == 0);
        CHECK(scene.failures.empty());
    }

    SUBCASE("work the canvas does not report fails the frame that draws it")
    {
        // nextWorkTime() that misses a pending state change
        bool hide=false;
        IdleLoop::Hooks hooks=scene.hooks();
        hooks.nextWorkTime=[&]() { return hide?controls::Canvas::NoWork:scene.canvas->nextWorkTime(); };

        IdleLoop loop(hooks, verify);

        scene.settle(loop);
        hide=true;
        panel->addStateChange(controls::StateChange_Background);
        scene.end=scene.clock+1.0;
        loop.iterate();

        CHECK(loop.failures() == 1);
        REQUIRE(scene.failures.size() == 1);
        CHECK(scene.failures[0].first == loop.frames()+loop.verifyFrames());
        CHECK(scene.failures[0].second.find("wrote "+std::to_string(test::StubControl::primitivesPerControl)+" primitive") == 0);
    }
}

TEST_CASE("Idle loop: a web tick draws only when work is due or a frame was wanted")
{
    Scene scene;
    IdleLoop loop(scene.hooks(), LoopOptions());

    loop.tick();
    CHECK(loop.frames() == 1);
    for(int i=0; i < 5 && scene.canvas->nextWorkTime() <= scene.clock; ++i)
        loop.tick();

    const size_t settled=loop.frames();

    for(int i=0; i < 30; ++i)
    {
        scene.clock+=1.0/60.0;
        loop.tick();
    }
    CHECK(loop.frames() == settled);
    CHECK(loop.ticks() >= 31);

    scene.canvas->post([]() {});
    loop.tick();
    CHECK(loop.frames() == settled+1);
    loop.tick();
    CHECK(loop.frames() == settled+1);

    loop.wantFrame();
    loop.tick();
    CHECK(loop.frames() == settled+2);
}
