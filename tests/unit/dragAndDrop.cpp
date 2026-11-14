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
 *
 *  The tab is dropped on its neighbour's right half (the point taken once the
 *  strip has closed up around the hidden tab); one dropped past the last tab
 *  goes to the end. A title bar dropped on the root's empty centre is refused
 *  and puts its dock back. Nothing is drawn after the walk.
 *
 *  The core itself: a drag move that leaves the TabStrip's marker where it is
 *  changes no control and lays nothing out (there is no redraw on every move),
 *  and the controls the cursor crosses take no hover while a drag lasts. An
 *  item drag (Package::sourceList) is its list's, not the pressed row's: the
 *  row deleted mid-drag leaves it running, the list hidden (in the next update
 *  phase) or deleted cancels it, telling the target it left. Escape cancels a
 *  live drag ahead of the focused control, and its release goes nowhere; over
 *  nothing that accepts the item the cursor the core asked for is No, and a
 *  release there cancels; a cancel calls no handleDrop and ends the source's
 *  drag with false. A press moved under 5 units starts nothing. Once a drag
 *  ends the hover goes to what is under the mouse.
 *  A wheel during a tab drag goes to the control under the pointer, for
 *  vertical, horizontal and Shift+wheel input, without moving the hover.
 *  A ScrollControl with no Reorderable keeps Base's drag and drop: a package
 *  set with dragAndDrop_setPackage makes it draggable and is the one it hands
 *  out, and a subclass that accepts a package takes its drop as Base does
 *  (the source made its child).
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/dockBase.h>
#include <gweni/controls/dockedTabControl.h>
#include <gweni/controls/scrollControl.h>
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

//! A list of ItemRows whose drags are its own (Package::sourceList): it takes
//! the items of any ItemList that `accepts`, and records what the core told it.
class ItemList: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(ItemList, controls::Base)
    {
        package.name="Item";
        package.draggable=true;
        package.sourceList=this;
    }

    bool dragAndDrop_canAcceptPackage(draganddrop::Package *p) override
    {
        return accepts && (p->name == "Item");
    }
    void dragAndDrop_startDragging(draganddrop::Package *, int, int) override { ++starts; }
    void dragAndDrop_endDragging(bool success, int, int) override { ends.push_back(success); }
    void dragAndDrop_hoverEnter(draganddrop::Package *, int, int) override { ++enters; }
    void dragAndDrop_hoverLeave(draganddrop::Package *) override { ++leaves; }
    void dragAndDrop_hover(draganddrop::Package *, int, int) override { ++hovers; }
    bool dragAndDrop_handleDrop(draganddrop::Package *p, int, int) override
    {
        drops.push_back(p->key);
        dropSources.push_back(p->sourceList);
        return true;
    }

    draganddrop::Package package;
    bool accepts=true;
    int starts=0, enters=0, leaves=0, hovers=0;
    std::vector<bool> ends;
    std::vector<uint64_t> drops;
    std::vector<controls::Base *> dropSources;
};

//! A row of an ItemList, dragged as its list's item `key`.
class ItemRow: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(ItemRow, controls::Base)
    {}

    bool dragAndDrop_draggable() override { return true; }
    draganddrop::Package *dragAndDrop_getPackage(int, int) override
    {
        ItemList *list=gweni_cast<ItemList>(getParent());
        list->package.key=key;
        return &list->package;
    }

    uint64_t key=0;
};

//! Counts the Escape presses it hears while focused.
class EscapeCounter: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(EscapeCounter, controls::Base)
    {
        setKeyboardInputEnabled(true);
    }

    bool onKeyEscape(bool down) override
    {
        if(down)
            ++escapes;
        return true;
    }

    int escapes=0;
};

//! Records wheel input under the pointer, without accepting the tab's drop.
class WheelCounter: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(WheelCounter, controls::Base)
    {}

    bool onMouseWheeled(int delta) override { vertical.push_back(delta); return true; }
    bool onMouseWheeledH(int delta) override { horizontal.push_back(delta); return true; }

    std::vector<int> vertical, horizontal;
};

//! Two ItemLists of three 20-unit rows (keys 1-3 and 11-13), 100 wide, docked
//! left and right on the scene's 400x300 canvas, with nothing between them.
struct Lists
{
    ItemList *left;
    ItemList *right;
    std::vector<ItemRow *> leftRows;
    std::vector<ItemRow *> rightRows;

