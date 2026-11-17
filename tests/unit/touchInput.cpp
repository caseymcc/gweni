/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas::inputTouch on a real Canvas with a StubRenderer. A finger lifted
 *  within Canvas::TouchSlop of its down is a tap: the Button under the down
 *  point is pressed once (a move there and a lift elsewhere within the slop
 *  included), the canvas's input is given back and nothing is left hovered.
 *  A finger moved the slop or more presses nothing, and pans: a ScrollControl
 *  under it scrolls by the finger's whole units (the content following the
 *  finger, the fractions kept from move to move, the offset stopping at the
 *  range's ends), a ZoomPanel pans by the finger's motion exactly, and an
 *  axis the ScrollControl cannot move along (it is at its end, or scrolls the
 *  other way) goes to the ZoomPanel holding it, each axis kept by the
 *  control it went to until the lift. A control that keepsTouchDrag() gets a
 *  left-button mouse drag: a move to the down point and a press there, the
 *  moves, a release where the finger lifts, and no hover after. A second
 *  finger is taken and dropped while the first is down; a cancel presses
 *  nothing; an input capture set when a finger goes down gets that finger's
 *  every event (captureTouch) and nothing on the canvas does, and one set
 *  mid-gesture takes the finger with no tap replayed; a control panned
 *  deleted mid-drag leaves the drag harmless. Lifted while moving, a drag
 *  flings on: the list goes on scrolling, each frame by no more than the
 *  one before, about the speed times Canvas::FlingTimeConstant in all, and
 *  stops within a bounded number of frames with no animation left; at the
 *  end of the range it stops there at once; a finger held still before the
 *  lift flings nothing; a finger put down stops a fling, and its lift is no
 *  tap. Held still for Canvas::LongPressSeconds, and not before, a finger
 *  opens the context menu of the control under it with its corner at the
 *  finger, and the lift chooses nothing (nothing is even hit-tested); one
 *  moved past the slop is no long press, nor one lifted sooner, whose delay
 *  goes with it. A TextBox keeps the drag only while it has the keyboard
 *  focus: a drag starting on an unfocused one in a ScrollControl scrolls it
 *  and leaves the box unfocused, one in the focused box selects its text and
 *  scrolls nothing.
 */
#include "headlessFixture.h"

#include <gweni/anim.h>
#include <gweni/controls/button.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/zoomPanel.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/inputEvent.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! Writes down the mouse events it gets and whether it was hovered.
class DragTarget: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(DragTarget, controls::Base)
    {
        setMouseInputEnabled(true);
    }

    bool keepsTouchDrag() const override { return true; }

    void onMouseClickLeft(int x, int y, bool down) override
    {
        events.push_back(std::string(down?"press ":"release ")+std::to_string(x)+","+std::to_string(y));
        if(down)
            MouseFocus=this;
        else if(MouseFocus == this)
            MouseFocus=nullptr;
    }

    void onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/) override
    {
        events.push_back("move "+std::to_string(x)+","+std::to_string(y));
    }

    std::vector<std::string> events;
};

//! Writes down each touch and mouse button it gets.
class TouchRecorder: public controls::InputCapture
{
public:
    void captureTouch(int id, TouchPhase phase, float x, float y) override
    {
        static const char *const names[]={"down", "move", "up", "cancel"};

        events.push_back(std::to_string(id)+" "+names[static_cast<int>(phase)]+" "
            +std::to_string(int(x))+","+std::to_string(int(y)));
    }

    void captureMouseButton(int button, bool down) override
    {
        events.push_back("button "+std::to_string(button)+(down?" down":" up"));
    }

    std::vector<std::string> events;
};

