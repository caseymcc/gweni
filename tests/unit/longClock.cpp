/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The clock is a double of seconds since start (platform::getTimeInSeconds()),
 *  so a page left open for a month still resolves a frame: a float of seconds
 *  steps by 0.25 s at 30 days, and a 1/60 s step there would not move it at all.
 *  With the time source at 30 days plus whole 1/60 s steps, read as
 *  base+frame*(1.0/60) in double as the captures' stepped clocks do: a focused
 *  TextBox's caret stays lit 1.5 s and then flips every 0.5 s, the canvas's
 *  tooltip shows once its 0.5 s delay has passed, a held key repeats after
 *  0.3 s and then every other frame (its 0.03 s rate, under two frames), a
 *  TimedAnimation starts after its delay and finishes after its length, and a
 *  canvas timer (Canvas::after(), every()) fires after its delay and then each
 *  period, each within one frame of the time it asks for.
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

//! 30 days, in seconds.
const double Month=30.0*24.0*3600.0;

//! A control that counts its Right key presses, repeats included.
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
            presses.push_back(frame ? *frame : -1);
        return true;
    }

    const int *frame=nullptr;
    std::vector<int> presses;
};

//! Records the frames it starts and finishes on.
class Timed: public anim::TimedAnimation
{
public:
    Timed(float length, float delay, const int &frame, int &started, int &finished):
        TimedAnimation(length, delay), m_frame(frame), m_started(started), m_finished(finished)
    {}

    void onStart() override { m_started=m_frame; }
    void onFinish() override { m_finished=m_frame; }

private:
    const int &m_frame;
    int &m_started;
    int &m_finished;
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    int frameNumber=0;

    Scene()
    {
        platform::setTimeSource([this]() { return Month+double(frameNumber)*(1.0/60.0); });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        // the hovered and focused controls are process-global
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    //! Steps the clock 1/60 s and renders a frame.
    void step()
    {
        ++frameNumber;
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

//! \p frames is within one frame of \p expected.
bool withinAFrame(int frames, int expected)
{
    return frames == expected || frames == expected+1;
}

} // namespace

TEST_CASE("Long clock: the clock still steps by a frame 30 days in")
{
    Scene scene;
    const double before=platform::getTimeInSeconds();

    scene.step();
    CHECK(platform::getTimeInSeconds()-before == doctest::Approx(1.0/60.0).epsilon(1e-6));
}

TEST_CASE("Long clock: a focused TextBox's caret blinks on time 30 days in")
{
    Scene scene;
    controls::TextBox *box=scene.canvas->newChild<controls::TextBox>("box");
    box->setText("hello");
    scene.step();

    box->focus();
    const int focused=scene.frameNumber;
    scene.step();
    REQUIRE(box->isCaretVisible());

    std::vector<int> flips;
    bool lit=box->isCaretVisible();

    while(scene.frameNumber < focused+60*5)
    {
        scene.step();
        if(box->isCaretVisible() != lit)
        {
            lit=box->isCaretVisible();
            flips.push_back(scene.frameNumber);
        }
    }

    // lit for SolidTime (1.5 s, 90 frames), then a flip every Period (0.5 s)
    REQUIRE(flips.size() >= 6);
    INFO("first flip ", flips[0]-focused, " frames after the focus");
    CHECK(withinAFrame(flips[0]-focused, 90));
    for(size_t i=1; i < flips.size(); ++i)
    {
        INFO("flip ", i, " ", flips[i]-flips[i-1], " frames after the one before");
        CHECK(withinAFrame(flips[i]-flips[i-1], 30));
    }
}

TEST_CASE("Long clock: the tooltip shows after its delay 30 days in")
{
    Scene scene;
    controls::Base *control=scene.canvas->newChild<controls::Base>();
    control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    control->setBounds(20, 20, 60, 20);
    control->setToolTipText("tip");
    scene.step();

    scene.canvas->inputMouseMoved(50, 30, 0, 0);
    const int moved=scene.frameNumber;
    scene.step();

    auto shown=[&]() {
        controls::ToolTip *box=scene.canvas->toolTipControl();
        return box && box->visible();
    };

    while(!shown() && scene.frameNumber < moved+120)
        scene.step();

    REQUIRE(shown());
    // the delay is 0.5 s, 30 frames
    INFO("shown ", scene.frameNumber-moved, " frames after the move");
    CHECK(scene.canvas->getToolTipDelay() == doctest::Approx(0.5));
    CHECK(withinAFrame(scene.frameNumber-moved, 30));
}

TEST_CASE("Long clock: a held key repeats on time 30 days in")
{
    Scene scene;
    KeyCounter *counter=scene.canvas->newChild<KeyCounter>("counter");
    counter->frame=&scene.frameNumber;
    scene.step();
    counter->focus();
    REQUIRE(KeyboardFocus == counter);

    scene.canvas->inputModifierKey(keys::Right, true);
    const int pressed=scene.frameNumber;

    while(scene.frameNumber < pressed+60)
        scene.step();
    scene.canvas->inputModifierKey(keys::Right, false);

    const std::vector<int> &presses=counter->presses;

    // the press itself, then the first repeat after 0.3 s (18 frames; it
    // repeats once the clock is past it) and one every 0.03 s, every other
    // frame at 60 a second
    REQUIRE(presses.size() >= 10);
    CHECK(presses[0] == pressed);
    INFO("first repeat ", presses[1]-pressed, " frames after the press");
    CHECK(withinAFrame(presses[1]-pressed, 18));
    for(size_t i=2; i < presses.size(); ++i)
    {
        INFO("repeat ", i, " ", presses[i]-presses[i-1], " frames after the one before");
        CHECK(presses[i]-presses[i-1] == 2);
    }
    CHECK(withinAFrame(int(presses.size()), 1+(60-18)/2));
}

TEST_CASE("Long clock: a TimedAnimation starts and finishes on time 30 days in")
{
    Scene scene;
    controls::Base *control=scene.canvas->newChild<controls::Base>();
    scene.step();

    int started=-1;
    int finished=-1;
    const int added=scene.frameNumber;

    // 1 s long, starting 0.5 s after it is made
    anim::add(control, new Timed(1.0f, 0.5f, scene.frameNumber, started, finished));

    while(finished < 0 && scene.frameNumber < added+200)
        scene.step();

    INFO("started ", started-added, ", finished ", finished-added, " frames after it was made");
    CHECK(withinAFrame(started-added, 30));
    CHECK(withinAFrame(finished-added, 90));
}

TEST_CASE("Long clock: canvas timers fire on time 30 days in")
{
    Scene scene;
    scene.step();

    const int added=scene.frameNumber;
    int once=-1;
    std::vector<int> repeats;

    // 0.25 s (15 frames) once, and every 0.5 s (30 frames)
    scene.canvas->after(0.25, [&]() { once=scene.frameNumber; });
    scene.canvas->every(0.5, [&]() { repeats.push_back(scene.frameNumber); });

    while(scene.frameNumber < added+60*5)
        scene.step();

    INFO("once ", once-added, " frames after it was added");
    CHECK(withinAFrame(once-added, 15));
    REQUIRE(repeats.size() >= 9);
    CHECK(withinAFrame(repeats[0]-added, 30));
    for(size_t i=1; i < repeats.size(); ++i)
    {
        INFO("repeat ", i, " ", repeats[i]-repeats[i-1], " frames after the one before");
        // each deadline is the first plus whole periods, so a frame late
        // once is not late for the next
        CHECK(withinAFrame(repeats[i]-added, 30*int(i+1)));
    }
}
