/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas::nextWorkTime(), when an idle loop must next update and render a
 *  canvas, on a stepped clock: a settled canvas has no work; it has work now
 *  while a post is pending, after redraw(), with a control on its tree queued
 *  for render (one on another canvas or on none does not count), with layout
 *  or z order dirty, a delayed delete queued or the hover stale, and each of
 *  those is gone after one frame; otherwise it is the earliest of a timer's
 *  deadline, a TimedAnimation's start (then every frame until it ends, as for
 *  an animation that names no time), a focused TextBox's next caret edge
 *  (none once unfocused), the tooltip of a hovered control once its delay has
 *  passed and a held key's next repeat, and a frame at that time does the
 *  work it waited for; a hidden canvas waits for no layout, animation or
 *  tooltip.
 */
#include "headlessFixture.h"

#include <gweni/anim.h>
#include <gweni/gweni.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/toolTip.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

const double WorkNow=controls::Canvas::WorkNow;
const double NoWork=controls::Canvas::NoWork;

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    double clock=10.0;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        settle();
    }

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Frames until the canvas has no work now (at most 10).
    void settle()
    {
        for(int i=0; i < 10 && (canvas->nextWorkTime() <= clock); ++i)
            frame();
        REQUIRE(canvas->nextWorkTime() > clock);
    }

    double next() { return canvas->nextWorkTime(); }
};

//! An animation that names no time: every frame.
class EveryFrame: public anim::Animation
{
public:
    void think() override {}
};

//! Records the frames it starts and finishes on.
class Timed: public anim::TimedAnimation
{
public:
    Timed(int &started, int &finished): TimedAnimation(1.0f, 0.5f), m_started(started), m_finished(finished) {}

    void onStart() override { ++m_started; }
    void onFinish() override { ++m_finished; }

private:
    int &m_started;
    int &m_finished;
};

class KeyCounter: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(KeyCounter, controls::Base)
    {
        setKeyboardInputEnabled(true);
    }

    bool onKeyRight(bool down) override
    {
        if(down)
            ++presses;
        return true;
    }

    int presses=0;
};

} // namespace

TEST_CASE("Next work time: a settled canvas has none")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setBounds(10, 10, 100, 50);
    scene.settle();

    CHECK(scene.next() == NoWork);
    scene.clock+=60.0;
    CHECK(scene.next() == NoWork);
}

TEST_CASE("Next work time: now for each pending change, none after a frame")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setBounds(10, 10, 100, 50);
    scene.settle();

    SUBCASE("a post")
    {
        scene.canvas->post([]() {});
        CHECK(scene.next() == WorkNow);
    }
    SUBCASE("redraw()")
    {
        scene.canvas->redraw();
        CHECK(scene.next() == WorkNow);
    }
    SUBCASE("a control queued for render")
    {
        panel->addStateChange(controls::StateChange_Background);
        CHECK(controls::queuedRenderCount(scene.canvas) == 1);
        CHECK(scene.next() == WorkNow);
    }
    SUBCASE("layout")
    {
        panel->invalidate();
        CHECK(scene.next() == WorkNow);
    }
    SUBCASE("the z order")
    {
        scene.canvas->newChild<controls::Base>("second");
        scene.settle();
        panel->bringToFront();
        CHECK(scene.next() == WorkNow);
    }
    SUBCASE("a delayed delete")
    {
        panel->delayedDelete();
        CHECK(scene.next() == WorkNow);
    }
    SUBCASE("a stale hover")
    {
        scene.canvas->hoverMayHaveMoved();
        CHECK(scene.next() == WorkNow);
    }

    scene.frame();
    CHECK(scene.next() == NoWork);
}

TEST_CASE("Next work time: a control queued off this canvas is not its work")
{
    Scene scene;
    controls::Canvas *other=new controls::Canvas(&scene.skin);
    controls::Base *there=other->newChild<controls::Base>("there");
    controls::Base *detached=scene.canvas->newChild<controls::Base>("detached");

    scene.settle();
    other->updateCanvas();
    other->renderCanvas();
    detached->setParent(nullptr);
    scene.settle();

    there->addStateChange(controls::StateChange_Background);
    detached->addStateChange(controls::StateChange_Background);
    CHECK(controls::queuedRenderCount(scene.canvas) == 0);
    CHECK(controls::queuedRenderCount(other) >= 1);
    CHECK(scene.next() == NoWork);
    CHECK(other->nextWorkTime() == WorkNow);

    delete detached;
    delete other;
}

