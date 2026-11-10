/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A MenuItem owns the submenu MenuItem::getMenu() makes, which is a canvas
 *  child (so it draws above and outside its menu), not the item's (#518).
 *  Every case takes a baseline of the live primitive and text ids, the
 *  canvas's children and the skin's live Controls, builds menus whose items
 *  have submenus three levels deep, opens each so it is drawn (and takes its
 *  ids), and requires the baseline back once it is deleted.
 *
 *  Deleting an item with a submenu deletes the submenu and, recursively, its
 *  items' submenus: an outright delete, delayedDelete(), Menu::clearItems(),
 *  and deleting the owning Menu or MenuStrip, all gone by the end of the next
 *  frame (Canvas::processDelayedDeletes runs until nothing is queued). An
 *  open submenu, and its own open submenu, are hidden in the call that deletes
 *  their item. A submenu deleted on its own (outright or delayed) leaves its
 *  item a plain item: isMenuOpen() false, openMenu()/closeMenu()/
 *  Menu::closeAll() no-ops, the row marked to be drawn again (an open
 *  submenu's highlight goes), no submenu arrow after the next frame, and a press
 *  checks a checkable item and fires onMenuItemSelected. A submenu deleted
 *  outright after its item, or its menu, was queued for a delayed delete
 *  (clearItems() included) queues the item's arrow behind it, and the next
 *  frame deletes each control once. A submenu item's own
 *  onMenuItemSelected handler may delete the submenu's parent item, outright
 *  or delayed: the submenu is only queued while the handler is on the stack.
 *  A canvas deleted with a MenuStrip and a Menu holding nested submenus, open
 *  or closed, deletes every control once (the skin's live Controls back to 0,
 *  every id given back), with the submenus after their strip or menu, or
 *  before it once the strip or menu was brought to the front. The
 *  use-after-free cases (a submenu deleted first, then its item used or
 *  deleted) are real checks only in the ASAN tree.
 */
#include "headlessFixture.h"

#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/menuStrip.h>

#include <doctest/doctest.h>

using namespace gweni;
using controls::Menu;
using controls::MenuItem;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    //! Both phases: the delayed deletes go, every pending change is drawn.
    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

//! What a case must give back.
struct Counts
{
    test::IdBankCounts ids;
    size_t canvasChildren;
    size_t skinControls;

    static Counts of(Scene &scene)
    {
        return {test::IdBankCounts::now(), scene.canvas->getActualChildren().size(),
            scene.skin.liveControls};
    }
};

void checkBack(Scene &scene, const Counts &baseline)
{
    const Counts now=Counts::of(scene);

    CHECK(now.ids.livePrimitives() == baseline.ids.livePrimitives());
    CHECK(now.ids.liveTexts() == baseline.ids.liveTexts());
    CHECK(now.canvasChildren == baseline.canvasChildren);
    CHECK(now.skinControls == baseline.skinControls);
}

//! An item of a Menu with a submenu three levels deep: the item's submenu
//! holds a leaf and an item with a submenu, and so on, the last submenu
//! holding a leaf alone.
struct Nested
{
    MenuItem *top=nullptr;
    //! The items with a submenu, top first.
    std::vector<MenuItem *> items;
    //! Their submenus, in the same order.
    std::vector<Menu *> subs;

    Nested(Menu *menu, const char *name)
    {
        top=menu->addItem(name);
        MenuItem *item=top;

        for(int level=0; level < 3; ++level)
        {
            Menu *sub=item->getMenu();

            items.push_back(item);
            subs.push_back(sub);
            sub->addItem("Leaf");
            if(level < 2)
                item=sub->addItem("Deeper");
        }
    }

    //! Opens every submenu, so a frame draws them all.
    void open()
    {
        for(MenuItem *item : items)
            item->openMenu();
    }
};

bool hasArrow(MenuItem *item)
{
    for(controls::Base *child : item->getChildren())
    {
        if(child->getTypeName() == std::string("RightArrow"))
            return true;
    }
    return false;
}

//! Counts an item's onMenuItemSelected calls.
struct Selections
{
    int count=0;

    explicit Selections(MenuItem *item)
    {
        item->onMenuItemSelected.addCallback(item,
            [this](event::Handler &, event::Info const &) {++count;});
    }
};

}//namespace

TEST_CASE("deleting a Menu deletes its items' submenus, three levels deep")
{
    Scene scene;
    scene.frame();
    const Counts baseline=Counts::of(scene);

    Menu *menu=scene.canvas->newChild<Menu>();
    menu->addItem("Plain");
    Nested nested(menu, "Nested");
    menu->openAt(Point(10, 10));
    scene.frame();
    nested.open();
    scene.frame();
    REQUIRE(Counts::of(scene).canvasChildren == baseline.canvasChildren+4);

    SUBCASE("outright")
    {
        // ~Menu is protected: deleted as a Base, as the canvas does
        delete static_cast<controls::Base *>(menu);
    }
    SUBCASE("delayed")
    {
        menu->delayedDelete();
    }
    scene.frame();
    checkBack(scene, baseline);
}

