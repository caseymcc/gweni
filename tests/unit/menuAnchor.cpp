/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A MenuItem's menu is placed from its item on every layout pass (#517). A
 *  MenuStrip item's menu opened with openMenu() before any frame is drawn has
 *  its top-left at (the item's canvas x, the item's bottom + 1) once the first
 *  frames have settled, the place an opening after layout gives, at canvas
 *  scales 1, 1.5 and 2; opened first before any frame, then closed and opened
 *  again, it is at that same place every time. An item that moves while its
 *  menu is open (an earlier item's text grown, the canvas resized under a
 *  strip on the right) has its menu at its new place on the next settled
 *  frame, each frame laying out in at most one pass, and a frame where nothing
 *  moved runs no layout pass and writes no slot. A submenu opens to the right
 *  of its item, its top at the item's top, and on the item's left where the
 *  canvas's right edge leaves it no room.
 *
 *  openMenu() anchors the menu to its item (Menu::openNextTo, self-placed on
 *  the canvas at the size the canvas gives a menu it places) until the menu
 *  is hidden, a setPos() after it (which then wins, kept inside the canvas,
 *  the item's moves taking it nowhere) or an openAt() (at the cursor, flipped
 *  at the right edge as a context menu).
 */
#include "headlessFixture.h"

#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/menuStrip.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::MenuStrip *strip=nullptr;
    controls::MenuItem *file=nullptr;
    controls::MenuItem *edit=nullptr;
    //! In File's menu, with a submenu of its own.
    controls::MenuItem *recent=nullptr;

    //! A 400x300 canvas at \p scale with a strip of File and Edit, each with a
    //! menu, File's holding Recent, which has a submenu. With \p rightPanel the
    //! strip is in a 150-unit panel docked on the canvas's right, so its items
    //! move with the canvas's width and sit near its right edge.
    explicit Scene(float scale=1.0f, bool rightPanel=false)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setScale(scale);
        canvas->setSize(400, 300);

        controls::Base *parent=canvas;

        if(rightPanel)
        {
            parent=canvas->newChild<controls::Base>();
            parent->setDock(DockPosition::Right);
            parent->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
            parent->setWidth(150);
        }

        strip=parent->newChild<controls::MenuStrip>();
        file=strip->addItem("File");
        edit=strip->addItem("Edit");

        controls::Menu *fileMenu=file->getMenu();

        fileMenu->addItem("New");
        recent=fileMenu->addItem("Recent");
        recent->getMenu()->addItem("a much longer file name.txt");
        recent->getMenu()->addItem("b.txt");
        edit->getMenu()->addItem("Undo");
        edit->getMenu()->addItem("Redo");
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        delete canvas;
    }

    //! Both phases, so every pending state change is drawn and cleared.
    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Frames until one runs no layout pass (at most 4), each laying out in
    //! at most one pass.
    void settle()
    {
        for(int i=0; i < 4; ++i)
        {
            frame();
            CHECK(canvas->getLayoutStats().passes <= 1);
            CHECK_FALSE(canvas->getLayoutStats().renderLayout);

            if(canvas->getLayoutStats().passes == 0)
                return;
        }
        FAIL("layout did not settle in 4 frames");
    }
};

Rect canvasRect(controls::Base *control)
{
    const Point origin=control->localPosToCanvas(Point(0, 0));
    return Rect(origin.x, origin.y, control->getWidth(), control->getHeight());
}

//! Where a strip item's menu belongs: at the item's left, 1 below it.
Point belowItem(controls::MenuItem *item)
{
    const Rect r=canvasRect(item);
    return Point(r.x, r.y+r.h+1);
}

Point topLeft(controls::Base *control)
{
    return Point(control->getX(), control->getY());
}

//! A control's top-left is \p p, x and y checked on their own so a failure
//! shows both.
#define CHECK_AT(control, p) \
    do { \
        const Point at_=(p); \
        CHECK((control)->getX() == at_.x); \
        CHECK((control)->getY() == at_.y); \
    } while(false)

