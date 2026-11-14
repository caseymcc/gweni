/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A Menu's opt-in keyboard navigation (Menu::startKeyboardNavigation, the
 *  MenuFocus argument of openNextTo and openAt) on a real Canvas drawn by the
 *  (un-init()'d) textured skin, keyed through input::onKeyEvent and moused
 *  through inputMouseMoved:
 *  - opened with controls::MenuFocus::FirstItem it takes the keyboard focus and
 *    highlights its first enabled item; Up/Down move the highlight over the
 *    enabled, shown items, skipping dividers and disabled and hidden items and
 *    wrapping, and Home/End go to the first/last;
 *  - Return and Space choose the highlighted item as a click does
 *    (onMenuItemSelected once, a checkable item toggled, every menu closed);
 *  - Right opens the highlighted item's submenu with its first item
 *    highlighted and the focus in it, Left closes it back to its item, which
 *    stays highlighted in the parent menu with the focus back there, and
 *    Return on a submenu's item opens it as Right does;
 *  - Escape closes it (a submenu one level) and gives the focus back to the
 *    control that had it at the open, also when an item chosen in a submenu
 *    closes the whole chain;
 *  - the mouse and the keys share one highlight: the mouse onto an enabled
 *    item highlights it, the keys move on from there, the hovered item shows
 *    no highlight once the keys moved it away, and a highlight move writes the
 *    two items' slots alone;
 *  - a menu opened without it (as a ComboBox's list, a context menu or a
 *    MenuStrip's menus are) takes no focus, reacts to no key, and highlights
 *    the hovered item as before; and once a navigating menu is hidden, an
 *    open without it is the plain menu again.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace gweni;

namespace
{

//! Records the slots each frame draws into, shows or hides.
class SlotRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color, Color, Color, int) override
    {
        touchedIds.push_back(primitiveId);
    }

    void drawFilledRect(size_t primitiveId, Rect, int) override
    {
        touchedIds.push_back(primitiveId);
    }

    void drawIcon(size_t primitiveId, Rect, int, int, Color, int) override
    {
        touchedIds.push_back(primitiveId);
    }

    void showPrimitive(int, size_t primitiveId) override
    {
        touchedIds.push_back(primitiveId);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        touchedIds.push_back(primitiveId);
    }

    std::vector<size_t> touchedIds;
};

//! Counts a Caller's calls.
class Counter: public event::Handler
{
public:
    void count(event::Info)
    {
        ++calls;
    }

    int calls=0;
};

//! A Button to open from and a menu below it: One, a divider, Two (disabled),
//! Three, Four (checkable), Sub (a submenu of A, B), Five (hidden).
struct Scene
{
    SlotRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Button *opener=nullptr;
    controls::Menu *menu=nullptr;
    controls::MenuItem *one=nullptr;
    controls::MenuItem *two=nullptr;
    controls::MenuItem *three=nullptr;
    controls::MenuItem *four=nullptr;
    controls::MenuItem *sub=nullptr;
    controls::MenuItem *five=nullptr;
    controls::MenuItem *subA=nullptr;
    controls::MenuItem *subB=nullptr;
    Counter chosen;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);

        opener=canvas->newChild<controls::Button>();
        opener->setText("Open");
        opener->setBounds(20, 20, 80, 24);
        opener->setKeyboardInputEnabled(true);

        menu=canvas->newChild<controls::Menu>();
        menu->setHidden(true);
        one=menu->addItem("One");
        menu->addDivider();
        two=menu->addItem("Two");
        two->setDisabled(true);
        three=menu->addItem("Three");
        four=menu->addItem("Four");
        four->setCheckable(true);
        sub=menu->addItem("Sub");
        subA=sub->getMenu()->addItem("A");
        subB=sub->getMenu()->addItem("B");
        five=menu->addItem("Five");
        five->setHidden(true);

        for(controls::MenuItem *item : {one, two, three, four, subA, subB})
            item->onMenuItemSelected.add(&chosen, &Counter::count);

        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered and focused controls are process-global
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        renderer.touchedIds.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void key(int code)
    {
        input::onKeyEvent(canvas, code, true);
        input::onKeyEvent(canvas, code, false);
        frame();
    }

    //! The opener focused, then the menu opened below it navigating by keyboard.
    void openNavigating()
    {
        opener->focus();
        menu->openNextTo(opener, controls::PopupPlacement::Below, 1, controls::MenuFocus::FirstItem);
        frame();
        frame();
    }

    void moveOnto(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        frame();
    }

    static const std::vector<size_t> &ids(const controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives();
    }

    //! True if every slot touched in the last frame is one of the controls'.
    bool touchedOnly(std::initializer_list<const controls::Base *> controls) const
    {
        std::vector<size_t> own;

        for(const controls::Base *control : controls)
            own.insert(own.end(), ids(control).begin(), ids(control).end());
        for(size_t id : renderer.touchedIds)
        {
            if(std::find(own.begin(), own.end(), id) == own.end())
                return false;
        }
        return true;
    }

    bool touched(const controls::Base *control) const
    {
        for(size_t id : ids(control))
        {
            if(std::find(renderer.touchedIds.begin(), renderer.touchedIds.end(), id) != renderer.touchedIds.end())
                return true;
        }
        return false;
    }
};

}//namespace