TEST_CASE("deleting a MenuItem deletes its submenu tree, outright and delayed")
{
    Scene scene;
    Menu *menu=scene.canvas->newChild<Menu>();
    menu->addItem("Stays");
    menu->openAt(Point(10, 10));
    scene.frame();
    const Counts baseline=Counts::of(scene);

    Nested chain(menu, "Nested");
    MenuItem *nested=chain.top;
    scene.frame();
    chain.open();
    scene.frame();
    REQUIRE(Counts::of(scene).canvasChildren == baseline.canvasChildren+3);

    SUBCASE("outright")
    {
        delete nested;
    }
    SUBCASE("delayed")
    {
        nested->delayedDelete();
    }
    SUBCASE("closed first, then deleted")
    {
        scene.canvas->closeMenus();
        menu->openAt(Point(10, 10));
        scene.frame();
        delete nested;
    }
    scene.frame();
    checkBack(scene, baseline);
    CHECK(menu->getChildren().size() == 1);
}

TEST_CASE("Menu::clearItems() deletes the items' submenus, so a rebuilt submenu leaks nothing")
{
    Scene scene;
    MenuItem *recent=nullptr;
    Menu *menu=scene.canvas->newChild<Menu>();
    recent=menu->addItem("Recent");
    menu->openAt(Point(10, 10));
    Menu *files=recent->getMenu();
    recent->openMenu();
    scene.frame();
    const Counts baseline=Counts::of(scene);

    // an app rebuilding its "Recent files" list
    for(int rebuild=0; rebuild < 3; ++rebuild)
    {
        CAPTURE(rebuild);
        Nested(files, "a.txt").open();
        Nested(files, "b.txt").open();
        scene.frame();
        REQUIRE(Counts::of(scene).canvasChildren == baseline.canvasChildren+6);

        files->clearItems();
        scene.frame();
        checkBack(scene, baseline);
    }
}

TEST_CASE("deleting a MenuStrip deletes its menus and their submenus")
{
    Scene scene;
    scene.frame();
    const Counts baseline=Counts::of(scene);

    auto *strip=scene.canvas->newChild<controls::MenuStrip>();
    strip->setDock(DockPosition::Top);
    MenuItem *file=strip->addItem("File");
    Menu *fileMenu=file->getMenu();
    fileMenu->addItem("Open");
    Nested recent(fileMenu, "Recent");
    MenuItem *edit=strip->addItem("Edit");
    edit->getMenu()->addItem("Copy");
    scene.frame();
    file->openMenu();
    scene.frame();
    recent.open();
    scene.frame();
    // the strip, File's and Edit's menus, and Recent's three levels
    REQUIRE(Counts::of(scene).canvasChildren == baseline.canvasChildren+6);

    SUBCASE("outright")
    {
        delete strip;
    }
    SUBCASE("delayed")
    {
        strip->delayedDelete();
    }
    scene.frame();
    checkBack(scene, baseline);
}

TEST_CASE("an open submenu is hidden in the call that deletes its item, its own open submenu too")
{
    Scene scene;
    Menu *menu=scene.canvas->newChild<Menu>();
    menu->openAt(Point(10, 10));
    Nested nested(menu, "Nested");
    scene.frame();
    nested.open();
    scene.frame();
    for(Menu *sub : nested.subs)
        REQUIRE_FALSE(sub->hidden());

    delete nested.top;
    // still alive: queued for the next frame's delayed deletes
    for(Menu *sub : nested.subs)
        CHECK(sub->hidden());
    CHECK_FALSE(menu->isMenuOpen());
    scene.frame();
}