//! A settled frame with the menu open: no layout pass, no slot written.
void checkSettled(Scene &scene)
{
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
}

}//namespace

TEST_CASE("Menu anchor: a strip menu opened before any frame opens below its item, as one opened after layout")
{
    for(float scale : {1.0f, 1.5f, 2.0f})
    {
        CAPTURE(scale);

        Scene early(scale);
        early.edit->openMenu();
        early.settle();

        Scene late(scale);
        late.settle();
        late.edit->openMenu();
        late.settle();

        controls::Menu *earlyMenu=early.edit->getMenu();
        controls::Menu *lateMenu=late.edit->getMenu();

        REQUIRE(earlyMenu->visible());
        REQUIRE(lateMenu->visible());
        // behind the strip's 5-unit left padding and the File item
        REQUIRE(canvasRect(early.edit).x > 5);
        CHECK_AT(earlyMenu, belowItem(early.edit));
        CHECK_AT(lateMenu, belowItem(late.edit));
        CHECK_AT(earlyMenu, topLeft(lateMenu));
        checkSettled(early);
        checkSettled(late);
    }
}

TEST_CASE("Menu anchor: a strip menu opened, closed and opened again opens at the same place")
{
    for(float scale : {1.0f, 1.5f, 2.0f})
    {
        CAPTURE(scale);

        Scene scene(scale);
        controls::Menu *menu=scene.edit->getMenu();

        scene.edit->openMenu();
        scene.settle();

        const Point first=topLeft(menu);

        CHECK_AT(menu, belowItem(scene.edit));

        for(int i=0; i < 3; ++i)
        {
            CAPTURE(i);
            scene.edit->closeMenu();
            scene.settle();
            CHECK_FALSE(menu->visible());
            scene.edit->openMenu();
            scene.settle();
            CHECK(menu->visible());
            CHECK_AT(menu, first);
            checkSettled(scene);
        }
    }
}

TEST_CASE("Menu anchor: an open strip menu follows its item when an earlier item's text grows")
{
    for(float scale : {1.0f, 1.5f, 2.0f})
    {
        CAPTURE(scale);

        Scene scene(scale);
        controls::Menu *menu=scene.edit->getMenu();

        scene.settle();
        scene.edit->openMenu();
        scene.settle();

        const Rect before=canvasRect(scene.edit);

        CHECK_AT(menu, belowItem(scene.edit));
        checkSettled(scene);

        scene.file->setText("File with a much longer caption");
        scene.file->sizeToContents();
        scene.settle();

        REQUIRE(canvasRect(scene.edit).x > before.x);
        CHECK(menu->visible());
        CHECK_AT(menu, belowItem(scene.edit));
        checkSettled(scene);
    }
}

TEST_CASE("Menu anchor: an open strip menu follows its item when the canvas is resized")
{
    for(float scale : {1.0f, 1.5f, 2.0f})
    {
        CAPTURE(scale);

        Scene scene(scale, true);
        controls::Menu *menu=scene.file->getMenu();

        scene.settle();
        scene.file->openMenu();
        scene.settle();

        const Rect before=canvasRect(scene.file);

        CHECK_AT(menu, belowItem(scene.file));

        for(int width : {340, 460, 400})
        {
            CAPTURE(width);
            scene.canvas->setSize(width, 300);
            scene.settle();

            const Rect item=canvasRect(scene.file);

            REQUIRE(item.x == before.x+width-400);
            CHECK(menu->visible());
            CHECK_AT(menu, belowItem(scene.file));
            // kept inside the canvas
            CHECK(menu->getX()+menu->getWidth() <= width);
            checkSettled(scene);
        }
    }
}

