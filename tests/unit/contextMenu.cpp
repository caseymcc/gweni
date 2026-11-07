/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Base::setContextMenu on a real Canvas, clicked through input::onMouseClicked
 *  and keyed through input::onKeyEvent: a right press opens the menu with its
 *  top-left at the cursor, flipped to the cursor's left and above it at the
 *  right and bottom edges and kept inside the canvas (Menu::placement and
 *  keepInside, also checked on their own); the nearest ancestor with a menu
 *  wins; a click outside, Escape, an item chosen, the owner hidden or deleted
 *  close it; a right press on another owner moves to its menu; onContextMenu
 *  fires once per open; a disabled or hidden owner opens nothing; a menu the
 *  control created goes with it, one set from outside stays, and deleting a
 *  shared menu clears it from every control.
 *
 *  Menu::placement and keepInside on their own also put the menu against the
 *  left/top edge where it fits on neither side. An open menu is placed again
 *  when the canvas shrinks; when the nearest ancestor's menu wins, the other
 *  one closes; a click outside may be with either button; onContextMenu fires
 *  before the menu is laid out; a hidden owner closes its open menu. A created
 *  menu goes with its owner on a delayed or outright delete, on
 *  setContextMenu(nullptr) and when the canvas is deleted with it open; a menu
 *  set from outside stays when an owner is deleted, and can be shared by two
 *  owners. A menu elsewhere in the tree is re-parented to the canvas, and
 *  Menu::open opens at the mouse, a later setPos winning.
 */
#include "headlessFixture.h"

#include <gweni/controls/menu.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>

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
        canvas->setSize(300, 200);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

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

    //! A plain control at a fixed place on the canvas (or in \p parent).
    controls::Base *box(int x, int y, int w, int h, controls::Base *parent=nullptr)
    {
        controls::Base *control=(parent ? parent : canvas)->newChild<controls::Base>();

        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(x, y, w, h);
        return control;
    }

    //! A context menu of three items made by \p owner.
    static controls::Menu *menuFor(controls::Base *owner)
    {
        controls::Menu *menu=owner->createContextMenu();

        menu->addItem("One");
        menu->addItem("Two");
        menu->addItem("Three");
        return menu;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void click(int x, int y, int button)
    {
        clock+=1.0f;
        canvas->inputMouseMoved(x, y, 0, 0);
        input::onMouseClicked(canvas, button, true);
        input::onMouseClicked(canvas, button, false);
        frame();
    }

    void rightClick(int x, int y) { click(x, y, 1); }
    void leftClick(int x, int y) { click(x, y, 0); }

    void escape()
    {
        input::onKeyEvent(canvas, keys::Escape, true);
        input::onKeyEvent(canvas, keys::Escape, false);
        frame();
    }

    bool onCanvas(controls::Base *control) const
    {
        const auto &children=canvas->getActualChildren();
        return std::find(children.begin(), children.end(), control) != children.end();
    }
};

//! Counts a Caller's calls and keeps the control of the last one.
class Counter: public event::Handler
{
public:
    void count(event::Info info)
    {
        ++calls;
        last=info.control;
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
    controls::Base *last=nullptr;
};

//! Adds an item to the menu of the control it is told about, each time.
class Filler: public event::Handler
{
public:
    void fill(event::Info info)
    {
        info.control->getContextMenu()->addItem("Added");
        ++calls;
    }

    int calls=0;
};

bool same(const Point &a, const Point &b)
{
    return a.x == b.x && a.y == b.y;
}

}//namespace

TEST_CASE("Menu::placement: at the cursor, flipped at the right and bottom edges, kept inside")
{
    const Rect area(0, 0, 300, 200);
    const Point size(80, 50);

    // room right and below: at the cursor
    CHECK(same(controls::Menu::placement(Point(10, 20), size, area), Point(10, 20)));
    // just fits
    CHECK(same(controls::Menu::placement(Point(220, 150), size, area), Point(220, 150)));
    // past the right edge: on the cursor's left
    CHECK(same(controls::Menu::placement(Point(250, 20), size, area), Point(170, 20)));
    // past the bottom: above the cursor
    CHECK(same(controls::Menu::placement(Point(10, 180), size, area), Point(10, 130)));
    // both: its bottom-right corner at the cursor
    CHECK(same(controls::Menu::placement(Point(290, 190), size, area), Point(210, 140)));
    // no room on either side: flipped, then against the left / top edge
    CHECK(same(controls::Menu::placement(Point(50, 30), Point(280, 190), area), Point(0, 0)));
    CHECK(same(controls::Menu::placement(Point(250, 20), Point(280, 150), area), Point(0, 20)));
    // wider and higher than the area: at its top-left corner
    CHECK(same(controls::Menu::placement(Point(150, 100), Point(400, 300), area), Point(0, 0)));
    // an area that does not start at the origin
    CHECK(same(controls::Menu::placement(Point(105, 105), size, Rect(100, 100, 100, 100)), Point(105, 105)));
    CHECK(same(controls::Menu::placement(Point(195, 195), size, Rect(100, 100, 100, 100)), Point(115, 145)));
}

