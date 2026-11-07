/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Mouse capture and hover, through the input handler on a real Canvas: while
 *  a control holds the mouse (MouseFocus) it is the hovered control, so a move
 *  sends no leave or enter to anything and marks no other control, and a
 *  control with a tooltip the mouse crosses does not become the tooltip's
 *  owner; capture taken while another control is hovered gives that one a
 *  single leave. When the capture ends, by the release or by its holder being
 *  hidden, hover is hit-tested once and goes to the control under the mouse. A
 *  Button released off itself is not clicked (a toggle does not toggle), one
 *  released on itself is clicked once.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/dragger.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

//! Counts a Caller's calls.
class Counter: public event::Handler
{
public:
    void count(event::Info)
    {
        ++calls;
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
};

//! A control's hover enters and leaves.
struct HoverCount
{
    Counter enters, leaves;

    explicit HoverCount(controls::Base *control)
    {
        enters.listen(control->onHoverEnter);
        leaves.listen(control->onHoverLeave);
    }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    float clock=nextClock();

    //! The double-click state is process-global too: each scene starts its
    //! clock far past the last one's, so its first press is never a double
    //! click with the last scene's press at the same point.
    static float nextClock()
    {
        static float start=0.0f;
        start+=1000.0f;
        return start;
    }

    Scene()
    {
        // a second apart per click, so two clicks are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the captures and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        input::onMouseClicked(canvas, 1, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    template<typename T>
    T *place(controls::Base *parent, int x, int y, int w, int h)
    {
        T *control=parent->newChild<T>();
        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(x, y, w, h);
        return control;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Moves the mouse and runs the update phase, but not the render, so the
    //! state changes the move raised can still be read.
    void moveUpdate(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        canvas->updateCanvas();
    }

    void moveTo(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        frame();
    }

    void press(int button=0)
    {
        clock+=1.0f;
        canvas->inputMouseButton(button, true);
        frame();
    }

    void release(int button=0)
    {
        clock+=1.0f;
        canvas->inputMouseButton(button, false);
        frame();
    }
};

} // namespace

TEST_CASE("Mouse capture: a drag sends no hover leave or enter and marks no other control")
{
    Scene scene;
    controls::Base *parent=scene.place<controls::Base>(scene.canvas, 0, 0, 300, 200);
    controls::internal::Dragger *dragger=scene.place<controls::internal::Dragger>(parent, 10, 10, 20, 20);
    controls::Base *sibling=scene.place<controls::Base>(parent, 100, 10, 50, 50);
    scene.frame();

    HoverCount parentHover(parent), draggerHover(dragger), siblingHover(sibling);
    Counter drags;
    drags.listen(dragger->onDragged);

    scene.moveTo(20, 20);
    REQUIRE(HoveredControl == dragger);
    CHECK(draggerHover.enters.calls == 1);
    draggerHover.enters.calls=0;

    scene.press();
    REQUIRE(MouseFocus == dragger);

    // across the parent, onto the sibling, back onto the parent
    const Point path[]={Point(50, 20), Point(80, 40), Point(120, 30), Point(140, 50), Point(200, 150)};
    for(const Point &at : path)
    {
        scene.moveUpdate(at.x, at.y);
        CHECK(parent->getStateChange() == controls::StateChange_Nothing);
        CHECK(sibling->getStateChange() == controls::StateChange_Nothing);
        CHECK(dragger->getStateChange() == controls::StateChange_Nothing);
        CHECK(HoveredControl == dragger);
        CHECK(dragger->isHovered());
        scene.canvas->renderCanvas();
    }

    CHECK(drags.calls == 5);
    CHECK(parentHover.enters.calls == 0);
    CHECK(parentHover.leaves.calls == 0);
    CHECK(siblingHover.enters.calls == 0);
    CHECK(siblingHover.leaves.calls == 0);
    CHECK(draggerHover.enters.calls == 0);
    CHECK(draggerHover.leaves.calls == 0);

    // released over the sibling: one leave, one enter, hover on the sibling
    scene.moveTo(120, 30);
    scene.canvas->inputMouseButton(0, false);
    CHECK(MouseFocus == nullptr);
    CHECK(HoveredControl == sibling);
    CHECK(draggerHover.leaves.calls == 1);
    CHECK(siblingHover.enters.calls == 1);
    CHECK(parentHover.enters.calls == 0);
    CHECK(parentHover.leaves.calls == 0);
    // the old holder is repainted in the frame of the release
    CHECK((dragger->getStateChange() & controls::StateChange_Background) != 0);
    scene.frame();

    // and the hover follows the mouse again
    scene.moveTo(50, 100);
    CHECK(HoveredControl == parent);
    CHECK(siblingHover.leaves.calls == 1);
    CHECK(parentHover.enters.calls == 1);
}

TEST_CASE("Mouse capture: a release over the holder keeps it hovered with no leave or enter")
{
    Scene scene;
    controls::internal::Dragger *dragger=scene.place<controls::internal::Dragger>(scene.canvas, 10, 10, 20, 20);
    scene.frame();

    scene.moveTo(20, 20);
    HoverCount hover(dragger);
    scene.press();
    scene.moveTo(200, 200);
    scene.moveTo(25, 25);
    scene.release();

    CHECK(HoveredControl == dragger);
    CHECK(hover.enters.calls == 0);
    CHECK(hover.leaves.calls == 0);
}

TEST_CASE("Mouse capture: taken while another control is hovered, that one gets one leave")
{
    Scene scene;
    controls::Base *hovered=scene.place<controls::Base>(scene.canvas, 0, 0, 100, 100);
    controls::Base *holder=scene.place<controls::Base>(scene.canvas, 200, 0, 100, 100);
    scene.frame();

    scene.moveTo(50, 50);
    REQUIRE(HoveredControl == hovered);
    HoverCount hoveredHover(hovered), holderHover(holder);

    MouseFocus=holder;
    scene.moveTo(60, 50);
    CHECK(HoveredControl == holder);
    CHECK(hoveredHover.leaves.calls == 1);

    scene.moveTo(250, 50);
    scene.moveTo(40, 40);
    CHECK(HoveredControl == holder);
    CHECK(hoveredHover.leaves.calls == 1);
    CHECK(hoveredHover.enters.calls == 0);
    CHECK(holderHover.enters.calls == 0);
    CHECK(holderHover.leaves.calls == 0);
}

TEST_CASE("Mouse capture: a Button released off itself is not clicked, released on itself it is, once")
{
    Scene scene;
    controls::Button *button=scene.place<controls::Button>(scene.canvas, 10, 10, 80, 20);
    controls::Button *toggle=scene.place<controls::Button>(scene.canvas, 10, 50, 80, 20);
    toggle->setIsToggle(true);
    scene.frame();

    Counter presses, rightPresses, toggles;
    presses.listen(button->onPressCaller);
    rightPresses.listen(button->onRightPressCaller);
    toggles.listen(toggle->onToggleCaller);

    // pressed, dragged off, released: cancelled, and hover goes to the canvas
    scene.moveTo(50, 20);
    scene.press();
    scene.moveTo(200, 200);
    CHECK(button->isHovered());
    scene.release();
    CHECK(presses.calls == 0);
    CHECK_FALSE(button->isDepressed());
    CHECK_FALSE(button->isHovered());
    CHECK(MouseFocus == nullptr);

    // pressed and released on it: one click
    scene.moveTo(50, 20);
    scene.press();
    scene.release();
    CHECK(presses.calls == 1);
    CHECK(button->isHovered());

    // off and back on before the release still clicks
    scene.press();
    scene.moveTo(200, 200);
    scene.moveTo(20, 15);
    scene.release();
    CHECK(presses.calls == 2);

    // the right button the same
    scene.press(1);
    scene.moveTo(200, 200);
    scene.release(1);
    CHECK(rightPresses.calls == 0);
    scene.moveTo(50, 20);
    scene.press(1);
    scene.release(1);
    CHECK(rightPresses.calls == 1);

    // a toggle released off itself does not toggle
    scene.moveTo(50, 60);
    scene.press();
    scene.moveTo(50, 100);
    scene.release();
    CHECK(toggles.calls == 0);
    CHECK_FALSE(toggle->getToggleState());

    scene.moveTo(50, 60);
    scene.press();
    scene.release();
    CHECK(toggles.calls == 1);
    CHECK(toggle->getToggleState());
}

TEST_CASE("Mouse capture: a Button in a moved parent tests the release in its own bounds")
{
    Scene scene;
    controls::Base *parent=scene.place<controls::Base>(scene.canvas, 100, 80, 200, 100);
    controls::Button *button=scene.place<controls::Button>(parent, 20, 30, 60, 20);
    scene.frame();

    Counter presses;
    presses.listen(button->onPressCaller);

    // the button's canvas rect is 120..179 x 110..129
    scene.moveTo(121, 111);
    scene.press();
    scene.moveTo(179, 129);
    scene.release();
    CHECK(presses.calls == 1);

    scene.moveTo(150, 120);
    scene.press();
    scene.moveTo(180, 120);
    scene.release();
    CHECK(presses.calls == 1);

    scene.moveTo(150, 120);
    scene.press();
    scene.moveTo(150, 130);
    scene.release();
    CHECK(presses.calls == 1);
}

TEST_CASE("Mouse capture: its holder hidden mid-drag, hover goes to the control under the mouse")
{
    Scene scene;
    controls::Base *under=scene.place<controls::Base>(scene.canvas, 100, 0, 100, 100);
    controls::internal::Dragger *dragger=scene.place<controls::internal::Dragger>(scene.canvas, 10, 10, 20, 20);
    scene.frame();

    scene.moveTo(20, 20);
    scene.press();
    scene.moveTo(150, 50);
    REQUIRE(HoveredControl == dragger);

    HoverCount underHover(under), draggerHover(dragger);
    dragger->hide();
    scene.frame();
    CHECK(MouseFocus == nullptr);
    CHECK(HoveredControl == under);
    CHECK(underHover.enters.calls == 1);
    CHECK(draggerHover.leaves.calls == 1);
}

TEST_CASE("Mouse capture: a control with a tooltip the drag crosses does not become the tooltip's owner")
{
    Scene scene;
    controls::internal::Dragger *dragger=scene.place<controls::internal::Dragger>(scene.canvas, 10, 10, 20, 20);
    controls::Base *tipped=scene.place<controls::Base>(scene.canvas, 100, 10, 100, 50);
    tipped->setToolTipText("Tip");
    scene.frame();

    scene.moveTo(20, 20);
    scene.press();
    scene.moveTo(150, 30);
    scene.clock+=scene.canvas->getToolTipDelay();
    scene.frame();
    scene.moveTo(160, 30);
    CHECK(HoveredControl == dragger);
    CHECK(scene.canvas->toolTipOwner() != tipped);
    CHECK_FALSE(scene.canvas->toolTipShown());

    // released over it, it is hovered and its tooltip may follow
    scene.release();
    CHECK(HoveredControl == tipped);
}
