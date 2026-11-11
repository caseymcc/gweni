/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The input side of a ZoomPanel: a content point p is at the panel's origin
 *  plus pan()+p*zoom() canvas units, and hit-testing, localPosToCanvas() and
 *  canvasPosToLocal() go through that (Base::toChildSpace/fromChildSpace), a
 *  canvas point naming the content unit its own unit starts in (floor), as
 *  the input layer names a device px's canvas unit. So a tree in a panel at
 *  zoom z on a canvas at scale 1 finds, at every device px, the control the
 *  same tree finds on a canvas at setScale(z) (2, 1.5 and 1.37), and the
 *  mouse hovers and presses the same Button there. getCanvasBounds() is a
 *  control's rect as it is drawn, its size zoomed too, and what opens next to
 *  a control in a panel opens at that rect, on the canvas: a ComboBox's list
 *  (below it, as wide), a Popup, the tooltip box; a right press in the panel
 *  reaches the control under it and opens its context menu at the cursor.
 *  A new zoom or pan makes the canvas hit-test the hover again.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/toolTip.h>
#include <gweni/controls/zoomPanel.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace gweni;

namespace
{

bool sameRect(const Rect &a, const Rect &b)
{
    return (a.x == b.x) && (a.y == b.y) && (a.w == b.w) && (a.h == b.h);
}

const int ContentW=200;
const int ContentH=150;

//! The same tree of fixed-size controls, some nested, some touching: built on
//! a canvas, or in a ZoomPanel's content. Returned in build order, the parent
//! (the canvas or the panel's inner panel) first.
std::vector<controls::Base *> buildTree(controls::Base *parent)
{
    std::vector<controls::Base *> made{parent};
    auto box=[&](controls::Base *in, int x, int y, int w, int h) {
        controls::Base *control=in->newChild<controls::Button>();

        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(x, y, w, h);
        made.push_back(control);
        return control;
    };

    box(parent, 3, 4, 37, 13);
    box(parent, 40, 4, 37, 13);
    box(parent, 3, 17, 74, 9);
    controls::Base *frame=parent->newChild<controls::Base>();

    frame->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    frame->setBounds(90, 30, 101, 77);
    frame->setMouseInputEnabled(true);
    made.push_back(frame);
    box(frame, 0, 0, 11, 11);
    box(frame, 7, 13, 23, 31);
    box(frame, 50, 40, 51, 37);
    box(parent, 5, 120, 190, 29);
    return made;
}

struct Scene: public event::Handler
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::ZoomPanel *panel=nullptr;
    float clock=10.0f;
    int presses=0;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
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

    //! A ZoomPanel at (\p x, \p y) of \p w x \p h, its content 200x150.
    void addPanel(int x, int y, int w, int h)
    {
        panel=canvas->newChild<controls::ZoomPanel>();
        panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        panel->setBounds(x, y, w, h);
        panel->setContentSize(Point(ContentW, ContentH));
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void pressed(event::Info) { ++presses; }
};

} // namespace

TEST_CASE("ZoomPanel input: every device px hits what it hits on a canvas at setScale(zoom)")
{
    for(float zoom : {2.0f, 1.5f, 1.37f})
    {
        CAPTURE(zoom);

        Scene scaled;

        scaled.canvas->setScale(zoom);
        scaled.canvas->setSize(ContentW, ContentH);
        const std::vector<controls::Base *> plain=buildTree(scaled.canvas);

        Scene zoomed;
        const int deviceW=static_cast<int>(std::lround(ContentW*zoom));
        const int deviceH=static_cast<int>(std::lround(ContentH*zoom));

        zoomed.canvas->setSize(deviceW, deviceH);
        zoomed.addPanel(0, 0, deviceW, deviceH);
        zoomed.panel->setZoom(zoom);
        const std::vector<controls::Base *> inPanel=buildTree(const_cast<controls::Base *>(zoomed.panel->getInnerPanel()));

        scaled.frame();
        zoomed.frame();
        REQUIRE(plain.size() == inPanel.size());

        int mismatches=0;

        for(int dy=0; dy < deviceH; ++dy)
        {
            for(int dx=0; dx < deviceW; ++dx)
            {
                // the input layer's rule: floor(device px / scale)
                const int x=static_cast<int>(std::floor(double(dx)/zoom));
                const int y=static_cast<int>(std::floor(double(dy)/zoom));
                controls::Base *expected=scaled.canvas->getControlAt(x, y);
                controls::Base *found=zoomed.canvas->getControlAt(dx, dy);
                size_t expectedIndex=plain.size(), foundIndex=inPanel.size();

                for(size_t i=0; i < plain.size(); ++i)
                {
                    if(plain[i] == expected)
                        expectedIndex=i;
                    if(inPanel[i] == found)
                        foundIndex=i;
                }
                if(expectedIndex != foundIndex)
                {
                    if(mismatches < 5)
                        MESSAGE("device px ", dx, ",", dy, ": control ", expectedIndex, " on the scaled canvas, ",
                            foundIndex, " in the panel");
                    ++mismatches;
                }
            }
        }
        CHECK(mismatches == 0);

        // the mouse hovers and presses the same Button through the input layer
        const int dx=static_cast<int>(std::lround(60*zoom));
        const int dy=static_cast<int>(std::lround(10*zoom));

        scaled.canvas->inputMouseMoved(static_cast<int>(std::floor(double(dx)/zoom)),
            static_cast<int>(std::floor(double(dy)/zoom)), 0, 0);
        scaled.frame();
        controls::Base *hoveredPlain=HoveredControl;
        HoveredControl=nullptr;

        static_cast<controls::Button *>(inPanel[2])->onPressCaller.add(&zoomed, &Scene::pressed);
        zoomed.canvas->inputMouseMoved(dx, dy, 0, 0);
        zoomed.frame();
        CHECK((hoveredPlain == plain[2]));
        CHECK((HoveredControl == inPanel[2]));
        input::onMouseClicked(zoomed.canvas, 0, true);
        input::onMouseClicked(zoomed.canvas, 0, false);
        zoomed.frame();
        CHECK(zoomed.presses == 1);
    }
}