TEST_CASE("Menu::keepInside: moved the least that puts it inside, never flipped")
{
    const Rect area(0, 0, 300, 200);
    const Point size(80, 50);

    CHECK(same(controls::Menu::keepInside(Point(10, 20), size, area), Point(10, 20)));
    CHECK(same(controls::Menu::keepInside(Point(250, 190), size, area), Point(220, 150)));
    CHECK(same(controls::Menu::keepInside(Point(-5, -7), size, area), Point(0, 0)));
    CHECK(same(controls::Menu::keepInside(Point(50, 50), Point(400, 300), area), Point(0, 0)));
}

TEST_CASE("Context menu: a right-click opens it with its top-left at the cursor")
{
    Scene scene;
    controls::Base *owner=scene.box(10, 10, 100, 60);
    controls::Menu *menu=Scene::menuFor(owner);

    CHECK(owner->ownsContextMenu());
    CHECK(owner->getContextMenu() == menu);
    CHECK(scene.onCanvas(menu));
    CHECK(menu->hidden());
    scene.frame();

    scene.rightClick(30, 40);
    CHECK(scene.canvas->shownContextMenu() == menu);
    CHECK(scene.canvas->contextMenuOwner() == owner);
    CHECK(menu->visible());
    CHECK(menu->getX() == 30);
    CHECK(menu->getY() == 40);
    CHECK(menu->getWidth() > 0);
    CHECK(menu->getHeight() > 0);

    // a left click opens nothing
    scene.leftClick(250, 150);
    CHECK(menu->hidden());
    scene.leftClick(30, 40);
    CHECK(menu->hidden());
    CHECK(!scene.canvas->shownContextMenu());
}

TEST_CASE("Context menu: flipped and kept inside at the canvas's right and bottom edges")
{
    Scene scene;
    controls::Base *owner=scene.box(150, 100, 150, 100);
    controls::Menu *menu=Scene::menuFor(owner);

    scene.frame();
    scene.rightClick(295, 195);
    REQUIRE(menu->visible());

    const int w=menu->getWidth();
    const int h=menu->getHeight();

    REQUIRE(w < 295);
    REQUIRE(h < 195);
    CHECK(menu->getX() == 295-w);
    CHECK(menu->getY() == 195-h);

    // only the right edge: flipped left, at the cursor's height
    scene.leftClick(5, 5);
    scene.rightClick(295, 105);
    CHECK(menu->getX() == 295-w);
    CHECK(menu->getY() == std::min(105, 200-h));
    CHECK(menu->getX() >= 0);
    CHECK(menu->getY()+h <= 200);

    // the canvas shrinks under an open menu: it is placed again, inside
    scene.canvas->setSize(200, 150);
    scene.frame();
    CHECK(menu->getX() >= 0);
    CHECK(menu->getY() >= 0);
    CHECK(menu->getX()+w <= 200);
    CHECK(menu->getY()+menu->getHeight() <= 150);
}

TEST_CASE("Context menu: the nearest ancestor with one wins")
{
    Scene scene;
    controls::Base *outer=scene.box(0, 0, 300, 200);
    controls::Base *plain=scene.box(10, 10, 100, 100, outer);
    controls::Base *inner=scene.box(150, 10, 100, 100, outer);
    controls::Base *innerChild=scene.box(10, 10, 50, 50, inner);
    controls::Menu *outerMenu=Scene::menuFor(outer);
    controls::Menu *innerMenu=Scene::menuFor(inner);

    scene.frame();

    // a child with none: its parent's
    scene.rightClick(20, 20);
    CHECK(scene.canvas->contextMenuOwnerFor(plain) == outer);
    CHECK(scene.canvas->shownContextMenu() == outerMenu);
    CHECK(scene.canvas->contextMenuOwner() == outer);

    // a child of a control with its own: that one, and the other closes
    scene.rightClick(170, 30);
    CHECK(scene.canvas->contextMenuOwnerFor(innerChild) == inner);
    CHECK(scene.canvas->shownContextMenu() == innerMenu);
    CHECK(outerMenu->hidden());
    CHECK(innerMenu->visible());
    CHECK(innerMenu->getX() == 170);

    // a part of an open menu opens nothing
    CHECK(scene.canvas->contextMenuOwnerFor(innerMenu) == nullptr);
    scene.leftClick(5, 190);
    CHECK(!scene.canvas->shownContextMenu());
}