    explicit Lists(Scene &scene)
    {
        left=scene.canvas->newChild<ItemList>();
        left->setDock(DockPosition::Left);
        left->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
        left->setWidth(100);
        right=scene.canvas->newChild<ItemList>();
        right->setDock(DockPosition::Right);
        right->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
        right->setWidth(100);

        for(uint64_t i=1; i <= 3; ++i)
        {
            leftRows.push_back(row(left, i));
            rightRows.push_back(row(right, 10+i));
        }
        scene.frame();
    }

    static ItemRow *row(ItemList *list, uint64_t key)
    {
        ItemRow *row=list->newChild<ItemRow>();
        row->key=key;
        row->setDock(DockPosition::Top);
        row->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
        row->setHeight(20);
        return row;
    }
};

//! Presses at \p from and moves 6 units right, which starts a drag.
void startDrag(Scene &scene, Point from)
{
    scene.clock+=1.0f;
    scene.move(from);
    input::onMouseClicked(scene.canvas, 0, true);
    scene.frame();
    scene.move(Point(from.x+6, from.y));
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

TEST_CASE("dragAndDrop: a drag move that keeps the marker where it is changes no control")
{
    Scene scene;
    controls::TabControl *tabs=scene.canvas->newChild<controls::TabControl>();
    tabs->setDock(DockPosition::Center);
    tabs->setAllowReorder(true);

    controls::TabButton *one=tabs->addPage("One");
    controls::TabButton *two=tabs->addPage("Two");
    controls::TabButton *three=tabs->addPage("Three");
    scene.frame();

    scene.clock+=1.0f;
    scene.move(at(one));
    input::onMouseClicked(scene.canvas, 0, true);
    scene.frame();
    scene.move(Point(at(one).x+6, at(one).y));
    REQUIRE(draganddrop::CurrentPackage != nullptr);

    // onto the right half of "Two", then a unit further inside it: the marker
    // stays at Two's right edge
    scene.move(at(two, 0.7f));
    scene.move(at(two, 0.8f));

    const Point inside=at(two, 0.8f);
    scene.move(Point(inside.x+1, inside.y));
    // previousFrameStats() is taken as the next frame starts: one more frame,
    // with nothing to do, for the move's
    scene.frame();

    const controls::CanvasFrameStats &stats=scene.canvas->previousFrameStats();
    CHECK(stats.render.controls == 0);
    CHECK(stats.render.visited == 0);
    CHECK(stats.layout.passes == 0);

    // the tabs the cursor crosses take no hover while the drag lasts
    scene.move(at(three, 0.2f));
    CHECK_FALSE(three->isHovered());
    CHECK_FALSE(two->isHovered());

    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();
    CHECK(buttons(tabs) == std::vector<controls::TabButton *>{two, one, three});
}

TEST_CASE("dragAndDrop: wheel input during a tab drag reaches the pointer without moving hover")
{
    Scene scene;
    controls::TabControl *tabs=scene.canvas->newChild<controls::TabControl>();
    tabs->setSize(200, 150);
    tabs->setAllowReorder(true);
    controls::TabButton *one=tabs->addPage("One");
    tabs->addPage("Two");
    WheelCounter *underPointer=scene.canvas->newChild<WheelCounter>();
    underPointer->setPos(250, 50);
    underPointer->setSize(100, 100);
    scene.frame();

    startDrag(scene, at(one));
    REQUIRE(draganddrop::isDragging());
    REQUIRE(draganddrop::CurrentPackage->sourceList == nullptr);
    controls::Base *hover=HoveredControl;
    REQUIRE(hover == one);
    scene.move(at(underPointer));
    REQUIRE(scene.canvas->getControlAt(input::getMousePosition().x,
        input::getMousePosition().y) == underPointer);

    CHECK(scene.canvas->inputMouseWheel(-120));
    CHECK(underPointer->vertical == std::vector<int>{-120});
    CHECK(scene.canvas->inputMouseWheelH(60));
    CHECK(underPointer->horizontal == std::vector<int>{60});
    scene.canvas->inputModifierKey(keys::Shift, true);
    // No REQUIRE between modifier press and release: a failure must not leave
    // Shift held for the next case.
    CHECK(scene.canvas->inputMouseWheel(-60));
    scene.canvas->inputModifierKey(keys::Shift, false);
    CHECK(underPointer->horizontal == std::vector<int>{60, -60});
    CHECK(underPointer->vertical == std::vector<int>{-120});
    CHECK(HoveredControl == hover);
    CHECK_FALSE(underPointer->isHovered());
    CHECK(draganddrop::isDragging());

    draganddrop::cancel();
    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();
    CHECK(underPointer->isHovered());
}

TEST_CASE("dragAndDrop: an item drag is its list's, and drops its key on the list under the cursor")
{
    Scene scene;
    Lists lists(scene);

    startDrag(scene, at(lists.leftRows[1]));
    REQUIRE(draganddrop::isDragging());
    // the list is the source, not the pressed row
    CHECK(draganddrop::SourceControl == lists.left);
    CHECK((draganddrop::dragCanvas() == scene.canvas));
    CHECK(lists.left->starts == 1);
    // the list takes its own items back: it is the target over its own rows
    CHECK(draganddrop::HoveredControl == lists.left);
    CHECK(draganddrop::currentCursor() == cursor::Normal);

    scene.move(at(lists.rightRows[0]));
    CHECK(draganddrop::HoveredControl == lists.right);
    CHECK(lists.left->leaves == 1);
    CHECK(lists.right->enters == 1);

    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();

    CHECK_FALSE(draganddrop::isDragging());
    CHECK(lists.right->drops == std::vector<uint64_t>{2});
    CHECK(lists.right->dropSources == std::vector<controls::Base *>{lists.left});
    CHECK(lists.left->drops.empty());
    CHECK(lists.left->ends == std::vector<bool>{true});
    // the hover is hit-tested again once the drag is over
    CHECK(lists.rightRows[0]->isHovered());
}

TEST_CASE("dragAndDrop: Escape cancels the live drag ahead of the focused control")
{
    Scene scene;
    Lists lists(scene);
    EscapeCounter *focused=scene.canvas->newChild<EscapeCounter>();
    focused->setDock(DockPosition::Bottom);
    focused->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    focused->setHeight(20);
    scene.frame();

    startDrag(scene, at(lists.leftRows[0]));
    scene.move(at(lists.rightRows[2]));
    REQUIRE(draganddrop::HoveredControl == lists.right);
    focused->focus();

    CHECK(input::onKeyEvent(scene.canvas, keys::Escape, true));
    CHECK_FALSE(input::onKeyEvent(scene.canvas, keys::Escape, false));
    scene.frame();

    CHECK_FALSE(draganddrop::isDragging());
    CHECK(focused->escapes == 0);
    CHECK(lists.right->leaves == 1);
    CHECK(lists.right->drops.empty());
    CHECK(lists.left->ends == std::vector<bool>{false});
    CHECK(draganddrop::currentCursor() == cursor::Normal);
    CHECK(lists.rightRows[2]->isHovered());

    // the release that follows is a plain release: nothing is dropped
    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();
    CHECK(lists.right->drops.empty());
    CHECK(lists.left->ends.size() == 1);

    // with no drag, Escape is the focused control's again (the press on a row
    // took its focus)
    focused->focus();
    input::onKeyEvent(scene.canvas, keys::Escape, true);
    input::onKeyEvent(scene.canvas, keys::Escape, false);
    CHECK(focused->escapes == 1);
}

TEST_CASE("dragAndDrop: over nothing that accepts the item the cursor is No, and a release there cancels")
{
    Scene scene;
    Lists lists(scene);
    lists.right->accepts=false;

    startDrag(scene, at(lists.leftRows[0]));
    REQUIRE(draganddrop::isDragging());

    // the empty canvas between the lists, then a list that refuses the item
    scene.move(Point(200, 150));
    CHECK(draganddrop::HoveredControl == nullptr);
    CHECK(draganddrop::currentCursor() == cursor::No);
    scene.move(at(lists.rightRows[1]));
    CHECK(draganddrop::HoveredControl == nullptr);
    CHECK(draganddrop::currentCursor() == cursor::No);
    CHECK(lists.right->enters == 0);

    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();

    CHECK_FALSE(draganddrop::isDragging());
    CHECK(lists.right->drops.empty());
    CHECK(lists.left->drops.empty());
    CHECK(lists.left->ends == std::vector<bool>{false});
    CHECK(draganddrop::currentCursor() == cursor::Normal);
}

TEST_CASE("dragAndDrop: an item drag ends when its list is hidden or deleted, not when its row is deleted")
{
    SUBCASE("the pressed row deleted: the drag is the list's and goes on")
    {
        Scene scene;
        Lists lists(scene);
        startDrag(scene, at(lists.leftRows[0]));
        REQUIRE(draganddrop::isDragging());

        lists.leftRows[0]->delayedDelete();
        scene.frame();
        CHECK(draganddrop::isDragging());

        scene.move(at(lists.rightRows[0]));
        input::onMouseClicked(scene.canvas, 0, false);
        scene.frame();
        CHECK(lists.right->drops == std::vector<uint64_t>{1});
    }

    SUBCASE("the list hidden: cancelled in the next update phase")
    {
        Scene scene;
        Lists lists(scene);
        startDrag(scene, at(lists.leftRows[0]));
        scene.move(at(lists.rightRows[0]));
        REQUIRE(draganddrop::HoveredControl == lists.right);

        lists.left->hide();
        scene.frame();
        CHECK_FALSE(draganddrop::isDragging());
        CHECK(lists.left->ends == std::vector<bool>{false});
        CHECK(lists.right->leaves == 1);

        input::onMouseClicked(scene.canvas, 0, false);
        scene.frame();
        CHECK(lists.right->drops.empty());
    }

    SUBCASE("the list deleted: cancelled at once, the target told")
    {
        Scene scene;
        Lists lists(scene);
        startDrag(scene, at(lists.leftRows[0]));
        scene.move(at(lists.rightRows[0]));
        REQUIRE(draganddrop::HoveredControl == lists.right);

        delete lists.left;
        CHECK_FALSE(draganddrop::isDragging());
        CHECK(draganddrop::SourceControl == nullptr);
        CHECK(lists.right->leaves == 1);
        scene.frame();

        input::onMouseClicked(scene.canvas, 0, false);
        scene.frame();
        CHECK(lists.right->drops.empty());
    }
}

TEST_CASE("dragAndDrop: a press moved less than 5 units starts no drag")
{
    Scene scene;
    Lists lists(scene);
    const Point from=at(lists.leftRows[0]);

    scene.clock+=1.0f;
    scene.move(from);
    input::onMouseClicked(scene.canvas, 0, true);
    scene.frame();
    scene.move(Point(from.x+2, from.y+2));
    CHECK_FALSE(draganddrop::isDragging());
    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();
    CHECK(lists.left->starts == 0);

    // and once released, a move starts nothing either
    scene.move(Point(from.x+20, from.y));
    CHECK_FALSE(draganddrop::isDragging());
}

namespace
{

//! A ScrollControl an app made a drop target of, with no Reorderable
class PanelTarget: public controls::ScrollControl
{
public:
    GWENI_CONTROL_INLINE(PanelTarget, controls::ScrollControl)
    {}

    bool dragAndDrop_canAcceptPackage(draganddrop::Package *package) override
    {
        return package && (package->name == "panel");
    }
};

}//namespace

TEST_CASE("dragAndDrop: a ScrollControl with no Reorderable keeps Base's drag and drop")
{
    Scene scene;
    controls::ScrollControl *source=scene.canvas->newChild<controls::ScrollControl>();
    PanelTarget *target=scene.canvas->newChild<PanelTarget>();

    source->setBounds(10, 10, 100, 100);
    target->setBounds(200, 10, 100, 100);
    scene.frame();

    // nothing set: not draggable, no package, as Base
    CHECK_FALSE(source->dragAndDrop_draggable());
    CHECK(source->dragAndDrop_getPackage(20, 20) == nullptr);

    source->dragAndDrop_setPackage(true, "panel");
    REQUIRE(source->dragAndDrop_draggable());
    draganddrop::Package *package=source->dragAndDrop_getPackage(20, 20);
    REQUIRE(package != nullptr);
    CHECK(package->name == "panel");
    CHECK(package->sourceList == nullptr);

    REQUIRE(target->dragAndDrop_canAcceptPackage(package));
    REQUIRE(draganddrop::start(source, package));
    CHECK(target->dragAndDrop_handleDrop(package, 210, 20));
    CHECK(source->getParent() == target);
    draganddrop::cancel();
    input::onMouseClicked(scene.canvas, 0, false);
}

namespace
{
// A docked tab's title bar in a drag moved past the threshold: the package is
// the bar's, the drag's source its TabControl (TabTitleBar::dragAndDrop_startDragging).
struct TitleBarDrag
{
    Scene scene;
    controls::DockBase *root=nullptr;
    controls::TabControl *tabs=nullptr;
    controls::TabTitleBar *bar=nullptr;

    TitleBarDrag()
    {
        root=scene.canvas->newChild<controls::DockBase>();
        root->setDock(DockPosition::Center);

        controls::DockBase *left=root->getLeft();
        left->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
        left->setWidth(150);

        tabs=left->getTabControl();
        tabs->addPage("A");
        scene.frame();

        bar=titleBar(tabs);
        REQUIRE(bar != nullptr);

        const Point from=at(bar);
        scene.clock+=1.0f;
        scene.move(from);
        input::onMouseClicked(scene.canvas, 0, true);
        scene.frame();
        scene.move(Point(from.x+6, from.y));
        REQUIRE(draganddrop::isDragging());
        REQUIRE(draganddrop::CurrentPackage == bar->dragAndDrop_getPackage(0, 0));
        REQUIRE(draganddrop::SourceControl == tabs);
    }
};
}

// In the ASAN tree these were heap-use-after-free in controlDeleted: the bar's
// package went with the bar, before its TabControl (the source) or the canvas
// was told, and they read it.
TEST_CASE("dragAndDrop: a dock deleted mid title-bar drag ends the drag before the package goes")
{
    TitleBarDrag drag;

    delete drag.root;
    CHECK_FALSE(draganddrop::isDragging());
    CHECK(draganddrop::CurrentPackage == nullptr);
    CHECK(draganddrop::SourceControl == nullptr);
    drag.scene.frame();
    input::onMouseClicked(drag.scene.canvas, 0, false);
    drag.scene.frame();
}

namespace
{
// A handle whose package drags its parent, as a TabTitleBar's drags its
// TabControl: the package is the handle's, the drag's source the parent.
class DragHandle : public controls::Base
{
public:
    GWENI_CONTROL_INLINE(DragHandle, controls::Base)
    {
        setMouseInputEnabled(true);
        dragAndDrop_setPackage(true, "handle");
    }

    void dragAndDrop_startDragging(draganddrop::Package *package, int x, int y) override
    {
        draganddrop::SourceControl=getParent();
        draganddrop::SourceControl->dragAndDrop_startDragging(package, x, y);
    }
};

class EndRecorder : public controls::Base
{
public:
    GWENI_CONTROL_INLINE(EndRecorder, controls::Base)
    {
    }

    void dragAndDrop_endDragging(bool success, int, int) override
    {
        ++ends;
        endSuccess=success;
    }

    int ends=0;
    bool endSuccess=true;
};
}

TEST_CASE("dragAndDrop: a handle deleted mid drag ends the drag its package was")
{
    Scene scene;
    EndRecorder *source=scene.canvas->newChild<EndRecorder>();
    source->setBounds(0, 0, 200, 200);
    DragHandle *handle=source->newChild<DragHandle>();
    handle->setBounds(10, 10, 50, 20);
    scene.frame();

    const Point from(20, 20);
    scene.clock+=1.0f;
    scene.move(from);
    input::onMouseClicked(scene.canvas, 0, true);
    scene.frame();
    scene.move(Point(from.x+6, from.y));
    REQUIRE(draganddrop::isDragging());
    REQUIRE(draganddrop::SourceControl == source);

    delete handle;
    CHECK_FALSE(draganddrop::isDragging());
    CHECK(draganddrop::CurrentPackage == nullptr);
    // the source, still there, hears the cancel
    CHECK(source->ends == 1);
    CHECK_FALSE(source->endSuccess);
    scene.move(Point(150, 150));
    scene.frame();
    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();
    CHECK(source->ends == 1);
}

namespace
{
// Records, as it is deleted, whether a drag was still live.
class DragRecorder : public controls::Base
{
public:
    GWENI_CONTROL_INLINE(DragRecorder, controls::Base)
    {
    }

    ~DragRecorder() override
    {
        if(draggingWhenDeleted)
            *draggingWhenDeleted=draganddrop::isDragging();
    }

    bool *draggingWhenDeleted=nullptr;
};
}

TEST_CASE("dragAndDrop: the canvas deleted mid title-bar drag ends it before its children go")
{
    TitleBarDrag drag;
    // the canvas's first child goes first: the drag is over by then, so the
    // teardown calls nothing of it (no endDragging showing the dock again)
    bool draggingWhenDeleted=true;
    DragRecorder *recorder=drag.scene.canvas->newChild<DragRecorder>();

    recorder->sendToBack();
    recorder->draggingWhenDeleted=&draggingWhenDeleted;
    delete drag.scene.canvas;
    drag.scene.canvas=new controls::Canvas(&drag.scene.skin);
    drag.scene.canvas->setSize(400, 300);
    CHECK_FALSE(draganddrop::isDragging());
    CHECK_FALSE(draggingWhenDeleted);
}