TEST_CASE("a submenu deleted on its own leaves its item a plain item")
{
    Scene scene;
    Menu *menu=scene.canvas->newChild<Menu>();
    menu->addItem("Stays");
    menu->openAt(Point(10, 10));
    scene.frame();
    const Counts baseline=Counts::of(scene);

    MenuItem *item=menu->addItem("Item");
    item->setCheckable(true);
    Selections selections(item);
    Menu *sub=item->getMenu();
    sub->addItem("Inner")->getMenu()->addItem("Leaf");
    item->openMenu();
    scene.frame();
    REQUIRE(hasArrow(item));
    REQUIRE(item->isMenuOpen());

    const Counts withItem=Counts::of(scene);

    SUBCASE("outright")
    {
        delete static_cast<controls::Base *>(sub);
        // the open highlight goes: the row is drawn again, as a plain one
        CHECK((item->getStateChange() & controls::StateChange_Background) != 0);
        scene.frame();
    }
    SUBCASE("delayed")
    {
        sub->delayedDelete();
        scene.canvas->updateCanvas();
        CHECK((item->getStateChange() & controls::StateChange_Background) != 0);
        scene.canvas->renderCanvas();
    }
    CHECK_FALSE(hasArrow(item));
    CHECK_FALSE(item->isMenuOpen());
    item->openMenu();
    item->closeMenu();
    item->toggleMenu();
    menu->closeAll();
    CHECK_FALSE(menu->isMenuOpen());
    CHECK_FALSE(item->isMenuOpen());

    // a press is a plain item's: it checks and selects it
    item->doAction();
    CHECK(item->isChecked());
    CHECK(selections.count == 1);

    menu->openAt(Point(10, 10));
    scene.frame();
    CHECK_FALSE(hasArrow(item));
    // the submenu's controls and ids, and the arrow's, are gone; the item's stay
    const Counts now=Counts::of(scene);
    CHECK(now.canvasChildren == baseline.canvasChildren);
    CHECK(now.skinControls < withItem.skinControls);
    CHECK(now.ids.livePrimitives() < withItem.ids.livePrimitives());

    delete item;
    scene.frame();
    checkBack(scene, baseline);
}

TEST_CASE("a submenu deleted outright after its item was queued for delete")
{
    Scene scene;
    const Counts baseline=Counts::of(scene);

    Menu *menu=scene.canvas->newChild<Menu>();
    MenuItem *item=menu->addItem("Item");
    Menu *sub=item->getMenu();
    sub->addItem("Inner")->getMenu()->addItem("Leaf");
    menu->openAt(Point(10, 10));
    item->openMenu();
    scene.frame();
    REQUIRE(hasArrow(item));

    // the item's arrow, queued by the submenu's delete, is queued after the
    // item (or its menu): deleting that deletes the arrow with it
    SUBCASE("Menu::clearItems()")
    {
        menu->clearItems();
    }
    SUBCASE("the item delayed")
    {
        item->delayedDelete();
    }
    SUBCASE("the menu delayed")
    {
        menu->delayedDelete();
        menu=nullptr;
    }
    delete static_cast<controls::Base *>(sub);
    scene.frame();

    delete static_cast<controls::Base *>(menu);
    scene.frame();
    checkBack(scene, baseline);
}

TEST_CASE("a submenu item's handler may delete the submenu's parent item")
{
    Scene scene;
    Menu *menu=scene.canvas->newChild<Menu>();
    menu->addItem("Stays");
    menu->openAt(Point(10, 10));
    scene.frame();
    const Counts baseline=Counts::of(scene);

    MenuItem *parent=menu->addItem("Parent");
    Menu *sub=parent->getMenu();
    MenuItem *remove=sub->addItem("Remove me");
    sub->addItem("Other")->getMenu()->addItem("Leaf");
    parent->openMenu();
    scene.frame();

    bool outright=false;
    SUBCASE("outright")
    {
        outright=true;
    }
    SUBCASE("delayed")
    {
        outright=false;
    }
    int calls=0;
    remove->onMenuItemSelected.addCallback(remove,
        [&](event::Handler &, event::Info const &)
        {
            ++calls;
            if(outright)
                delete parent;
            else
                parent->delayedDelete();
        });

    remove->doAction();
    CHECK(calls == 1);
    scene.frame();
    checkBack(scene, baseline);
}

TEST_CASE("a canvas deleted with menus holding nested submenus deletes every control once, in any order")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    // 0: as built (submenus after their strip or menu), 1: the strip brought
    // to the front, 2: the menu brought to the front, 3: both, 4: a submenu
    // brought to the front
    for(const int order : {0, 1, 2, 3, 4})
    {
        for(const bool open : {false, true})
        {
            CAPTURE(order);
            CAPTURE(open);
            {
                Scene scene;

                auto *strip=scene.canvas->newChild<controls::MenuStrip>();
                strip->setDock(DockPosition::Top);
                MenuItem *file=strip->addItem("File");
                Nested recent(file->getMenu(), "Recent");
                Menu *menu=scene.canvas->newChild<Menu>();
                Nested nested(menu, "Nested");
                menu->setPos(200, 100);
                scene.frame();

                if(open)
                {
                    file->openMenu();
                    recent.open();
                    menu->setHidden(false);
                    nested.open();
                }
                if(order == 1 || order == 3)
                    strip->bringToFront();
                if(order == 2 || order == 3)
                    menu->bringToFront();
                if(order == 4)
                    nested.subs.front()->bringToFront();
                scene.frame();
                REQUIRE(scene.skin.liveControls > 0);

                test::StubSkin *skin=&scene.skin;
                HoveredControl=nullptr;
                KeyboardFocus=nullptr;
                MouseFocus=nullptr;
                delete scene.canvas;
                scene.canvas=nullptr;
                CHECK(skin->liveControls == 0);
            }
        }
    }
    const test::IdBankCounts after=test::IdBankCounts::now();
    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
}