struct Scene: public event::Handler
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Button *button=nullptr;
    controls::ZoomPanel *panel=nullptr;
    controls::ScrollControl *scroll=nullptr;
    controls::Button *row=nullptr;
    DragTarget *dragTarget=nullptr;
    int presses=0;
    int rowPresses=0;
    double clock=10.0;
    controls::Menu *menu=nullptr;

    Scene()
    {
        // a second apart per frame, so two taps are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        button=canvas->newChild<controls::Button>();
        button->setText("Button");
        button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        button->setBounds(10, 10, 80, 30);
        button->onPressCaller.add(this, &Scene::pressed);
        menu=button->createContextMenu();
        menu->addItem("One");
        menu->addItem("Two");

        dragTarget=canvas->newChild<DragTarget>();
        dragTarget->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        dragTarget->setBounds(10, 250, 80, 30);

        panel=canvas->newChild<controls::ZoomPanel>();
        panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        panel->setBounds(150, 10, 240, 280);
        panel->setContentSize(Point(600, 600));

        scroll=panel->newChild<controls::ScrollControl>();
        scroll->setScroll(false, true);
        scroll->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        scroll->setBounds(10, 10, 150, 120);

        // rows 30 high, 900 in all: the offset ranges far
        for(int i=0; i < 30; ++i)
        {
            controls::Button *child=scroll->newChild<controls::Button>();

            child->setText("Row "+std::to_string(i));
            child->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
            child->setBounds(0, i*30, 120, 30);
            child->onPressCaller.add(this, &Scene::rowPressed);
            if(!row)
                row=child;
        }
        frame();
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        canvas->setInputCapture(nullptr);
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void pressed(event::Info) { ++presses; }
    void rowPressed(event::Info) { ++rowPresses; }

    void frame()
    {
        step(1.0);
    }

    void step(double seconds)
    {
        clock+=seconds;
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    // a finger dragged from `at` by `by` a move, `moves` moves a 60th of a
    // second apart, each followed by a frame, and lifted in the last one's
    void flick(const PointF &at, const PointF &by, int moves)
    {
        canvas->inputTouch(0, TouchPhase::Down, at.x, at.y);
        for(int i=1; i <= moves; ++i)
        {
            step(1.0/60.0);
            canvas->inputTouch(0, TouchPhase::Move, at.x+by.x*i, at.y+by.y*i);
        }
        canvas->inputTouch(0, TouchPhase::Up, at.x+by.x*moves, at.y+by.y*moves);
    }

    // canvas units of a point in the scroll control
    PointF inScroll(float x, float y) const
    {
        const Point origin=scroll->localPosToCanvas();
        return PointF(origin.x+x, origin.y+y);
    }
};

}//namespace

TEST_CASE("Touch: a tap presses the control under the finger once and leaves nothing hovered")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    CHECK(canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f));
    CHECK(scene.presses == 0);
    CHECK(canvas->inputCapture() != nullptr);
    CHECK(canvas->inputTouch(0, TouchPhase::Up, 30.0f, 20.0f));
    CHECK(scene.presses == 1);
    CHECK(canvas->inputCapture() == nullptr);
    CHECK(HoveredControl == nullptr);
    CHECK(!scene.button->isDepressed());
    scene.frame();

    // moved within the slop, lifted elsewhere: still a tap, at the down point
    canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f);
    canvas->inputTouch(0, TouchPhase::Move, 34.0f, 24.0f);
    canvas->inputTouch(0, TouchPhase::Up, 35.0f, 25.0f);
    CHECK(scene.presses == 2);
    CHECK(HoveredControl == nullptr);
}

TEST_CASE("Touch: a drag past the slop presses nothing")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f);
    canvas->inputTouch(0, TouchPhase::Move, 30.0f+controls::Canvas::TouchSlop, 20.0f);
    canvas->inputTouch(0, TouchPhase::Move, 30.0f, 20.0f);
    canvas->inputTouch(0, TouchPhase::Up, 30.0f, 20.0f);
    CHECK(scene.presses == 0);
    CHECK(!scene.button->isDepressed());
    CHECK(canvas->inputCapture() == nullptr);
    CHECK(HoveredControl == nullptr);

    // nor a cancel within the slop
    canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f);
    canvas->inputTouch(0, TouchPhase::Cancel, 30.0f, 20.0f);
    CHECK(scene.presses == 0);
    CHECK(canvas->inputCapture() == nullptr);
}