TEST_CASE("Menu keyboard navigation: the focus and the first enabled item on open, Up/Down skipping dividers and disabled and hidden items and wrapping, and Home/End")
{
    Scene scene;

    scene.openNavigating();
    REQUIRE_FALSE(scene.menu->hidden());
    CHECK(scene.menu->isKeyboardNavigating());
    CHECK(scene.menu->isFocussed());
    CHECK(scene.menu->getHighlightedItem() == scene.one);
    CHECK(scene.one->isHighlighted());

    // past the divider and the disabled Two
    scene.key(keys::Down);
    CHECK(scene.menu->getHighlightedItem() == scene.three);
    CHECK_FALSE(scene.one->isHighlighted());
    CHECK(scene.three->isHighlighted());
    scene.key(keys::Down);
    CHECK(scene.menu->getHighlightedItem() == scene.four);
    scene.key(keys::Down);
    CHECK(scene.menu->getHighlightedItem() == scene.sub);
    // the hidden Five is skipped: wraps to One
    scene.key(keys::Down);
    CHECK(scene.menu->getHighlightedItem() == scene.one);
    // and back up, wrapping to Sub
    scene.key(keys::Up);
    CHECK(scene.menu->getHighlightedItem() == scene.sub);
    scene.key(keys::Up);
    CHECK(scene.menu->getHighlightedItem() == scene.four);
    scene.key(keys::Up);
    CHECK(scene.menu->getHighlightedItem() == scene.three);
    scene.key(keys::Up);
    CHECK(scene.menu->getHighlightedItem() == scene.one);

    scene.key(keys::End);
    CHECK(scene.menu->getHighlightedItem() == scene.sub);
    scene.key(keys::Home);
    CHECK(scene.menu->getHighlightedItem() == scene.one);

    // the keys never left the menu, which is still open
    CHECK(scene.menu->isFocussed());
    CHECK_FALSE(scene.menu->hidden());
    CHECK(scene.chosen.calls == 0);
}

TEST_CASE("Menu keyboard navigation: Return and Space choose as a click, a checkable item toggles, and the menu closes")
{
    Scene scene;

    scene.openNavigating();
    scene.key(keys::Down);
    scene.key(keys::Down);
    REQUIRE(scene.menu->getHighlightedItem() == scene.four);
    REQUIRE_FALSE(scene.four->isChecked());

    scene.key(keys::Return);
    CHECK(scene.chosen.calls == 1);
    CHECK(scene.four->isChecked());
    CHECK(scene.menu->hidden());
    CHECK_FALSE(scene.menu->isKeyboardNavigating());
    CHECK(scene.opener->isFocussed());

    // Space too, the check back off
    scene.openNavigating();
    scene.key(keys::End);
    scene.key(keys::Up);
    REQUIRE(scene.menu->getHighlightedItem() == scene.four);
    scene.key(keys::Space);
    CHECK(scene.chosen.calls == 2);
    CHECK_FALSE(scene.four->isChecked());
    CHECK(scene.menu->hidden());
    CHECK(scene.opener->isFocussed());
}

TEST_CASE("Menu keyboard navigation: Right opens a submenu with its first item highlighted, Left closes it back to its item")
{
    Scene scene;
    controls::Menu *submenu=scene.sub->getMenu();

    scene.openNavigating();
    scene.key(keys::End);
    REQUIRE(scene.menu->getHighlightedItem() == scene.sub);

    scene.key(keys::Right);
    REQUIRE_FALSE(submenu->hidden());
    CHECK(submenu->isKeyboardNavigating());
    CHECK(submenu->isFocussed());
    CHECK(submenu->getHighlightedItem() == scene.subA);
    // the parent item stays highlighted, its submenu open
    CHECK(scene.sub->isHighlighted());

    scene.key(keys::Down);
    CHECK(submenu->getHighlightedItem() == scene.subB);
    // the parent menu's highlight did not move
    CHECK(scene.menu->getHighlightedItem() == scene.sub);

    scene.key(keys::Left);
    CHECK(submenu->hidden());
    CHECK_FALSE(scene.menu->hidden());
    CHECK(scene.menu->isFocussed());
    CHECK(scene.menu->getHighlightedItem() == scene.sub);
    CHECK(scene.sub->isHighlighted());

    // Right on an item with no submenu does nothing; Left in the first menu
    // keeps it open
    scene.key(keys::Home);
    scene.key(keys::Right);
    CHECK(scene.menu->isFocussed());
    scene.key(keys::Left);
    CHECK_FALSE(scene.menu->hidden());
    CHECK(scene.menu->isFocussed());

    // Return on the submenu's item opens it as Right does; choosing in the
    // submenu closes the chain and the focus goes back to the opener
    scene.key(keys::End);
    scene.key(keys::Return);
    REQUIRE_FALSE(submenu->hidden());
    REQUIRE(submenu->isFocussed());
    scene.key(keys::Return);
    CHECK(scene.chosen.calls == 1);
    CHECK(submenu->hidden());
    CHECK(scene.menu->hidden());
    CHECK(scene.opener->isFocussed());
}

