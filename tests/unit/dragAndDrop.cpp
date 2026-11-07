/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Drag and drop on a real Canvas, pressed, moved and released through the
 *  input handler: a TabButton of a reorderable TabControl dragged past its
 *  neighbour lands after it, shown again, not depressed and still selecting
 *  its own page; a docked tab's title bar dragged onto another DockBase moves
 *  its tabs into that dock's tab control, and the dock it left, empty, is
 *  hidden. Neither drag draws anything of its own: there is no drag preview.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/dockBase.h>
#include <gweni/controls/dockedTabControl.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/tabTitleBar.h>
#include <gweni/dragAndDrop.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <functional>
#include <vector>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;

    Scene()
    {
        // a second apart per press, so two presses are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void move(Point to)
    {
        canvas->inputMouseMoved(to.x, to.y, 0, 0);
        frame();
    }

    //! Presses at \p from, moves 6 px right (past the 5 px a drag needs to
    //! start), then to where \p to says once the drag has started, through the
    //! point half-way, and releases there, a frame after each.
    void drag(Point from, const std::function<Point()> &to)
    {
        clock+=1.0f;
        move(from);
        input::onMouseClicked(canvas, 0, true);
        frame();
        move(Point(from.x+6, from.y));

        const Point target=to();

        move(Point((from.x+target.x)/2, (from.y+target.y)/2));
        move(target);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    void click(Point at)
    {
        clock+=1.0f;
        move(at);
        input::onMouseClicked(canvas, 0, true);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }
};

//! \p control's centre, or a point \p fx, \p fy of the way across it, on the canvas.
Point at(controls::Base *control, float fx=0.5f, float fy=0.5f)
{
    return control->localPosToCanvas(Point(static_cast<int>(control->getWidth()*fx),
        static_cast<int>(control->getHeight()*fy)));
}

std::vector<controls::TabButton *> buttons(controls::TabControl *control)
{
    std::vector<controls::TabButton *> result;

    for(auto &&child : control->getTabStrip()->getChildren())
    {
        if(controls::TabButton *button=gweni_cast<controls::TabButton>(child))
            result.push_back(button);
    }
    return result;
}

controls::TabTitleBar *titleBar(controls::TabControl *control)
{
    for(auto &&child : control->getChildren())
    {
        if(controls::TabTitleBar *bar=gweni_cast<controls::TabTitleBar>(child))
            return bar;
    }
    return nullptr;
}

}//namespace

TEST_CASE("dragAndDrop: a TabButton dragged past its neighbour lands after it")
{
    Scene scene;
    controls::TabControl *tabs=scene.canvas->newChild<controls::TabControl>();
    tabs->setDock(DockPosition::Center);
    tabs->setAllowReorder(true);

    controls::TabButton *one=tabs->addPage("One");
    controls::TabButton *two=tabs->addPage("Two");
    controls::TabButton *three=tabs->addPage("Three");
    controls::Base *pageOne=one->getPage();
    scene.frame();

    REQUIRE(buttons(tabs) == std::vector<controls::TabButton *>{one, two, three});
    REQUIRE(one->getWidth() > 0);

    // dropped on the right half of "Two"
    // The dragged tab is hidden as the drag starts, so the strip closes up
    // around it: the drop point is taken from the strip as it is then.
    bool hiddenWhileDragged=false;
    scene.drag(at(one), [&]() {
        hiddenWhileDragged=one->hidden();
        return at(two, 0.8f);
    });

    CHECK(hiddenWhileDragged);

    CHECK(draganddrop::CurrentPackage == nullptr);
    CHECK(draganddrop::SourceControl == nullptr);
    CHECK(buttons(tabs) == std::vector<controls::TabButton *>{two, one, three});
    CHECK(one->visible());
    CHECK_FALSE(one->isDepressed());
    CHECK(one->getPage() == pageOne);
    CHECK(MouseFocus == nullptr);

    // it still selects its own page
    scene.click(at(two));
    CHECK_FALSE(pageOne->visible());
    scene.click(at(one));
    CHECK(tabs->getCurrentButton() == one);
    CHECK(pageOne->visible());
    CHECK(one->isActive());
}