TEST_CASE("Touch: a ScrollControl follows the finger in whole units, the fractions kept")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    const PointF at=scene.inScroll(60.0f, 60.0f);
    const PointF pan=scene.panel->pan();

    REQUIRE(scene.scroll->getMaxVerticalScrollOffset() > 100);
    canvas->inputTouch(0, TouchPhase::Down, at.x, at.y);

    // up 2.5 units a move: nothing under the slop, then the finger's whole units
    for(int i=1; i <= 8; ++i)
    {
        const float moved=2.5f*i;

        canvas->inputTouch(0, TouchPhase::Move, at.x, at.y-moved);
        CHECK(scene.scroll->getVerticalScrollOffset() == (moved < controls::Canvas::TouchSlop?0:int(moved)));
    }
    scene.frame();

    // the panel gets none of it
    CHECK(scene.panel->pan().x == pan.x);
    CHECK(scene.panel->pan().y == pan.y);

    // past the range's end the offset stops there, and the axis stays the
    // ScrollControl's: the panel does not take what it could not
    const int maxOffset=scene.scroll->getMaxVerticalScrollOffset();

    canvas->inputTouch(0, TouchPhase::Move, at.x, at.y-float(maxOffset)-200.0f);
    CHECK(scene.scroll->getVerticalScrollOffset() == maxOffset);
    CHECK(scene.panel->pan().y == pan.y);
    canvas->inputTouch(0, TouchPhase::Up, at.x, at.y-float(maxOffset)-200.0f);
    CHECK(scene.rowPresses == 0);
    CHECK(canvas->inputCapture() == nullptr);
}

TEST_CASE("Touch: an axis a ScrollControl cannot move along goes to the ZoomPanel holding it")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    const PointF at=scene.inScroll(60.0f, 60.0f);
    const PointF pan=scene.panel->pan();

    // at its top a finger moving down asks for rows above the first, and it
    // scrolls only vertically: both axes are the panel's, unit for unit
    canvas->inputTouch(0, TouchPhase::Down, at.x, at.y);
    canvas->inputTouch(0, TouchPhase::Move, at.x+10.5f, at.y+20.25f);
    CHECK(scene.scroll->getVerticalScrollOffset() == 0);
    CHECK(scene.panel->pan().x == pan.x+10.5f);
    CHECK(scene.panel->pan().y == pan.y+20.25f);

    // once the panel has the axis, moving back up pans it, not the list
    canvas->inputTouch(0, TouchPhase::Move, at.x+10.5f, at.y-30.0f);
    CHECK(scene.scroll->getVerticalScrollOffset() == 0);
    CHECK(scene.panel->pan().y == pan.y-30.0f);
    canvas->inputTouch(0, TouchPhase::Up, at.x+10.5f, at.y-30.0f);
    CHECK(scene.rowPresses == 0);
}

TEST_CASE("Touch: a control that keeps the drag gets a left-button mouse drag")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    CHECK(canvas->inputTouch(0, TouchPhase::Down, 20.0f, 260.0f));
    CHECK(canvas->inputCapture() == nullptr);
    canvas->inputTouch(0, TouchPhase::Move, 40.0f, 262.0f);
    canvas->inputTouch(0, TouchPhase::Move, 60.4f, 263.6f);
    canvas->inputTouch(0, TouchPhase::Up, 60.4f, 263.6f);

    const std::vector<std::string> expected{"move 20,260", "press 20,260", "move 40,262", "move 60,264", "release 60,264"};

    CHECK(scene.dragTarget->events == expected);
    CHECK(HoveredControl == nullptr);
    CHECK(MouseFocus == nullptr);
}