TEST_CASE("Menu anchor: a submenu opens on its item's right, its top at the item's top")
{
    for(float scale : {1.0f, 1.5f, 2.0f})
    {
        CAPTURE(scale);

        Scene scene(scale);
        controls::Menu *submenu=scene.recent->getMenu();

        scene.settle();
        scene.file->openMenu();
        scene.settle();
        scene.recent->openMenu();
        scene.settle();

        const Rect item=canvasRect(scene.recent);

        REQUIRE(submenu->visible());
        REQUIRE(item.x+item.w+submenu->getWidth() <= 400);
        CHECK(submenu->getX() == item.x+item.w);
        CHECK(submenu->getY() == item.y);
        checkSettled(scene);
    }
}

TEST_CASE("Menu anchor: a submenu opens on its item's left where the canvas's right edge leaves no room")
{
    for(float scale : {1.0f, 1.5f, 2.0f})
    {
        CAPTURE(scale);

        Scene scene(scale, true);
        controls::Menu *submenu=scene.recent->getMenu();

        scene.settle();
        scene.file->openMenu();
        scene.settle();
        scene.recent->openMenu();
        scene.settle();

        const Rect item=canvasRect(scene.recent);

        REQUIRE(submenu->visible());
        // past the right edge on the right, room on the left
        REQUIRE(item.x+item.w+submenu->getWidth() > 400);
        REQUIRE(item.x-submenu->getWidth() >= 0);
        CHECK(submenu->getX()+submenu->getWidth() == item.x);
        CHECK(submenu->getY() == item.y);
        // left of its parent menu, over at most its padding
        CHECK(submenu->getX() < scene.file->getMenu()->getX());
        checkSettled(scene);
    }
}

TEST_CASE("Menu anchor: openMenu anchors the menu to its item until it is hidden, a request or openAt")
{
    Scene scene;
    controls::Menu *menu=scene.edit->getMenu();

    scene.settle();
    CHECK(menu->getAnchor() == nullptr);

    scene.edit->openMenu();
    CHECK(menu->getAnchor() == scene.edit);
    CHECK(menu->getPlacement() == controls::Placement::Owner);
    scene.settle();
    CHECK_AT(menu, belowItem(scene.edit));

    SUBCASE("hidden")
    {
        scene.edit->closeMenu();
        CHECK(menu->getAnchor() == nullptr);
        scene.edit->openMenu();
        CHECK(menu->getAnchor() == scene.edit);
    }

    SUBCASE("a setPos after it wins, kept inside the canvas")
    {
        menu->setPos(200, 150);
        scene.settle();
        CHECK(menu->getAnchor() == nullptr);
        CHECK_AT(menu, Point(200, 150));

        // the item moving takes it nowhere
        scene.file->setText("File with a much longer caption");
        scene.file->sizeToContents();
        scene.settle();
        CHECK_AT(menu, Point(200, 150));

        menu->setPos(390, 150);
        scene.settle();
        CHECK_AT(menu, Point(400-menu->getWidth(), 150));
    }

    SUBCASE("openAt places it at the cursor instead")
    {
        menu->openAt(Point(100, 120));
        CHECK(menu->getAnchor() == nullptr);
        scene.settle();
        CHECK_AT(menu, Point(100, 120));

        // flipped to the cursor's left at the right edge, as a context menu
        menu->openAt(Point(390, 120));
        scene.settle();
        CHECK_AT(menu, Point(390-menu->getWidth(), 120));
    }
}

TEST_CASE("Menu anchor: a menu opened next to its item is the size the canvas gave it before")
{
    Scene scene;
    controls::Menu *anchored=scene.edit->getMenu();
    // the same items in a menu the canvas places
    controls::Menu *placed=scene.canvas->newChild<controls::Menu>();

    placed->addItem("Undo");
    placed->addItem("Redo");
    scene.settle();
    scene.edit->openMenu();
    placed->openAt(Point(200, 100));
    scene.settle();

    CHECK(anchored->getPlacement() == controls::Placement::Owner);
    CHECK(placed->getPlacement() == controls::Placement::Parent);
    CHECK(anchored->getWidth() == placed->getWidth());
    CHECK(anchored->getHeight() == placed->getHeight());
    CHECK(anchored->getHeight() > 0);
}