TEST_CASE("Next work time: a timer's deadline")
{
    Scene scene;
    int runs=0;
    controls::TimerHandle timer=scene.canvas->after(2.0, [&]() { ++runs; });
    const double deadline=scene.clock+2.0;

    scene.canvas->after(5.0, [&]() { ++runs; });
    CHECK(scene.next() == deadline);

    // a frame before it does nothing, one at it fires it
    scene.clock=deadline-0.5;
    scene.frame();
    CHECK(runs == 0);
    scene.clock=deadline;
    scene.frame();
    CHECK(runs == 1);
    CHECK_FALSE(scene.canvas->timerPending(timer));
    CHECK(scene.next() == deadline+3.0);
}

TEST_CASE("Next work time: animations")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    scene.settle();

    SUBCASE("one that names no time wants every frame")
    {
        anim::add(panel, new EveryFrame());
        CHECK(scene.next() == WorkNow);
        anim::cancel(panel);
        CHECK(scene.next() == NoWork);
    }

    SUBCASE("a TimedAnimation waits for its start, then runs every frame until it ends")
    {
        int started=0;
        int finished=0;
        const double start=scene.clock+0.5;

        anim::add(panel, new Timed(started, finished));
        CHECK(scene.next() == start);

        scene.clock=start;
        scene.frame();
        CHECK(started == 1);
        CHECK(scene.next() == WorkNow);

        scene.clock=start+1.0;
        scene.frame();
        CHECK(finished == 1);
        CHECK(scene.next() == NoWork);
    }
}

TEST_CASE("Next work time: a focused TextBox's caret edges, none once unfocused")
{
    Scene scene;
    controls::TextBox *box=scene.canvas->newChild<controls::TextBox>("box");
    box->setText("hello");
    scene.settle();

    box->focus();
    scene.settle();
    REQUIRE(box->isCaretVisible());

    // lit for 1.5 s, then an edge every 0.5 s
    double edge=scene.next();
    CHECK(edge == doctest::Approx(scene.clock+1.5));

    for(int i=0; i < 4; ++i)
    {
        const bool lit=box->isCaretVisible();

        scene.clock=edge;
        scene.frame();
        CHECK(box->isCaretVisible() != lit);
        scene.settle();

        const double nextEdge=scene.next();
        CHECK(nextEdge == doctest::Approx(edge+0.5));
        edge=nextEdge;
    }

    box->unfocus();
    scene.settle();
    CHECK(scene.next() == NoWork);
}

TEST_CASE("Next work time: a hovered control's tooltip once its delay has passed")
{
    Scene scene;
    controls::Base *control=scene.canvas->newChild<controls::Base>();
    control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    control->setBounds(20, 20, 60, 20);
    control->setToolTipText("tip");
    scene.settle();

    scene.canvas->inputMouseMoved(50, 30, 0, 0);
    // the hover's new owner is taken by the next update
    CHECK(scene.next() == WorkNow);
    scene.settle();

    const double due=scene.next();
    CHECK(due == doctest::Approx(scene.clock+scene.canvas->getToolTipDelay()));

    auto shown=[&]() {
        controls::ToolTip *box=scene.canvas->toolTipControl();
        return box && box->visible();
    };

    scene.clock=due;
    scene.frame();
    CHECK(shown());
    scene.settle();
    CHECK(scene.next() == NoWork);
}

TEST_CASE("Next work time: a held key's next repeat")
{
    Scene scene;
    KeyCounter *counter=scene.canvas->newChild<KeyCounter>("counter");
    scene.settle();
    counter->focus();
    scene.settle();

    scene.canvas->inputModifierKey(keys::Right, true);
    REQUIRE(counter->presses == 1);
    scene.settle();

    // past the 0.3 s repeat delay
    double due=scene.next();
    CHECK(due > scene.clock+0.3);
    CHECK(due == doctest::Approx(scene.clock+0.3));
    CHECK(input::nextKeyRepeatTime(scene.canvas) == due);

    scene.clock=due;
    scene.frame();
    CHECK(counter->presses == 2);
    due=scene.next();
    CHECK(due == doctest::Approx(scene.clock+0.03));

    scene.canvas->inputModifierKey(keys::Right, false);
    scene.settle();
    CHECK(scene.next() == NoWork);
}

TEST_CASE("Next work time: a hidden canvas waits for no layout or animation, still for a timer")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    scene.settle();

    scene.canvas->hide();
    scene.settle();
    panel->invalidate();
    anim::add(panel, new EveryFrame());
    CHECK(scene.next() == NoWork);

    scene.canvas->after(1.0, []() {});
    CHECK(scene.next() == scene.clock+1.0);
    anim::cancel(panel);
}