TEST_CASE("Context menu: a click outside, Escape or a chosen item closes it")
{
    Scene scene;
    controls::Base *owner=scene.box(10, 10, 100, 60);
    controls::Menu *menu=Scene::menuFor(owner);

    scene.frame();

    scene.rightClick(20, 20);
    REQUIRE(menu->visible());
    scene.leftClick(250, 150);
    CHECK(menu->hidden());
    CHECK(!scene.canvas->shownContextMenu());
    CHECK(!scene.canvas->contextMenuOwner());

    // a right-click outside every owner closes it too, and opens nothing
    scene.rightClick(20, 20);
    REQUIRE(menu->visible());
    scene.rightClick(250, 150);
    CHECK(menu->hidden());
    CHECK(!scene.canvas->shownContextMenu());

    scene.rightClick(20, 20);
    REQUIRE(menu->visible());
    scene.escape();
    CHECK(menu->hidden());
    CHECK(!scene.canvas->shownContextMenu());

    // Escape with nothing open is the focused control's as before
    scene.escape();
    CHECK(menu->hidden());

    // an item chosen
    scene.rightClick(20, 20);
    REQUIRE(menu->visible());
    controls::Base *item=menu->getChildren().front();
    const Point at=item->localPosToCanvas();
    scene.leftClick(at.x+item->getWidth()/2, at.y+item->getHeight()/2);
    CHECK(menu->hidden());
    CHECK(!scene.canvas->shownContextMenu());
}

TEST_CASE("Context menu: onContextMenu fires once per open, before it opens")
{
    Scene scene;
    controls::Base *owner=scene.box(10, 10, 100, 60);
    controls::Menu *menu=Scene::menuFor(owner);
    Counter opens;
    Filler filler;

    opens.listen(owner->onContextMenu);
    owner->onContextMenu.add(&filler, &Filler::fill);
    scene.frame();

    const size_t items=menu->getChildren().size();

    scene.rightClick(20, 20);
    CHECK(opens.calls == 1);
    CHECK(opens.last == owner);
    // filled in before it was laid out and shown
    CHECK(menu->getChildren().size() == items+1);
    CHECK(menu->visible());

    // the menu stays open across frames and hovers: no second call
    scene.canvas->inputMouseMoved(40, 40, 0, 0);
    scene.frame();
    CHECK(opens.calls == 1);

    scene.leftClick(250, 150);
    CHECK(opens.calls == 1);
    scene.rightClick(25, 25);
    CHECK(opens.calls == 2);
    CHECK(filler.calls == 2);
}

TEST_CASE("Context menu: a disabled or hidden control opens nothing; hiding its owner closes it")
{
    Scene scene;
    controls::Base *parent=scene.box(10, 10, 200, 150);
    controls::Base *owner=scene.box(10, 10, 100, 60, parent);
    controls::Menu *menu=Scene::menuFor(owner);
    Counter opens;

    opens.listen(owner->onContextMenu);
    scene.frame();

    owner->setDisabled(true);
    scene.rightClick(30, 30);
    CHECK(menu->hidden());
    CHECK(opens.calls == 0);
    CHECK(scene.canvas->contextMenuOwnerFor(owner) == nullptr);
    owner->setDisabled(false);

    scene.rightClick(30, 30);
    REQUIRE(menu->visible());

    // the owner's parent hidden: the next update phase closes it
    parent->setHidden(true);
    scene.frame();
    CHECK(menu->hidden());
    CHECK(!scene.canvas->shownContextMenu());
    CHECK(scene.canvas->contextMenuOwnerFor(owner) == nullptr);
    parent->setHidden(false);
    scene.frame();
    CHECK(menu->hidden());
}