TEST_CASE("Touch: a second finger is ignored while the first is down")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f);
    CHECK(canvas->inputTouch(1, TouchPhase::Down, 30.0f, 20.0f));
    CHECK(canvas->inputTouch(1, TouchPhase::Move, 300.0f, 200.0f));
    CHECK(canvas->inputTouch(1, TouchPhase::Up, 300.0f, 200.0f));
    CHECK(scene.presses == 0);
    CHECK(canvas->inputCapture() != nullptr);
    canvas->inputTouch(0, TouchPhase::Up, 30.0f, 20.0f);
    CHECK(scene.presses == 1);

    // with no finger down, a stray move or lift is nobody's
    CHECK(!canvas->inputTouch(3, TouchPhase::Move, 30.0f, 20.0f));
    CHECK(!canvas->inputTouch(3, TouchPhase::Up, 30.0f, 20.0f));
    CHECK(scene.presses == 1);
}

TEST_CASE("Touch: an input capture set at the down gets the finger and nothing on the canvas does")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    TouchRecorder recorder;

    canvas->setInputCapture(&recorder);
    CHECK(canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f));
    CHECK(canvas->inputTouch(0, TouchPhase::Move, 50.0f, 40.0f));
    CHECK(canvas->inputTouch(0, TouchPhase::Up, 50.0f, 40.0f));

    const std::vector<std::string> expected{"0 down 30,20", "0 move 50,40", "0 up 50,40"};

    CHECK(recorder.events == expected);
    CHECK(scene.presses == 0);
    CHECK(canvas->inputCapture() == &recorder);
    canvas->setInputCapture(nullptr);

    // a capture set mid-gesture (the depth view opened) takes the finger from
    // the gesture: it goes nowhere, and no tap is replayed
    canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f);
    canvas->setInputCapture(&recorder);
    canvas->inputTouch(0, TouchPhase::Up, 30.0f, 20.0f);
    CHECK(recorder.events == expected);
    CHECK(scene.presses == 0);
    CHECK(canvas->inputCapture() == &recorder);
}

TEST_CASE("Touch: a ScrollControl deleted mid-drag leaves the drag harmless")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    const PointF at=scene.inScroll(60.0f, 60.0f);
    const PointF pan=scene.panel->pan();

    canvas->inputTouch(0, TouchPhase::Down, at.x, at.y);
    canvas->inputTouch(0, TouchPhase::Move, at.x, at.y-20.0f);
    REQUIRE(scene.scroll->getVerticalScrollOffset() == 20);
    scene.scroll->delayedDelete();
    scene.frame();
    canvas->inputTouch(0, TouchPhase::Move, at.x, at.y-40.0f);
    canvas->inputTouch(0, TouchPhase::Up, at.x, at.y-40.0f);
    CHECK(scene.panel->pan().y == pan.y);
    CHECK(canvas->inputCapture() == nullptr);
}

TEST_CASE("Touch: a drag lifted while moving flings on, slowing to a stop with no animation left")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    // 10 units a 60th: 600 units a second, up the list
    scene.flick(scene.inScroll(60.0f, 100.0f), PointF(0.0f, -10.0f), 6);

    const int lifted=scene.scroll->getVerticalScrollOffset();

    CHECK(lifted == 60);
    CHECK(anim::activeCount(scene.scroll) == 1);

    int frames=0;
    int last=lifted;
    int lastStep=1000;
    bool slowing=true;

    while(anim::activeCount(scene.scroll) && frames < 600)
    {
        scene.step(1.0/60.0);
        ++frames;

        const int stepped=scene.scroll->getVerticalScrollOffset()-last;

        // whole units each frame, so a step may be one more than the last
        if(stepped < 0 || stepped > lastStep+1)
            slowing=false;
        lastStep=stepped;
        last=scene.scroll->getVerticalScrollOffset();
    }

    // it goes about speed*FlingTimeConstant, less what is left under StopFlingSpeed
    const int travelled=last-lifted;

    CHECK(slowing);
    CHECK(frames < 120);
    CHECK(travelled >= 180);
    CHECK(travelled <= int(600*controls::Canvas::FlingTimeConstant));
    CHECK(anim::activeCount(scene.scroll) == 0);
    CHECK(scene.rowPresses == 0);

    // settled: the next frames move nothing
    scene.step(1.0/60.0);
    CHECK(scene.scroll->getVerticalScrollOffset() == last);
    CHECK(canvas->inputCapture() == nullptr);
}

