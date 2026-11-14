/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A ZoomPanel's own input. The wheel zooms by ZoomPanel::WheelStep a notch
 *  about the cursor: the content point under it stays under it, at every
 *  notch, and the zoom stops at the range's ends (the wheel still taken
 *  there); over a ScrollControl that can scroll, the wheel scrolls it and the
 *  zoom stays. The horizontal wheel and Shift+wheel are taken by the panel
 *  and zoom nothing, and mid-pan the horizontal wheel zooms nothing either.
 *  A right or middle press, or a left one with Space held, is a pan once
 *  the mouse is PanThreshold units from it: the content follows the
 *  mouse unit for unit (in the panel's units), the control under the press
 *  gets no press and no context menu opens; let go sooner, the press and the
 *  release reach that control as a click (a right click opens its context
 *  menu, a left one presses a Button), and a plain left drag is no pan. A
 *  zoom and a pan frame run no layout pass and call Base::invalidate()
 *  nowhere, a frame after them lays nothing out either, and a panel deleted
 *  while a pan holds the canvas's input gives it back. A wheel notch during
 *  a pan zooms about the cursor and the drag goes on from there, the content
 *  point under the cursor staying under it; a pan's press closes an open
 *  context menu as a press there would; a disabled panel neither zooms nor
 *  pans; the zoom range has a positive floor. Inside a zoomed panel a
 *  Dragger (a ScrollControl's thumb) and a Resizer keep the grabbed point
 *  under the cursor: their motion is in the panel's content units. So does
 *  a Dragger moving a control added to the panel itself (a window's title
 *  bar), whose logical parent is the panel and which is placed in the inner
 *  panel.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/dragger.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/resizableControl.h>
#include <gweni/controls/resizer.h>
#include <gweni/controls/scrollBarBar.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/zoomPanel.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/inputEvent.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <cmath>
#include <set>
#include <vector>

using namespace gweni;

namespace
{

struct Scene: public event::Handler
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::ZoomPanel *panel;
    controls::Button *button;
    controls::Menu *menu;
    float clock=10.0f;
    int presses=0;

    Scene()
    {
        // a second apart per press, so two presses are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        panel=canvas->newChild<controls::ZoomPanel>();
        panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        panel->setBounds(20, 10, 360, 280);
        panel->setContentSize(Point(600, 600));

        button=panel->newChild<controls::Button>();
        button->setText("Button");
        button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        button->setBounds(40, 40, 80, 30);
        button->onPressCaller.add(this, &Scene::pressed);
        menu=button->createContextMenu();
        menu->addItem("One");
        menu->addItem("Two");
        frame();
        frame();
    }

    ~Scene()
    {
        // the hovered control, the focus and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        input::onMouseClicked(canvas, 1, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void pressed(event::Info) { ++presses; }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void move(int x, int y) { canvas->inputMouseMoved(x, y, 0, 0); }
    void press(int button, bool down)
    {
        if(down)
            clock+=1.0f;
        canvas->inputMouseButton(button, down);
    }

    //! The content point drawn at the canvas point (x, y).
    PointF contentAt(int x, int y) const
    {
        return panel->getInnerPanel()->canvasPosToLocalF(PointF(float(x), float(y)));
    }

    //! The canvas point the content point \p p is drawn at.
    PointF canvasOf(const PointF &p) const
    {
        return panel->getInnerPanel()->localPosToCanvasF(p);
    }
};

//! The first shown vertical scroll bar thumb under \p control.
controls::internal::ScrollBarBar *verticalThumb(controls::Base *control)
{
    for(controls::Base *child : control->getActualChildren())
    {
        controls::internal::ScrollBarBar *bar=gweni_cast<controls::internal::ScrollBarBar>(child);

        if(bar && bar->visible() && bar->isVertical())
            return bar;
        if(controls::internal::ScrollBarBar *found=verticalThumb(child))
            return found;
    }
    return nullptr;
}

} // namespace

TEST_CASE("ZoomPanel gestures: the wheel zooms about the cursor, within the range")
{
    Scene scene;

    // over the panel's empty content
    scene.move(250, 200);
    scene.frame();
    const PointF under=scene.contentAt(250, 200);

    for(int notch=0; notch < 5; ++notch)
    {
        const size_t invalidations=controls::invalidateCalls();

        CHECK(scene.canvas->inputMouseWheel(60));
        CHECK(controls::invalidateCalls() == invalidations);
        CHECK(scene.frame().passes == 0);
        CHECK(controls::invalidateCalls() == invalidations);

        const PointF at=scene.canvasOf(under);

        CHECK(at.x == doctest::Approx(250.0f).epsilon(1e-4));
        CHECK(at.y == doctest::Approx(200.0f).epsilon(1e-4));
    }
    CHECK(scene.panel->zoom() == doctest::Approx(std::pow(controls::ZoomPanel::WheelStep, 5.0f)));
    CHECK(scene.frame().passes == 0);

    // to the top of the range and no further, then to the bottom
    for(int notch=0; notch < 40; ++notch)
        scene.canvas->inputMouseWheel(60);
    CHECK(scene.panel->zoom() == 4.0f);
    CHECK(scene.canvas->inputMouseWheel(60));
    CHECK(scene.panel->zoom() == 4.0f);
    for(int notch=0; notch < 80; ++notch)
        scene.canvas->inputMouseWheel(-60);
    CHECK(scene.panel->zoom() == 0.25f);

    // the point is still under the cursor at both ends
    const PointF at=scene.canvasOf(under);

    CHECK(at.x == doctest::Approx(250.0f).epsilon(1e-4));
    CHECK(at.y == doctest::Approx(200.0f).epsilon(1e-4));

    scene.panel->setZoomRange(0.5f, 2.0f);
    CHECK(scene.panel->zoom() == 0.5f);
}

TEST_CASE("ZoomPanel gestures: over a ScrollControl that can scroll, the wheel scrolls it")
{
    Scene scene;
    controls::ScrollControl *scroll=scene.panel->newChild<controls::ScrollControl>();

    scroll->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    scroll->setBounds(150, 40, 120, 100);
    scroll->setScroll(false, true);
    controls::Base *tall=scroll->newChild<controls::Base>();

    tall->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    tall->setBounds(0, 0, 100, 400);
    tall->setMouseInputEnabled(true);
    scene.panel->setZoom(1.5f);
    scene.frame();
    scene.frame();

    // content (170, 90): canvas 20+170*1.5, 10+90*1.5
    scene.move(275, 145);
    scene.frame();
    REQUIRE((HoveredControl == tall));
    CHECK(scene.canvas->inputMouseWheel(-60));
    scene.frame();
    CHECK(scroll->getVerticalScrollOffset() > 0);
    CHECK(scene.panel->zoom() == 1.5f);
}

TEST_CASE("ZoomPanel gestures: right, middle and Space+left drags pan, a plain left drag does not")
{
    for(int button : {1, 2, 0})
    {
        CAPTURE(button);

        Scene scene;
        const bool space=(button == 0);

        // on the Button: content (60, 50) is canvas (80, 60)
        scene.move(80, 60);
        scene.frame();
        REQUIRE((HoveredControl == scene.button));
        if(space)
            scene.canvas->inputModifierKey(keys::Space, true);
        scene.press(button, true);
        CHECK((scene.canvas->inputCapture() != nullptr));

        // under the threshold nothing moves
        scene.move(82, 61);
        scene.frame();
        CHECK(scene.panel->pan().x == 0.0f);
        CHECK(!scene.panel->panning());

        const size_t invalidations=controls::invalidateCalls();

        scene.move(110, 75);
        CHECK(scene.panel->panning());
        CHECK(scene.frame().passes == 0);
        CHECK(controls::invalidateCalls() == invalidations);
        CHECK(scene.panel->pan().x == 30.0f);
        CHECK(scene.panel->pan().y == 15.0f);

        scene.move(70, 50);
        CHECK(scene.frame().passes == 0);
        CHECK(scene.panel->pan().x == -10.0f);
        CHECK(scene.panel->pan().y == -10.0f);

        scene.press(button, false);
        if(space)
            scene.canvas->inputModifierKey(keys::Space, false);
        CHECK((scene.canvas->inputCapture() == nullptr));
        CHECK(scene.frame().passes == 0);
        CHECK(controls::invalidateCalls() == invalidations);
        CHECK(scene.presses == 0);
        CHECK((scene.canvas->shownContextMenu() == nullptr));
        // the hover is where the mouse is: content (60, 50) still, the Button
        CHECK((HoveredControl == scene.button));
    }

    // a left drag without Space is the content's
    Scene scene;

    scene.move(300, 200);
    scene.frame();
    scene.press(0, true);
    CHECK((scene.canvas->inputCapture() == nullptr));
    scene.move(330, 220);
    scene.frame();
    scene.press(0, false);
    CHECK(scene.panel->pan().x == 0.0f);
}

TEST_CASE("ZoomPanel gestures: a press let go under the threshold reaches the control")
{
    Scene scene;

    scene.panel->setZoom(2.0f);
    scene.frame();

    // the Button is content 40..120 x 40..70: canvas 100..260 x 90..150
    scene.move(150, 100);
    scene.frame();
    REQUIRE((HoveredControl == scene.button));

    // a right click with a little motion opens its context menu at the press
    scene.press(1, true);
    scene.move(152, 101);
    scene.press(1, false);
    scene.frame();
    CHECK((scene.canvas->inputCapture() == nullptr));
    REQUIRE((scene.canvas->shownContextMenu() == scene.menu));
    CHECK(scene.menu->getX() == 150);
    CHECK(scene.menu->getY() == 100);
    CHECK(scene.panel->pan().x == 0.0f);

    scene.canvas->inputModifierKey(keys::Escape, true);
    scene.canvas->inputModifierKey(keys::Escape, false);
    scene.frame();
    CHECK((scene.canvas->shownContextMenu() == nullptr));

    // Space and a left click: the Button is pressed
    scene.move(150, 100);
    scene.frame();
    scene.canvas->inputModifierKey(keys::Space, true);
    scene.press(0, true);
    scene.press(0, false);
    scene.canvas->inputModifierKey(keys::Space, false);
    scene.frame();
    CHECK(scene.presses == 1);
    CHECK(scene.panel->pan().x == 0.0f);

    // and a plain one, with nothing held back
    scene.press(0, true);
    scene.press(0, false);
    scene.frame();
    CHECK(scene.presses == 2);
}

TEST_CASE("ZoomPanel gestures: a panel deleted mid-pan gives the canvas its input back")
{
    Scene scene;

    scene.move(300, 200);
    scene.frame();
    scene.press(1, true);
    scene.move(330, 220);
    REQUIRE(scene.panel->panning());
    REQUIRE((scene.canvas->inputCapture() != nullptr));

    delete scene.panel;
    scene.panel=nullptr;
    CHECK((scene.canvas->inputCapture() == nullptr));
    scene.press(1, false);
    scene.frame();
}

TEST_CASE("ZoomPanel gestures: a zoom and a pan mark the panel's subtree for render and nothing else")
{
    Scene scene;
    controls::Button *outside=scene.canvas->newChild<controls::Button>();

    outside->setText("Out");
    outside->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    outside->setBounds(0, 100, 18, 20);
    controls::Base *text=scene.panel->newChild<test::StubText>();

    text->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    text->setBounds(10, 100, 60, 12);
    scene.frame();
    scene.frame();
    scene.move(250, 200);
    scene.frame();
    scene.frame();

    // what the update phase leaves for the render phase to draw
    auto update=[&]() {
        scene.canvas->updateCanvas();
        CHECK(scene.canvas->getLayoutStats().passes == 0);
        CHECK(outside->getStateChange() == controls::StateChange_Nothing);
        CHECK(scene.canvas->getStateChange() == controls::StateChange_Nothing);
        CHECK(scene.panel->getStateChange() == controls::StateChange_Nothing);
    };

    // a zoom: geometry and text under the panel
    scene.canvas->inputMouseWheel(60);
    update();
    CHECK(text->getStateChange() == (controls::StateChange_Geometry|controls::StateChange_Text));
    CHECK((scene.button->getStateChange()&controls::StateChange_Geometry) != 0);
    scene.canvas->renderCanvas();
    CHECK(text->getStateChange() == controls::StateChange_Nothing);

    // a pan: geometry alone
    scene.press(1, true);
    scene.move(270, 230);
    update();
    CHECK(text->getStateChange() == controls::StateChange_Geometry);
    scene.canvas->renderCanvas();
    scene.press(1, false);
    scene.frame();

    // settled: nothing
    scene.canvas->updateCanvas();
    CHECK(text->getStateChange() == controls::StateChange_Nothing);
    CHECK(scene.button->getStateChange() == controls::StateChange_Nothing);
    scene.canvas->renderCanvas();
}

TEST_CASE("ZoomPanel gestures: a wheel notch mid-pan zooms about the cursor and the pan goes on from there")
{
    Scene scene;

    scene.move(250, 200);
    scene.frame();
    scene.press(1, true);
    scene.move(270, 210);
    REQUIRE(scene.panel->panning());
    scene.frame();

    // the content point under the cursor stays under it through the zoom
    // and every move after it
    const PointF under=scene.contentAt(270, 210);

    scene.canvas->inputMouseWheel(60);
    scene.frame();
    CHECK(scene.panel->zoom() == doctest::Approx(controls::ZoomPanel::WheelStep));

    PointF at=scene.canvasOf(under);

    CHECK(at.x == doctest::Approx(270.0f).epsilon(1e-4));
    CHECK(at.y == doctest::Approx(210.0f).epsilon(1e-4));

    scene.move(290, 230);
    scene.frame();
    at=scene.canvasOf(under);
    CHECK(at.x == doctest::Approx(290.0f).epsilon(1e-4));
    CHECK(at.y == doctest::Approx(230.0f).epsilon(1e-4));

    scene.canvas->inputMouseWheel(-120);
    scene.move(250, 190);
    scene.frame();
    at=scene.canvasOf(under);
    CHECK(at.x == doctest::Approx(250.0f).epsilon(1e-4));
    CHECK(at.y == doctest::Approx(190.0f).epsilon(1e-4));

    scene.press(1, false);
    scene.frame();
    CHECK((scene.canvas->inputCapture() == nullptr));
}

TEST_CASE("ZoomPanel gestures: a pan's press closes an open context menu")
{
    Scene scene;

    scene.move(80, 60);
    scene.frame();
    scene.press(1, true);
    scene.press(1, false);
    scene.frame();
    REQUIRE((scene.canvas->shownContextMenu() == scene.menu));

    // a middle drag over the content, off the menu
    scene.move(300, 250);
    scene.frame();
    scene.press(2, true);
    CHECK(scene.menu->hidden());
    scene.move(320, 270);
    CHECK(scene.panel->panning());
    scene.press(2, false);
    scene.frame();
    CHECK((scene.canvas->shownContextMenu() == nullptr));
}

TEST_CASE("ZoomPanel gestures: a disabled panel neither zooms nor pans, and the range stays positive")
{
    Scene scene;

    scene.panel->setDisabled(true);
    scene.move(250, 200);
    scene.frame();
    scene.canvas->inputMouseWheel(60);
    scene.frame();
    CHECK(scene.panel->zoom() == 1.0f);
    scene.press(1, true);
    CHECK((scene.canvas->inputCapture() == nullptr));
    scene.move(290, 230);
    scene.press(1, false);
    scene.frame();
    CHECK(scene.panel->pan().x == 0.0f);

    scene.panel->setDisabled(false);
    scene.panel->setZoomRange(0.0f, 2.0f);
    CHECK(scene.panel->minZoom() == controls::ZoomPanel::MinZoomLimit);
    scene.panel->setZoom(0.0f);
    CHECK(scene.panel->zoom() == controls::ZoomPanel::MinZoomLimit);
    scene.panel->setZoomRange(-1.0f, -2.0f);
    CHECK(scene.panel->zoom() > 0.0f);
    CHECK(scene.panel->maxZoom() >= scene.panel->minZoom());
}

TEST_CASE("ZoomPanel gestures: a scroll bar thumb dragged in a zoomed panel keeps the grabbed point under the cursor")
{
    for(float zoom : {1.0f, 1.25f, 2.0f})
    {
        CAPTURE(zoom);

        Scene scene;
        controls::ScrollControl *scroll=scene.panel->newChild<controls::ScrollControl>();

        scroll->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        // inside the panel at every zoom: canvas 60..300 x 50..250 at 2
        scroll->setBounds(20, 20, 120, 100);
        scroll->setScroll(false, true);
        controls::Base *tall=scroll->newChild<controls::Base>();

        tall->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        tall->setBounds(0, 0, 100, 1200);
        scene.panel->setZoom(zoom);
        scene.frame();
        scene.frame();

        controls::internal::ScrollBarBar *thumb=verticalThumb(scroll);

        REQUIRE(thumb);

        const Rect drawn=thumb->getCanvasBounds();
        const Point press(drawn.x+drawn.w/2, drawn.y+2*int(zoom)+1);

        scene.move(press.x, press.y);
        scene.frame();
        REQUIRE((HoveredControl == thumb));

        const PointF grabbed=thumb->canvasPosToLocalF(PointF(press));

        scene.press(0, true);
        for(int step=1; step <= 4; ++step)
        {
            CAPTURE(step);
            const Point to(press.x, press.y+step*7);

            scene.move(to.x, to.y);
            scene.frame();

            const PointF held=thumb->canvasPosToLocalF(PointF(to));

            // to within the unit the hold was floored to
            CHECK(std::fabs(held.y-grabbed.y) < 1.0f);
            CHECK(scroll->getVerticalScrollOffset() > 0);
        }
        scene.press(0, false);
        scene.frame();
    }
}

TEST_CASE("ZoomPanel gestures: a Resizer dragged in a zoomed panel keeps the grabbed edge under the cursor")
{
    for(float zoom : {1.0f, 1.5f, 2.0f})
    {
        CAPTURE(zoom);

        Scene scene;
        controls::ResizableControl *box=scene.panel->newChild<controls::ResizableControl>();

        box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        // inside the panel at every zoom: canvas 80..280 x 90..250 at 2
        box->setBounds(30, 40, 100, 80);
        scene.panel->setZoom(zoom);
        scene.frame();
        scene.frame();

        // the top edge's resizer (index 3), at its middle
        controls::internal::Resizer *top=box->getResizer(3);

        REQUIRE(top);
        const Rect drawn=top->getCanvasBounds();
        const Point press(drawn.x+drawn.w/2, drawn.y+drawn.h/2);

        scene.move(press.x, press.y);
        scene.frame();
        REQUIRE((HoveredControl == top));

        const PointF grabbed=top->canvasPosToLocalF(PointF(press));

        scene.press(0, true);
        // up 12 canvas units: 12/zoom content units
        const Point to(press.x, press.y-12);

        scene.move(to.x, to.y);
        scene.frame();
        CHECK(std::abs(box->getY()-(40-int(std::lround(12.0f/zoom)))) <= 1);
        CHECK(std::abs(box->getHeight()-(80+int(std::lround(12.0f/zoom)))) <= 1);
        CHECK(box->getY()+box->getHeight() == 120);

        const PointF held=top->canvasPosToLocalF(PointF(to));

        CHECK(std::fabs(held.y-grabbed.y) < 1.0f);
        scene.press(0, false);
        scene.frame();
    }
}

TEST_CASE("ZoomPanel gestures: a Dragger moving a child of the panel itself keeps the grabbed point under the cursor")
{
    for(float zoom : {1.0f, 1.5f, 2.0f})
    {
        CAPTURE(zoom);

        Scene scene;

        scene.panel->setPan(PointF(10.0f, 6.0f));

        // as a WindowControl and its title bar: the box's parent is the
        // panel, the inner panel is where it is placed
        controls::Base *box=scene.panel->newChild<controls::Base>();

        box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        box->setBounds(60, 100, 100, 60);
        controls::internal::Dragger *bar=box->newChild<controls::internal::Dragger>();

        bar->setTarget(box);
        bar->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        bar->setBounds(0, 0, 100, 20);
        scene.panel->setZoom(zoom);
        scene.frame();
        scene.frame();
        REQUIRE((box->getParent() == scene.panel));
        REQUIRE((box->getActualParent() == scene.panel->getInnerPanel()));

        const Rect drawn=bar->getCanvasBounds();
        const Point press(drawn.x+drawn.w/2, drawn.y+drawn.h/2);

        scene.move(press.x, press.y);
        scene.frame();
        REQUIRE((HoveredControl == bar));

        const PointF grabbed=bar->canvasPosToLocalF(PointF(press));

        scene.press(0, true);
        for(int step=1; step <= 3; ++step)
        {
            CAPTURE(step);
            const Point to(press.x+step*14, press.y+step*10);

            scene.move(to.x, to.y);
            scene.frame();

            const PointF held=bar->canvasPosToLocalF(PointF(to));

            // to within the unit the hold was floored to
            CHECK(std::fabs(held.x-grabbed.x) < 1.0f);
            CHECK(std::fabs(held.y-grabbed.y) < 1.0f);
        }
        scene.press(0, false);
        scene.frame();
    }
}

TEST_CASE("ZoomPanel gestures: the horizontal wheel and Shift+wheel are taken and zoom nothing")
{
    Scene scene;

    scene.move(250, 200);
    scene.frame();
    CHECK(scene.canvas->inputMouseWheelH(60));
    CHECK(scene.canvas->inputMouseWheelH(-60));
    CHECK(scene.panel->zoom() == 1.0f);

    scene.canvas->inputModifierKey(keys::Shift, true);
    REQUIRE(input::isShiftDown());
    CHECK(scene.canvas->inputMouseWheel(60));
    scene.canvas->inputModifierKey(keys::Shift, false);
    CHECK(scene.panel->zoom() == 1.0f);
    CHECK(scene.frame().passes == 0);

    // disabled, it takes neither axis
    scene.panel->setDisabled(true);
    CHECK_FALSE(scene.canvas->inputMouseWheelH(60));
}

TEST_CASE("ZoomPanel gestures: a horizontal wheel mid-pan zooms nothing")
{
    Scene scene;

    scene.move(250, 200);
    scene.frame();
    scene.press(1, true);
    scene.move(270, 210);
    REQUIRE(scene.panel->panning());
    scene.frame();

    const PointF under=scene.contentAt(270, 210);

    CHECK(scene.canvas->inputMouseWheelH(60));
    scene.frame();
    CHECK(scene.panel->zoom() == 1.0f);

    // the pan goes on as before
    scene.move(290, 230);
    scene.frame();
    const PointF at=scene.canvasOf(under);

    CHECK(at.x == doctest::Approx(290.0f).epsilon(1e-4));
    CHECK(at.y == doctest::Approx(230.0f).epsilon(1e-4));

    scene.press(1, false);
    scene.frame();
}
