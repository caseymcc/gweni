/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  input::resetPointerState(): the input handler's process-wide pointer state
 *  (the last click's time and point, the mouse position and canvas, the held
 *  buttons and keys) is forgotten between canvases, so a fresh canvas inherits
 *  nothing of an earlier one's. On a real Canvas with a StubRenderer:
 *  - two scenes one after the other, each restarting its clock at the same
 *    time and pressing the same point, with the reset between them, get two
 *    single clicks and no double-click (without it the second press is the
 *    second half of a double-click);
 *  - a left button pressed and never released, and a key held, are not down
 *    after the reset, and the mouse position is (0,0).
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/gweni.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

//! Counts the presses and double-clicks it gets.
class ClickCounter: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(ClickCounter, controls::Base)
    {
        setMouseInputEnabled(true);
    }

    void onMouseClickLeft(int, int, bool down) override
    {
        if(down)
            ++presses;
    }

    void onMouseDoubleClickLeft(int, int) override
    {
        ++doubleClicks;
    }

    int presses=0;
    int doubleClicks=0;
};

//! A canvas with one ClickCounter, on a clock that starts at the same time
//! every scene.
struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    ClickCounter *counter=nullptr;
    double clock=5.0;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(200, 100);
        counter=canvas->newChild<ClickCounter>();
        counter->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        counter->setBounds(10, 10, 80, 40);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! a press and release at (30,30), the counter's
    void click()
    {
        canvas->inputMouseMoved(30, 30, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }
};

}//namespace

TEST_CASE("Pointer reset: two scenes clicking the same point at the same clock get two single clicks")
{
    input::resetPointerState();
    {
        Scene first;

        first.click();
        CHECK(first.counter->presses == 1);
        CHECK(first.counter->doubleClicks == 0);
    }

    input::resetPointerState();
    {
        Scene second;

        second.click();
        CHECK(second.counter->presses == 1);
        CHECK(second.counter->doubleClicks == 0);
    }
}

TEST_CASE("Pointer reset: held buttons and keys and the mouse position are forgotten")
{
    input::resetPointerState();
    {
        Scene scene;

        scene.canvas->inputMouseMoved(30, 30, 0, 0);
        scene.canvas->inputMouseButton(0, true);
        scene.canvas->inputModifierKey(keys::Shift, true);
        CHECK(input::isLeftMouseDown());
        CHECK(input::isKeyDown(keys::Shift));
        CHECK(input::getMousePosition().x == 30);

        // the canvas goes with its press and key still held, as a test's or
        // an app's last frame can leave them
        gweni::MouseFocus=nullptr;
    }

    input::resetPointerState();
    CHECK_FALSE(input::isLeftMouseDown());
    CHECK_FALSE(input::isKeyDown(keys::Shift));
    CHECK(input::getMousePosition().x == 0);
    CHECK(input::getMousePosition().y == 0);
}