TEST_CASE("ZoomPanel input: positions and canvas bounds go through the zoom and the pan")
{
    Scene scene;

    scene.canvas->setSize(400, 300);
    scene.addPanel(20, 10, 300, 250);
    scene.panel->setZoom(1.5f);
    scene.panel->setPan(PointF(10.0f, 5.0f));

    controls::Base *outer=scene.panel->newChild<controls::Base>();

    outer->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    outer->setBounds(30, 20, 100, 60);
    controls::Base *control=outer->newChild<controls::Base>();

    control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    control->setBounds(10, 10, 60, 20);
    scene.frame();

    // content (40, 30): 20+10+40*1.5, 10+5+30*1.5
    const PointF origin=control->localPosToCanvasF(PointF(0.0f, 0.0f));

    CHECK(origin.x == doctest::Approx(90.0f));
    CHECK(origin.y == doctest::Approx(60.0f));
    CHECK(sameRect(control->getCanvasBounds(), Rect(90, 60, 90, 30)));
    // the panel and the controls outside it are not zoomed
    CHECK(sameRect(scene.panel->getCanvasBounds(), Rect(20, 10, 300, 250)));

    // 2.5 content units in: 93.75, canvas unit 94 rounded
    CHECK(control->localPosToCanvas(Point(2, 3)).x == 93);
    CHECK(control->localPosToCanvas(Point(2, 3)).y == 65);
    const PointF back=control->canvasPosToLocalF(PointF(93.0f, 64.5f));

    CHECK(back.x == doctest::Approx(2.0f));
    CHECK(back.y == doctest::Approx(3.0f));
    // the unit a canvas unit starts in: 91 is 0.67 content units in, 89 is -0.67
    CHECK(control->canvasPosToLocal(Point(91, 61)).x == 0);
    CHECK(control->canvasPosToLocal(Point(89, 59)).x == -1);
    CHECK(control->canvasPosToLocal(Point(89, 59)).y == -1);

    // nested in a second panel, the zooms multiply
    controls::ZoomPanel *innerPanel=outer->newChild<controls::ZoomPanel>();

    innerPanel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    innerPanel->setBounds(0, 40, 80, 20);
    innerPanel->setZoom(2.0f);
    controls::Base *deep=innerPanel->newChild<controls::Base>();

    deep->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    deep->setBounds(4, 2, 10, 4);
    scene.frame();
    // outer content (30+0+8, 20+40+4) = (38, 64): 30+38*1.5, 15+64*1.5; 10x4 at 3
    CHECK(sameRect(deep->getCanvasBounds(), Rect(20+10+57, 10+5+96, 30, 12)));
    CHECK((scene.canvas->getControlAt(20+10+57+1, 10+5+96+1) == deep));
}