TEST_CASE("Touch: a finger put down stops a fling, and its lift is no tap")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    const PointF at=scene.inScroll(60.0f, 100.0f);

    scene.flick(at, PointF(0.0f, -10.0f), 6);
    scene.step(1.0/60.0);
    scene.step(1.0/60.0);
    REQUIRE(anim::activeCount(scene.scroll) == 1);

    canvas->inputTouch(0, TouchPhase::Down, at.x, at.y);
    CHECK(anim::activeCount(scene.scroll) == 0);

    const int stopped=scene.scroll->getVerticalScrollOffset();

    scene.step(1.0/60.0);
    scene.step(1.0/60.0);
    CHECK(scene.scroll->getVerticalScrollOffset() == stopped);
    canvas->inputTouch(0, TouchPhase::Up, at.x, at.y);
    CHECK(scene.rowPresses == 0);
    CHECK(canvas->inputCapture() == nullptr);

    // the next one is a tap again
    scene.frame();
    canvas->inputTouch(0, TouchPhase::Down, at.x, at.y);
    canvas->inputTouch(0, TouchPhase::Up, at.x, at.y);
    CHECK(scene.rowPresses == 1);
}

TEST_CASE("Touch: a finger held still before its lift flings nothing")
{
    Scene scene;
    const PointF at=scene.inScroll(60.0f, 100.0f);
    controls::Canvas *canvas=scene.canvas;

    canvas->inputTouch(0, TouchPhase::Down, at.x, at.y);
    for(int i=1; i <= 6; ++i)
    {
        scene.step(1.0/60.0);
        canvas->inputTouch(0, TouchPhase::Move, at.x, at.y-10.0f*i);
    }
    // 0.15 s still: the window's samples are where the finger rests
    for(int i=0; i < 9; ++i)
        scene.step(1.0/60.0);
    canvas->inputTouch(0, TouchPhase::Up, at.x, at.y-60.0f);
    CHECK(anim::activeCount(scene.scroll) == 0);
    scene.step(1.0/60.0);
    CHECK(scene.scroll->getVerticalScrollOffset() == 60);
}

TEST_CASE("Touch: a fling stops at the end of the range")
{
    Scene scene;
    const int maxOffset=scene.scroll->getMaxVerticalScrollOffset();

    scene.scroll->setVerticalScrollOffset(maxOffset-80);
    scene.frame();
    scene.flick(scene.inScroll(60.0f, 100.0f), PointF(0.0f, -10.0f), 6);
    CHECK(scene.scroll->getVerticalScrollOffset() == maxOffset-20);

    int frames=0;

    while(anim::activeCount(scene.scroll) && frames < 600)
    {
        scene.step(1.0/60.0);
        ++frames;
    }
    CHECK(scene.scroll->getVerticalScrollOffset() == maxOffset);
    // stopped by the end, well before its speed fell away
    CHECK(frames < 20);
}

TEST_CASE("Touch: a long press opens the context menu at the finger after the delay and its lift chooses nothing")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f);
    scene.step(controls::Canvas::LongPressSeconds-0.01);
    CHECK(canvas->shownContextMenu() == nullptr);
    scene.step(0.01);
    REQUIRE(canvas->shownContextMenu() == scene.menu);
    CHECK(canvas->contextMenuOwner() == scene.button);

    const Point origin=scene.menu->localPosToCanvas();

    CHECK(origin.x == 30);
    CHECK(origin.y == 20);

    // moved and lifted over the menu: nothing chosen, nothing pressed, nothing
    // under the finger so much as hit-tested
    const size_t hitTests=input::hitTestVisits();

    canvas->inputTouch(0, TouchPhase::Move, 60.0f, 40.0f);
    canvas->inputTouch(0, TouchPhase::Up, 60.0f, 40.0f);
    CHECK(input::hitTestVisits() == hitTests);
    scene.frame();
    CHECK(canvas->shownContextMenu() == scene.menu);
    CHECK(scene.presses == 0);
    CHECK(canvas->inputCapture() == nullptr);
}

