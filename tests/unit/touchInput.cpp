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
 *  deleted mid-drag leaves the drag harmless.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/scrollControl.h>
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
    float clock=10.0f;

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
        clock+=1.0f;
        canvas->updateCanvas();
        canvas->renderCanvas();
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