TEST_CASE("Menu keyboard navigation: Escape closes it, a submenu one level, and the focus goes back")
{
    Scene scene;
    controls::Menu *submenu=scene.sub->getMenu();

    scene.openNavigating();
    scene.key(keys::End);
    scene.key(keys::Right);
    REQUIRE(submenu->isFocussed());

    scene.key(keys::Escape);
    CHECK(submenu->hidden());
    CHECK_FALSE(scene.menu->hidden());
    CHECK(scene.menu->isFocussed());

    scene.key(keys::Escape);
    CHECK(scene.menu->hidden());
    CHECK_FALSE(scene.menu->isKeyboardNavigating());
    CHECK(scene.menu->getHighlightedItem() == nullptr);
    CHECK(scene.opener->isFocussed());
    CHECK(scene.chosen.calls == 0);

    // closed from outside (the canvas closing every menu) with nothing
    // focused before: the focus is dropped, not left on the hidden menu
    scene.opener->unfocus();
    scene.menu->openNextTo(scene.opener, controls::PopupPlacement::Below, 1, controls::MenuFocus::FirstItem);
    scene.frame();
    REQUIRE(scene.menu->isFocussed());
    scene.canvas->closeMenus();
    scene.frame();
    CHECK(scene.menu->hidden());
    CHECK(KeyboardFocus == nullptr);
}

TEST_CASE("Menu keyboard navigation: the mouse and the keys share one highlight, and a move writes the two items' slots alone")
{
    Scene scene;

    scene.openNavigating();
    REQUIRE(scene.menu->getHighlightedItem() == scene.one);

    // the mouse onto Three moves the highlight there
    scene.moveOnto(scene.three);
    CHECK(scene.menu->getHighlightedItem() == scene.three);
    CHECK(scene.three->isHighlighted());
    CHECK_FALSE(scene.one->isHighlighted());

    // settled: nothing written
    scene.frame();
    CHECK(scene.renderer.touchedIds.empty());

    // the keys go on from there; the hovered Three loses its look
    input::onKeyEvent(scene.canvas, keys::Down, true);
    input::onKeyEvent(scene.canvas, keys::Down, false);
    scene.frame();
    CHECK(scene.menu->getHighlightedItem() == scene.four);
    CHECK(HoveredControl == scene.three);
    CHECK_FALSE(scene.three->isHighlighted());
    CHECK(scene.four->isHighlighted());
    CHECK(scene.touched(scene.three));
    CHECK(scene.touched(scene.four));
    CHECK(scene.touchedOnly({scene.three, scene.four}));

    // the mouse onto the disabled Two takes no highlight
    scene.moveOnto(scene.two);
    CHECK(scene.menu->getHighlightedItem() == scene.four);
    CHECK_FALSE(scene.two->isHighlighted());

    // and the keys leave the menu still focused, the opener unhovered
    CHECK(scene.menu->isFocussed());
}

TEST_CASE("Menu keyboard navigation: a menu opened without it takes no focus, no key, and highlights the hovered item")
{
    Scene scene;

    scene.opener->focus();
    scene.menu->openNextTo(scene.opener, controls::PopupPlacement::Below, 1);
    scene.frame();
    scene.frame();
    REQUIRE_FALSE(scene.menu->hidden());
    CHECK_FALSE(scene.menu->isKeyboardNavigating());
    CHECK(scene.opener->isFocussed());
    CHECK(scene.menu->getHighlightedItem() == nullptr);
    CHECK_FALSE(scene.one->isHighlighted());

    // keys go to the opener, the menu stays as it is
    for(int code : {keys::Down, keys::Up, keys::Home, keys::End, keys::Right, keys::Left, keys::Return})
        scene.key(code);
    CHECK_FALSE(scene.menu->hidden());
    CHECK(scene.opener->isFocussed());
    CHECK(scene.chosen.calls == 0);
    CHECK(scene.menu->getHighlightedItem() == nullptr);
    // a key on the menu itself is not taken either
    CHECK_FALSE(scene.menu->onKeyDown(true));
    CHECK_FALSE(scene.menu->onKeyReturn(true));

    // the hover is the highlight, as before
    scene.moveOnto(scene.three);
    CHECK(scene.three->isHighlighted());
    CHECK(scene.menu->getHighlightedItem() == nullptr);
    scene.moveOnto(scene.four);
    CHECK(scene.four->isHighlighted());
    CHECK_FALSE(scene.three->isHighlighted());

    // a navigating open, hidden, then a plain open: the plain menu again
    scene.menu->close();
    scene.frame();
    scene.openNavigating();
    REQUIRE(scene.menu->isKeyboardNavigating());
    scene.key(keys::Escape);
    scene.menu->openNextTo(scene.opener, controls::PopupPlacement::Below, 1);
    scene.frame();
    CHECK_FALSE(scene.menu->isKeyboardNavigating());
    CHECK(scene.opener->isFocussed());
    scene.moveOnto(scene.one);
    CHECK(scene.one->isHighlighted());
}