TEST_CASE("Touch: a finger moved past the slop or lifted before the delay is no long press, and the next waits its own delay")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f);
    canvas->inputTouch(0, TouchPhase::Move, 30.0f+controls::Canvas::TouchSlop, 20.0f);
    scene.step(1.0);
    CHECK(canvas->shownContextMenu() == nullptr);
    canvas->inputTouch(0, TouchPhase::Up, 30.0f+controls::Canvas::TouchSlop, 20.0f);

    // lifted at half the delay and put down again: the first finger's delay
    // is gone with it, and the second one waits its own whole delay
    canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f);
    scene.step(controls::Canvas::LongPressSeconds/2);
    canvas->inputTouch(0, TouchPhase::Up, 30.0f, 20.0f);
    CHECK(scene.presses == 1);
    canvas->inputTouch(0, TouchPhase::Down, 30.0f, 20.0f);
    scene.step(controls::Canvas::LongPressSeconds*0.75);
    CHECK(canvas->shownContextMenu() == nullptr);
    scene.step(controls::Canvas::LongPressSeconds*0.25);
    CHECK(canvas->shownContextMenu() == scene.menu);
    canvas->inputTouch(0, TouchPhase::Up, 30.0f, 20.0f);
    CHECK(scene.presses == 1);
}

TEST_CASE("Touch: a TextBox keeps the drag only while focused (it selects) and a drag on an unfocused one scrolls")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    auto *scroll=canvas->newChild<controls::ScrollControl>();

    scroll->setScroll(false, true);
    scroll->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    scroll->setBounds(10, 50, 130, 180);

    auto *box=scroll->newChild<controls::TextBox>();

    box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    box->setBounds(0, 0, 120, 24);
    box->setText("hello touch world");

    auto *filler=scroll->newChild<controls::Base>();

    filler->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    filler->setBounds(0, 24, 120, 600);
    scene.frame();
    scene.frame();
    REQUIRE(scroll->getMaxVerticalScrollOffset() > 100);
    REQUIRE(KeyboardFocus != box);

    const Point origin=box->localPosToCanvas();
    const float x=float(origin.x)+10.0f;
    const float y=float(origin.y)+12.0f;

    // unfocused: the drag scrolls what holds the box, and focuses nothing
    canvas->inputTouch(0, TouchPhase::Down, x, y);
    canvas->inputTouch(0, TouchPhase::Move, x, y-20.0f);
    canvas->inputTouch(0, TouchPhase::Move, x, y-40.0f);
    scene.step(0.6);
    canvas->inputTouch(0, TouchPhase::Up, x, y-40.0f);
    CHECK(scroll->getVerticalScrollOffset() == 40);
    CHECK(KeyboardFocus != box);
    CHECK(box->getCursorPos() == box->getCursorEnd());

    scroll->setVerticalScrollOffset(0);
    scene.frame();
    box->focus();
    box->setCursorPos(0);
    box->setCursorend(0);
    scene.frame();

    // focused: a drag along it selects, the ScrollControl stays put
    const Point at=box->localPosToCanvas();

    canvas->inputTouch(0, TouchPhase::Down, float(at.x)+4.0f, float(at.y)+12.0f);
    canvas->inputTouch(0, TouchPhase::Move, float(at.x)+40.0f, float(at.y)+12.0f);
    canvas->inputTouch(0, TouchPhase::Move, float(at.x)+80.0f, float(at.y)+40.0f);
    canvas->inputTouch(0, TouchPhase::Up, float(at.x)+80.0f, float(at.y)+40.0f);
    CHECK(KeyboardFocus == box);
    CHECK(box->getCursorPos() != box->getCursorEnd());
    CHECK(scroll->getVerticalScrollOffset() == 0);
}