TEST_CASE("dragAndDrop: a TabButton dropped on the strip past the last tab goes to the end")
{
    Scene scene;
    controls::TabControl *tabs=scene.canvas->newChild<controls::TabControl>();
    tabs->setDock(DockPosition::Center);
    tabs->setAllowReorder(true);

    controls::TabButton *one=tabs->addPage("One");
    controls::TabButton *two=tabs->addPage("Two");
    scene.frame();

    // into the empty strip past the last tab: brought to the end
    scene.drag(at(one), [&]() {
        return two->localPosToCanvas(Point(two->getWidth()+40, two->getHeight()/2));
    });

    CHECK(buttons(tabs) == std::vector<controls::TabButton *>{two, one});
    CHECK(one->visible());
    CHECK_FALSE(one->isDepressed());
}

TEST_CASE("dragAndDrop: a docked tab's title bar dropped on another dock moves its tabs there")
{
    Scene scene;
    controls::DockBase *root=scene.canvas->newChild<controls::DockBase>();
    root->setDock(DockPosition::Center);

    controls::DockBase *left=root->getLeft();
    controls::DockBase *right=root->getRight();
    left->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
    left->setWidth(150);
    right->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
    right->setWidth(150);

    controls::TabControl *leftTabs=left->getTabControl();
    controls::TabControl *rightTabs=right->getTabControl();
    controls::TabButton *a=leftTabs->addPage("A");
    controls::TabButton *b=rightTabs->addPage("B");
    scene.frame();

    controls::TabTitleBar *bar=titleBar(leftTabs);
    REQUIRE(bar != nullptr);
    REQUIRE(bar->visible());
    REQUIRE(bar->getHeight() > 0);
    REQUIRE(right->getWidth() > 0);

    bool dockHiddenWhileDragged=false;
    scene.drag(at(bar), [&]() {
        dockHiddenWhileDragged=leftTabs->hidden() && left->hidden();
        return at(right);
    });

    CHECK(dockHiddenWhileDragged);

    CHECK(draganddrop::CurrentPackage == nullptr);
    CHECK(draganddrop::SourceControl == nullptr);
    CHECK(rightTabs->tabCount() == 2);
    CHECK(buttons(rightTabs) == std::vector<controls::TabButton *>{b, a});
    CHECK(a->getTabControl() == rightTabs);
    CHECK(leftTabs->tabCount() == 0);
    // the source tab control is shown again; its dock, left empty, is hidden
    CHECK_FALSE(leftTabs->hidden());
    CHECK(left->hidden());
    CHECK(right->visible());
    CHECK(MouseFocus == nullptr);
}

TEST_CASE("dragAndDrop: a docked tab's title bar dropped nowhere puts its dock back")
{
    Scene scene;
    controls::DockBase *root=scene.canvas->newChild<controls::DockBase>();
    root->setDock(DockPosition::Center);

    controls::DockBase *left=root->getLeft();
    left->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
    left->setWidth(150);

    controls::TabControl *leftTabs=left->getTabControl();
    controls::TabButton *a=leftTabs->addPage("A");
    scene.frame();

    controls::TabTitleBar *bar=titleBar(leftTabs);
    REQUIRE(bar != nullptr);

    // back onto its own dock, which is hidden while the drag lasts, so the
    // drop lands on the root's centre with no tab control there: refused
    bool dockHiddenWhileDragged=false;
    scene.drag(at(bar), [&]() {
        dockHiddenWhileDragged=leftTabs->hidden() && left->hidden();
        return at(root);
    });

    CHECK(dockHiddenWhileDragged);

    CHECK(draganddrop::CurrentPackage == nullptr);
    CHECK(leftTabs->tabCount() == 1);
    CHECK(a->getTabControl() == leftTabs);
    CHECK(left->visible());
    CHECK(leftTabs->visible());
}
