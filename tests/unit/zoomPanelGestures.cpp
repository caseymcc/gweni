/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A ZoomPanel's own input. The wheel zooms by ZoomPanel::WheelStep a notch
 *  about the cursor: the content point under it stays under it, at every
 *  notch, and the zoom stops at the range's ends (the wheel still taken
 *  there); over a ScrollControl that can scroll, the wheel scrolls it and the
 *  zoom stays. A right or middle press, or a left one with Space held, is a
 *  pan once the mouse is PanThreshold units from it: the content follows the
 *  mouse unit for unit (in the panel's units), the control under the press
 *  gets no press and no context menu opens; let go sooner, the press and the
 *  release reach that control as a click (a right click opens its context
 *  menu, a left one presses a Button), and a plain left drag is no pan. A
 *  zoom and a pan frame run no layout pass and call Base::invalidate()
 *  nowhere, a frame after them lays nothing out either, and a panel deleted
 *  while a pan holds the canvas's input gives it back.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/menu.h>
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