TEST_CASE("Context menu: a menu the control created goes with it")
{
    Scene scene;

    SUBCASE("delayed delete, open")
    {
        controls::Base *owner=scene.box(10, 10, 100, 60);
        controls::Menu *menu=Scene::menuFor(owner);

        scene.frame();
        scene.rightClick(20, 20);
        REQUIRE(scene.canvas->shownContextMenu() == menu);

        owner->delayedDelete();
        scene.frame();
        CHECK(!scene.canvas->shownContextMenu());
        CHECK(!scene.canvas->contextMenuOwner());
        CHECK(!scene.onCanvas(menu));
    }

    SUBCASE("outright delete, open")
    {
        controls::Base *owner=scene.box(10, 10, 100, 60);
        controls::Menu *menu=Scene::menuFor(owner);

        scene.frame();
        scene.rightClick(20, 20);
        REQUIRE(scene.canvas->shownContextMenu() == menu);

        delete owner;
        CHECK(!scene.canvas->shownContextMenu());
        CHECK(menu->hidden());
        scene.frame();
        CHECK(!scene.onCanvas(menu));
    }

    SUBCASE("replaced by setContextMenu")
    {
        controls::Base *owner=scene.box(10, 10, 100, 60);
        controls::Menu *menu=Scene::menuFor(owner);

        scene.frame();
        scene.rightClick(20, 20);
        REQUIRE(menu->visible());

        owner->setContextMenu(nullptr);
        CHECK(!scene.canvas->shownContextMenu());
        CHECK(!owner->ownsContextMenu());
        scene.frame();
        CHECK(!scene.onCanvas(menu));

        // and nothing opens now
        scene.rightClick(20, 20);
        CHECK(!scene.canvas->shownContextMenu());
    }

    SUBCASE("the canvas deleted with the menu open")
    {
        controls::Base *owner=scene.box(10, 10, 100, 60);

        Scene::menuFor(owner);
        scene.frame();
        scene.rightClick(20, 20);
        REQUIRE(scene.canvas->shownContextMenu());
        // ~Scene deletes the canvas, owner and menu in one go
    }
}

TEST_CASE("Context menu: a menu set from outside stays, shared by several controls")
{
    Scene scene;
    controls::Base *first=scene.box(10, 10, 100, 60);
    controls::Base *second=scene.box(150, 10, 100, 60);
    controls::Menu *shared=scene.canvas->newChild<controls::Menu>();

    shared->addItem("Shared");
    shared->setHidden(true);
    first->setContextMenu(shared);
    second->setContextMenu(shared);
    CHECK(!first->ownsContextMenu());
    scene.frame();

    scene.rightClick(20, 20);
    CHECK(scene.canvas->contextMenuOwner() == first);
    CHECK(shared->getX() == 20);
    scene.rightClick(160, 20);
    CHECK(scene.canvas->contextMenuOwner() == second);
    CHECK(shared->getX() == 160);

    // deleting an owner closes it but leaves it to its creator
    delete second;
    CHECK(!scene.canvas->shownContextMenu());
    scene.frame();
    CHECK(scene.onCanvas(shared));
    CHECK(first->getContextMenu() == shared);

    // deleting the menu clears it from every control it is set on
    controls::Base *third=scene.box(150, 100, 100, 60);

    third->setContextMenu(shared);
    scene.rightClick(20, 20);
    REQUIRE(scene.canvas->shownContextMenu() == shared);
    shared->delayedDelete();
    scene.frame();
    CHECK(first->getContextMenu() == nullptr);
    CHECK(third->getContextMenu() == nullptr);
    CHECK(!scene.canvas->shownContextMenu());
    scene.rightClick(20, 20);
    CHECK(!scene.canvas->shownContextMenu());
}

TEST_CASE("Context menu: a menu elsewhere in the tree is shown on the canvas")
{
    Scene scene;
    controls::Base *panel=scene.box(50, 50, 200, 100);
    controls::Base *owner=scene.box(10, 10, 100, 60, panel);
    controls::Menu *menu=owner->newChild<controls::Menu>();

    menu->addItem("Child menu");
    menu->setHidden(true);
    owner->setContextMenu(menu);
    scene.frame();

    scene.rightClick(70, 70);
    CHECK((menu->getParent() == scene.canvas));
    CHECK(menu->visible());
    CHECK(menu->getX() == 70);
    CHECK(menu->getY() == 70);
}

TEST_CASE("Context menu: Menu::open places at the mouse, a setPos after it wins")
{
    Scene scene;
    controls::Menu *menu=scene.canvas->newChild<controls::Menu>();

    menu->addItem("One");
    menu->addItem("Two");
    menu->setHidden(true);
    scene.frame();

    scene.canvas->inputMouseMoved(295, 195, 0, 0);
    menu->open(Alignment::Left);
    scene.frame();
    CHECK(menu->getX()+menu->getWidth() == 295);
    CHECK(menu->getY()+menu->getHeight() == 195);

    // a position asked for after opening: kept, inside the canvas
    menu->setPos(40, 30);
    scene.frame();
    CHECK(menu->getX() == 40);
    CHECK(menu->getY() == 30);
    menu->setPos(290, 30);
    scene.frame();
    CHECK(menu->getX() == 300-menu->getWidth());
    CHECK(menu->getY() == 30);
}
