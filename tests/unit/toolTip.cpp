/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The canvas's tooltip: one ToolTip box per canvas, shown by the update phase
 *  once the mouse has rested on a control with a tooltip for the canvas's
 *  delay, placed below the control (ToolTip::placement: above it when there is
 *  no room below, pulled inside the canvas's right and left edges) and kept as
 *  the canvas's last child. Moving straight to another control with a tooltip
 *  fills the same box in at once; leaving, a press, and the control being
 *  hidden, disabled or deleted hide it. A setToolTip() control is shown in the
 *  box and handed back to its owner, hidden, afterwards.
 *
 *  The clock is a driven setTimeSource. ToolTip::placement puts the box at the
 *  bottom when neither below nor above fits; the box is unmoved by a move
 *  inside the control; through a gap the delay starts again; a child shows its
 *  parent's tooltip; a press hides it until the mouse has left, also when the
 *  press comes in the same frame as the move onto the control; the cursor
 *  leaving the window (input::GLFW3::processCursorEnter) hides it, while a
 *  drag keeps its control hovered; clearing the text hides it too, and a
 *  delete may be delayed or outright; a setToolTip() control is deleted with
 *  its owner.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/toolTip.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

bool isAt(const Point &p, int x, int y)
{
    return p.x == x && p.y == y;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    float clock=10.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        // the hovered control and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    controls::Base *control(int x, int y, int w, int h, const String &toolTip)
    {
        controls::Base *control=canvas->newChild<controls::Base>();
        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(x, y, w, h);
        control->setToolTipText(toolTip);
        return control;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Moves the mouse into \p control and renders a frame.
    void hover(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();
        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        frame();
    }

    void moveTo(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        frame();
    }

    //! Lets the delay pass, then renders a frame.
    void waitDelay()
    {
        clock+=canvas->getToolTipDelay();
        frame();
    }

    Rect boxRect() const
    {
        controls::ToolTip *box=canvas->toolTipControl();
        REQUIRE(box);
        return box->getBounds();
    }
};

} // namespace

TEST_CASE("ToolTip::placement: below the control, above it at the bottom, inside the left and right edges")
{
    const Rect area(0, 0, 400, 300);
    const int gap=controls::ToolTip::Gap;

    // below, left edges aligned
    CHECK(isAt(controls::ToolTip::placement(Rect(20, 30, 60, 15), Point(100, 20), area), 20, 30+15+gap));

    // no room below: above
    CHECK(isAt(controls::ToolTip::placement(Rect(20, 270, 60, 15), Point(100, 20), area), 20, 270-gap-20));

    // past the right edge: pulled in, still below
    CHECK(isAt(controls::ToolTip::placement(Rect(350, 30, 50, 15), Point(100, 20), area), 300, 30+15+gap));

    // a control partly past the left edge: the box starts at the edge
    CHECK(isAt(controls::ToolTip::placement(Rect(-30, 30, 50, 15), Point(100, 20), area), 0, 30+15+gap));

    // wider than the area: at its left edge
    CHECK(controls::ToolTip::placement(Rect(100, 30, 50, 15), Point(500, 20), area).x == 0);

    // room neither below nor above: at the area's bottom
    CHECK(isAt(controls::ToolTip::placement(Rect(20, 0, 60, 290), Point(100, 20), area), 20, 280));

    // an area not at the origin
    CHECK(isAt(controls::ToolTip::placement(Rect(5, 5, 10, 10), Point(30, 10), Rect(10, 10, 100, 100)), 10, 5+10+gap));
}

TEST_CASE("A tooltip shows once its delay has passed, below the control, as the canvas's last child")
{
    Scene scene;
    controls::Base *a=scene.control(20, 30, 60, 15, "Tooltip A");
    scene.frame();
    CHECK(scene.canvas->toolTipControl() == nullptr);

    scene.hover(a);
    CHECK(scene.canvas->toolTipOwner() == a);
    CHECK_FALSE(scene.canvas->toolTipShown());

    scene.clock+=scene.canvas->getToolTipDelay()*0.8f;
    scene.frame();
    CHECK_FALSE(scene.canvas->toolTipShown());

    scene.clock+=scene.canvas->getToolTipDelay()*0.3f;
    scene.frame();
    REQUIRE(scene.canvas->toolTipShown());

    controls::ToolTip *box=scene.canvas->toolTipControl();
    REQUIRE(box);
    CHECK_FALSE(box->hidden());
    CHECK(box->getText() == "Tooltip A");
    CHECK((box->getParent() == scene.canvas));
    CHECK(scene.canvas->getActualChildren().back() == box);
    CHECK_FALSE(box->getMouseInputEnabled());

    const Rect rect=scene.boxRect();
    CHECK(rect.x == 20);
    CHECK(rect.y == 30+15+controls::ToolTip::Gap);
    CHECK(rect.w > 0);
    CHECK(rect.h > 0);

    // a move inside the control changes nothing
    scene.moveTo(25, 35);
    CHECK(scene.canvas->toolTipShown());
    CHECK((scene.boxRect() == rect));

    // a child added after it is still numbered below it
    controls::Base *late=scene.control(200, 200, 20, 20, "");
    scene.frame();
    CHECK(scene.canvas->getActualChildren().back() == box);
    CHECK(box->getZIndex() > late->getZIndex());
}

TEST_CASE("Next to the canvas's bottom-right corner the tooltip goes above the control, inside the canvas")
{
    Scene scene;
    controls::Base *corner=scene.control(360, 280, 40, 20, "A tooltip with a long text");
    scene.frame();
    scene.hover(corner);
    scene.waitDelay();
    REQUIRE(scene.canvas->toolTipShown());

    const Rect rect=scene.boxRect();
    CHECK(rect.x+rect.w == 400);
    CHECK(rect.y+rect.h == 280-controls::ToolTip::Gap);
    CHECK(rect.x >= 0);
    CHECK(rect.y >= 0);
}

TEST_CASE("One box serves every control: straight from one to the next it moves at once, through a gap it waits again")
{
    Scene scene;
    controls::Base *a=scene.control(20, 30, 60, 15, "Tooltip A");
    controls::Base *b=scene.control(20, 45, 80, 15, "Tooltip B, longer");
    scene.frame();

    scene.hover(a);
    scene.waitDelay();
    REQUIRE(scene.canvas->toolTipShown());
    controls::ToolTip *box=scene.canvas->toolTipControl();
    const Rect forA=scene.boxRect();

    scene.hover(b);
    CHECK(scene.canvas->toolTipShown());
    CHECK(scene.canvas->toolTipOwner() == b);
    CHECK(scene.canvas->toolTipControl() == box);
    CHECK(box->getText() == "Tooltip B, longer");
    const Rect forB=scene.boxRect();
    CHECK(forB.y == 45+15+controls::ToolTip::Gap);
    CHECK(forB.w > forA.w);

    // off both, then back on A: the delay again
    scene.moveTo(300, 200);
    CHECK_FALSE(scene.canvas->toolTipShown());
    CHECK(box->hidden());
    scene.hover(a);
    CHECK_FALSE(scene.canvas->toolTipShown());
    scene.waitDelay();
    CHECK(scene.canvas->toolTipShown());
    CHECK(scene.canvas->toolTipControl() == box);
    CHECK(box->getText() == "Tooltip A");
}

TEST_CASE("A child with no tooltip shows its parent's")
{
    Scene scene;
    controls::Base *parent=scene.control(20, 30, 100, 40, "The parent's");
    controls::Base *child=parent->newChild<controls::Base>();
    child->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    child->setBounds(10, 10, 20, 20);
    scene.frame();

    scene.hover(child);
    scene.waitDelay();
    REQUIRE(scene.canvas->toolTipShown());
    CHECK(scene.canvas->toolTipOwner() == parent);
    CHECK(scene.canvas->toolTipControl()->getText() == "The parent's");
    CHECK(scene.boxRect().y == 30+40+controls::ToolTip::Gap);
}

TEST_CASE("A press hides the tooltip until the mouse has been on another control")
{
    Scene scene;
    controls::Base *a=scene.control(20, 30, 60, 15, "Tooltip A");
    scene.frame();
    scene.hover(a);
    scene.waitDelay();
    REQUIRE(scene.canvas->toolTipShown());

    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    CHECK_FALSE(scene.canvas->toolTipShown());
    scene.canvas->inputMouseButton(0, false);
    scene.waitDelay();
    scene.waitDelay();
    CHECK_FALSE(scene.canvas->toolTipShown());

    scene.moveTo(300, 200);
    scene.hover(a);
    scene.waitDelay();
    CHECK(scene.canvas->toolTipShown());
}

TEST_CASE("A press in the same frame as the move onto the control suppresses its tooltip")
{
    Scene scene;
    controls::Base *a=scene.control(20, 30, 60, 15, "Tooltip A");
    scene.control(20, 45, 60, 15, "Tooltip B");
    scene.moveTo(300, 200);

    // a fast click: move, press and release with no frame between
    scene.canvas->inputMouseMoved(50, 37, 0, 0);
    scene.canvas->inputMouseButton(0, true);
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    scene.waitDelay();
    scene.waitDelay();
    CHECK_FALSE(scene.canvas->toolTipShown());

    // from A's shown tooltip straight onto B, pressed in the same frame
    scene.moveTo(300, 200);
    scene.hover(a);
    scene.waitDelay();
    REQUIRE(scene.canvas->toolTipShown());
    REQUIRE(scene.canvas->toolTipOwner() == a);
    scene.canvas->inputMouseMoved(50, 52, 0, 0);
    scene.canvas->inputMouseButton(0, true);
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK_FALSE(scene.canvas->toolTipShown());
    scene.waitDelay();
    scene.waitDelay();
    CHECK_FALSE(scene.canvas->toolTipShown());

    // back on A, which was not pressed: it shows after the delay
    scene.hover(a);
    scene.waitDelay();
    CHECK(scene.canvas->toolTipShown());
    CHECK(scene.canvas->toolTipOwner() == a);
}

TEST_CASE("The cursor leaving the window hides the tooltip, through the GLFW translator")
{
    Scene scene;
    controls::Base *a=scene.control(20, 30, 60, 15, "Tooltip A");
    scene.frame();

    input::GLFW3 glfw;
    glfw.initialize(scene.canvas);

    glfw.processMouseMove(50, 37);
    scene.frame();
    scene.waitDelay();
    REQUIRE(scene.canvas->toolTipShown());
    REQUIRE(HoveredControl == a);

    glfw.processCursorEnter(false);
    CHECK(HoveredControl == nullptr);
    scene.frame();
    CHECK_FALSE(scene.canvas->toolTipShown());
    CHECK(scene.canvas->toolTipControl()->hidden());

    // coming back in starts the delay again
    glfw.processCursorEnter(true);
    glfw.processMouseMove(50, 37);
    scene.frame();
    CHECK(HoveredControl == a);
    CHECK_FALSE(scene.canvas->toolTipShown());
    scene.waitDelay();
    CHECK(scene.canvas->toolTipShown());
}

TEST_CASE("The cursor leaving the window during a drag keeps the dragged control hovered")
{
    Scene scene;
    controls::Base *a=scene.control(20, 30, 60, 15, "Tooltip A");
    scene.frame();
    scene.hover(a);
    MouseFocus=a;

    scene.canvas->inputMouseLeave();
    CHECK(HoveredControl == a);

    MouseFocus=nullptr;
    scene.canvas->inputMouseLeave();
    CHECK(HoveredControl == nullptr);
}

TEST_CASE("Hiding, disabling, deleting the control or clearing its tooltip hides the tooltip")
{
    Scene scene;
    controls::Base *a=scene.control(20, 30, 60, 15, "Tooltip A");
    scene.frame();

    SUBCASE("hidden")
    {
        scene.hover(a);
        scene.waitDelay();
        REQUIRE(scene.canvas->toolTipShown());
        a->setHidden(true);
        scene.frame();
        CHECK_FALSE(scene.canvas->toolTipShown());
        CHECK(scene.canvas->toolTipControl()->hidden());
    }
    SUBCASE("disabled")
    {
        scene.hover(a);
        scene.waitDelay();
        REQUIRE(scene.canvas->toolTipShown());
        a->setDisabled(true);
        scene.frame();
        CHECK_FALSE(scene.canvas->toolTipShown());
    }
    SUBCASE("deleted")
    {
        scene.hover(a);
        scene.waitDelay();
        REQUIRE(scene.canvas->toolTipShown());
        a->delayedDelete();
        scene.frame();
        CHECK_FALSE(scene.canvas->toolTipShown());
        CHECK(scene.canvas->toolTipOwner() == nullptr);
        CHECK(scene.canvas->toolTipControl()->hidden());
    }
    SUBCASE("deleted outright")
    {
        scene.hover(a);
        scene.waitDelay();
        REQUIRE(scene.canvas->toolTipShown());
        delete a;
        CHECK_FALSE(scene.canvas->toolTipShown());
        CHECK(scene.canvas->toolTipOwner() == nullptr);
        scene.frame();
        CHECK_FALSE(scene.canvas->toolTipShown());
    }
    SUBCASE("text cleared, then set again")
    {
        scene.hover(a);
        scene.waitDelay();
        REQUIRE(scene.canvas->toolTipShown());
        a->setToolTipText("");
        scene.frame();
        CHECK_FALSE(scene.canvas->toolTipShown());

        a->setToolTipText("Back");
        scene.frame();
        scene.waitDelay();
        CHECK(scene.canvas->toolTipShown());
        CHECK(scene.canvas->toolTipControl()->getText() == "Back");
    }
    SUBCASE("text changed while shown")
    {
        scene.hover(a);
        scene.waitDelay();
        REQUIRE(scene.canvas->toolTipShown());
        a->setToolTipText("A longer text than before");
        scene.frame();
        CHECK(scene.canvas->toolTipShown());
        CHECK(scene.canvas->toolTipControl()->getText() == "A longer text than before");
    }
}

TEST_CASE("A setToolTip() control is shown in the box and handed back to its owner, hidden")
{
    Scene scene;
    controls::Base *a=scene.control(20, 30, 60, 15, "");
    controls::Base *custom=a->newChild<controls::Base>();
    custom->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    custom->setSize(50, 30);
    a->setToolTip(custom);
    CHECK(custom->getParent() == a);
    CHECK(custom->hidden());
    scene.frame();

    scene.hover(a);
    scene.waitDelay();
    REQUIRE(scene.canvas->toolTipShown());
    controls::ToolTip *box=scene.canvas->toolTipControl();
    CHECK(box->content() == custom);
    CHECK(custom->getParent() == box);
    CHECK(custom->visible());
    CHECK(scene.boxRect().w > 50);
    CHECK(scene.boxRect().h > 30);

    scene.moveTo(300, 200);
    CHECK_FALSE(scene.canvas->toolTipShown());
    CHECK(box->content() == nullptr);
    CHECK(custom->getParent() == a);
    CHECK(custom->hidden());

    // shown again, then its owner deleted with it in the box: it goes too
    scene.hover(a);
    scene.waitDelay();
    REQUIRE(custom->getParent() == box);
    const size_t boxChildren=box->getActualChildren().size();
    delete a;
    CHECK(box->content() == nullptr);
    CHECK(box->getActualChildren().size() == boxChildren-1);
    scene.frame();
}