TEST_CASE("ZoomPanel input: a list, a popup and a tooltip open at the control as it is drawn, on the canvas")
{
    Scene scene;

    scene.canvas->setSize(600, 400);
    scene.addPanel(20, 10, 500, 350);
    scene.panel->setZoom(2.0f);
    scene.panel->setPan(PointF(6.0f, 4.0f));

    controls::ComboBox *combo=scene.panel->newChild<controls::ComboBox>();

    combo->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    combo->setBounds(10, 10, 80, 20);
    combo->addItem("One");
    combo->addItem("Two");

    controls::Button *button=scene.panel->newChild<controls::Button>();

    button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    button->setBounds(10, 50, 60, 20);
    button->setToolTipText("Zoomed");
    scene.frame();

    // the box is at 20+6+20, 10+4+20, 160 x 40
    const Rect comboOnCanvas=combo->getCanvasBounds();

    REQUIRE(sameRect(comboOnCanvas, Rect(46, 34, 160, 40)));
    combo->openList();
    scene.frame();
    controls::Base *list=nullptr;

    for(controls::Base *child : scene.canvas->getChildren())
    {
        if(dynamic_cast<controls::Menu *>(child) && !child->hidden())
            list=child;
    }
    REQUIRE(list);
    CHECK((list->getParent() == scene.canvas));
    CHECK(list->getX() == 46);
    CHECK(list->getY() == 34+40);
    CHECK(list->getWidth() == 160);
    combo->closeList();
    scene.frame();

    // a popup below the button as it is drawn
    controls::Popup *popup=combo->createPopup();

    popup->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    popup->setSize(50, 30);
    popup->open(button, controls::PopupPlacement::Below);
    scene.frame();
    const Rect buttonOnCanvas=button->getCanvasBounds();

    REQUIRE(sameRect(buttonOnCanvas, Rect(46, 114, 120, 40)));
    CHECK((popup->getParent() == scene.canvas));
    const Point popupAt=controls::Popup::placement(buttonOnCanvas, Point(popup->getWidth(), popup->getHeight()),
        Rect(0, 0, 600, 400), controls::PopupPlacement::Below);

    CHECK(popup->getX() == popupAt.x);
    CHECK(popup->getY() == popupAt.y);
    popup->close();
    scene.frame();

    // the tooltip box below the button as it is drawn
    scene.canvas->inputMouseMoved(buttonOnCanvas.x+10, buttonOnCanvas.y+10, 0, 0);
    scene.frame();
    CHECK((HoveredControl == button));
    scene.clock+=scene.canvas->getToolTipDelay();
    scene.frame();
    controls::ToolTip *box=scene.canvas->toolTipControl();

    REQUIRE(box);
    CHECK(!box->hidden());
    CHECK((box->getParent() == scene.canvas));
    const Point boxAt=controls::ToolTip::placement(buttonOnCanvas, Point(box->getWidth(), box->getHeight()),
        Rect(0, 0, 600, 400));

    CHECK(box->getX() == boxAt.x);
    CHECK(box->getY() == boxAt.y);
}

TEST_CASE("ZoomPanel input: a right press in the panel opens the control's context menu at the cursor")
{
    Scene scene;

    scene.canvas->setSize(400, 300);
    scene.addPanel(0, 0, 400, 300);
    scene.panel->setZoom(1.5f);

    controls::Base *owner=scene.panel->newChild<controls::Base>();

    owner->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    owner->setBounds(100, 50, 60, 40);
    owner->setMouseInputEnabled(true);
    controls::Menu *menu=owner->createContextMenu();

    menu->addItem("One");
    menu->addItem("Two");
    scene.frame();

    // content 100..160 is canvas 150..240: 160 is inside, 140 (93.3) is not
    scene.canvas->inputMouseMoved(140, 90, 0, 0);
    input::onMouseClicked(scene.canvas, 1, true);
    input::onMouseClicked(scene.canvas, 1, false);
    scene.frame();
    CHECK((scene.canvas->shownContextMenu() == nullptr));

    scene.clock+=1.0f;
    scene.canvas->inputMouseMoved(160, 90, 0, 0);
    input::onMouseClicked(scene.canvas, 1, true);
    input::onMouseClicked(scene.canvas, 1, false);
    scene.frame();
    REQUIRE((scene.canvas->shownContextMenu() == menu));
    CHECK((menu->getParent() == scene.canvas));
    CHECK(menu->getX() == 160);
    CHECK(menu->getY() == 90);
}

TEST_CASE("ZoomPanel input: a new zoom or pan hit-tests the resting mouse again")
{
    Scene scene;

    scene.canvas->setSize(400, 300);
    scene.addPanel(0, 0, 400, 300);

    controls::Base *left=scene.panel->newChild<controls::Base>();

    left->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    left->setBounds(0, 0, 50, 50);
    left->setMouseInputEnabled(true);
    controls::Base *right=scene.panel->newChild<controls::Base>();

    right->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    right->setBounds(50, 0, 50, 50);
    right->setMouseInputEnabled(true);
    scene.frame();

    scene.canvas->inputMouseMoved(70, 10, 0, 0);
    scene.frame();
    CHECK((HoveredControl == right));

    // at 2, canvas 70 is content 35: the left box, with no mouse move
    scene.panel->setZoom(2.0f);
    scene.frame();
    CHECK((HoveredControl == left));

    // panned 40 right, content 15
    scene.panel->setPan(PointF(40.0f, 0.0f));
    scene.frame();
    CHECK((HoveredControl == left));
    scene.panel->setPan(PointF(-80.0f, 0.0f));
    scene.frame();
    CHECK((HoveredControl == right));
}
