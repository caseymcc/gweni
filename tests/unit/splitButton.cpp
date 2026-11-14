/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  DropDownButton on a real Canvas drawn by the (un-init()'d) textured skin,
 *  clicked through input::onMouseClicked and keyed through input::onKeyEvent:
 *  - a click opens getMenu() below it, the menu taking the keyboard focus
 *    with nothing highlighted (MenuFocus::Menu), and a second click closes it
 *    with the focus back on the button;
 *  - Space, Return, Alt+Down and F4 on the focused button each open it with
 *    its first enabled item highlighted, and Escape closes it with the focus
 *    back; a plain Down opens nothing;
 *  - while the menu is open the button is pressed (isDepressed()) and its
 *    renderer draws ButtonDown in slot [0], the down chevron (style 5, kind
 *    1, direction 0) in slot [1] at z+1 in its text colour; opening and
 *    closing write the button's slots and the menu's alone;
 *  - opening it closes another open menu (a second DropDownButton's);
 *  - disabled, neither a click nor a key opens it;
 *  - deleting the button deletes its menu.
 */
#include "headlessFixture.h"

#include <gweni/controls/dropDownButton.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace gweni;

namespace
{

//! Records the slots each frame draws into, shows or hides, with the top
//! colour of each rounded rect and the icons.
class SlotRecorder: public test::StubRenderer
{
public:
    struct Icon
    {
        size_t id;
        int kind;
        int direction;
        Color color;
        int zIndex;
    };

    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color top, Color, Color, int) override
    {
        touchedIds.push_back(primitiveId);
        rects.push_back({primitiveId, top});
    }

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex, uint8_t, float) override
    {
        drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
    }

    void drawFilledRect(size_t primitiveId, Rect, int) override
    {
        touchedIds.push_back(primitiveId);
    }

    void drawIcon(size_t primitiveId, Rect, int kind, int direction, Color color, int zIndex) override
    {
        touchedIds.push_back(primitiveId);
        icons.push_back({primitiveId, kind, direction, color, zIndex});
    }

    void showPrimitive(int, size_t primitiveId) override
    {
        touchedIds.push_back(primitiveId);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        touchedIds.push_back(primitiveId);
    }

    //! The top colour last drawn into the slot, or a transparent one.
    Color lastTop(size_t id) const
    {
        for(auto it=rects.rbegin(); it != rects.rend(); ++it)
        {
            if(it->first == id)
                return it->second;
        }
        return Color(0, 0, 0, 0);
    }

    const Icon *lastIcon(size_t id) const
    {
        for(auto it=icons.rbegin(); it != icons.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    void clear()
    {
        touchedIds.clear();
        rects.clear();
        icons.clear();
    }

    std::vector<size_t> touchedIds;
    std::vector<std::pair<size_t, Color>> rects;
    std::vector<Icon> icons;
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

struct Scene
{
    SlotRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    double clock=10.0;

    Scene()
    {
        // a second apart per press, so two presses are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
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

    //! A DropDownButton at (x, y) whose menu holds One, a divider, Two
    //! (disabled), Three.
    controls::DropDownButton *dropDown(int x, int y, const String &text="Options")
    {
        controls::DropDownButton *button=canvas->newChild<controls::DropDownButton>();

        button->setText(text);
        button->setBounds(x, y, 100, 24);

        controls::Menu *menu=button->getMenu();

        menu->addItem("One");
        menu->addDivider();
        menu->addItem("Two")->setDisabled(true);
        menu->addItem("Three");
        return button;
    }

    void frame()
    {
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void click(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        clock+=1.0;
        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        input::onMouseClicked(canvas, 0, true);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    void key(int code)
    {
        input::onKeyEvent(canvas, code, true);
        input::onKeyEvent(canvas, code, false);
        frame();
    }

    //! \p code with Alt held.
    void altKey(int code)
    {
        input::onKeyEvent(canvas, keys::Alt, true);
        input::onKeyEvent(canvas, code, true);
        input::onKeyEvent(canvas, code, false);
        input::onKeyEvent(canvas, keys::Alt, false);
        frame();
    }

    static const std::vector<size_t> &ids(const controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives();
    }

    static void collect(controls::Base *control, std::vector<size_t> &own)
    {
        if(control->getSkinControl())
            own.insert(own.end(), ids(control).begin(), ids(control).end());
        for(controls::Base *child : control->getActualChildren())
            collect(child, own);
    }

    //! True if every slot touched in the last frame is \p button's or one in
    //! its menu's subtree: every slot an open or a close may write.
    bool touchedOnly(controls::DropDownButton *button) const
    {
        std::vector<size_t> own(ids(button));

        collect(button->getMenu(), own);
        for(size_t id : renderer.touchedIds)
        {
            if(std::find(own.begin(), own.end(), id) == own.end())
                return false;
        }
        return true;
    }

    //! The menu's first enabled item.
    static controls::MenuItem *firstItem(controls::DropDownButton *button)
    {
        for(controls::Base *child : button->getMenu()->getChildren())
        {
            if(auto *item=gweni_cast<controls::MenuItem>(child))
                return item;
            for(controls::Base *inner : child->getChildren())
            {
                if(auto *item=gweni_cast<controls::MenuItem>(inner))
                    return item;
            }
        }
        return nullptr;
    }
};

}//namespace

TEST_CASE("DropDownButton: a click opens its menu below it with the focus in the menu, a second click closes it")
{
    Scene scene;
    controls::DropDownButton *button=scene.dropDown(40, 40);
    controls::Menu *menu=button->getMenu();
    Counter pressed;

    button->onPressCaller.add(&pressed, &Counter::count);
    scene.frame();
    REQUIRE(menu->hidden());
    REQUIRE_FALSE(button->isMenuOpen());

    scene.click(button);
    REQUIRE(button->isMenuOpen());
    scene.frame();
    // below the button, its left edge with the button's
    CHECK(menu->getY() >= button->getY()+button->getHeight());
    CHECK(menu->getX() == button->getX());
    CHECK(menu->getAnchor() == button);
    // the focus is in the menu, nothing highlighted until a key
    CHECK(menu->isKeyboardNavigating());
    CHECK(menu->isFocussed());
    CHECK(menu->getHighlightedItem() == nullptr);
    CHECK(pressed.calls == 1);

    // Down highlights the first item
    scene.key(keys::Down);
    CHECK(menu->getHighlightedItem() == Scene::firstItem(button));

    scene.click(button);
    CHECK_FALSE(button->isMenuOpen());
    CHECK(menu->hidden());
    CHECK(button->isFocussed());
    CHECK(pressed.calls == 2);

    // and a third opens it again
    scene.click(button);
    CHECK(button->isMenuOpen());
}

TEST_CASE("DropDownButton: Space, Return, Alt+Down and F4 open it with the first item highlighted; Escape closes it with the focus back")
{
    Scene scene;
    controls::DropDownButton *button=scene.dropDown(40, 40);
    controls::Menu *menu=button->getMenu();

    scene.frame();
    button->focus();
    REQUIRE(button->isFocussed());

    // a plain Down is not one of its keys
    scene.key(keys::Down);
    CHECK_FALSE(button->isMenuOpen());

    for(int code : {int(keys::Space), int(keys::Return), int(keys::Down), int(keys::F4)})
    {
        CAPTURE(code);
        if(code == keys::Down)
            scene.altKey(code);
        else
            scene.key(code);

        REQUIRE(button->isMenuOpen());
        CHECK(menu->isFocussed());
        CHECK(menu->getHighlightedItem() == Scene::firstItem(button));

        scene.key(keys::Escape);
        CHECK_FALSE(button->isMenuOpen());
        CHECK(button->isFocussed());
    }
}

TEST_CASE("DropDownButton: pressed look while its menu is open, its chevron at z+1, opening and closing writing the button's and the menu's slots alone")
{
    Scene scene;
    controls::DropDownButton *button=scene.dropDown(40, 40);
    controls::DropDownButton *other=scene.dropDown(200, 40, "Other");

    scene.frame();
    scene.frame();

    REQUIRE(Scene::ids(button).size() == 2);
    const size_t surface=Scene::ids(button)[0];
    const size_t chevron=Scene::ids(button)[1];

    // at rest: ButtonNormal, the chevron down in the text colour at z+1
    button->redraw();
    scene.frame();
    CHECK((scene.renderer.lastTop(surface) == skin::textured::tokyonight::ButtonNormal.top));
    const SlotRecorder::Icon *icon=scene.renderer.lastIcon(chevron);
    REQUIRE(icon != nullptr);
    CHECK(icon->kind == 1);
    CHECK(icon->direction == 0);
    CHECK(icon->zIndex == button->getZIndex()+1);
    CHECK((icon->color == button->getTextColor()));

    // opened from the keyboard, the mouse elsewhere: pressed
    button->focus();
    scene.frame();
    scene.key(keys::F4);
    REQUIRE(button->isMenuOpen());
    CHECK(button->isDepressed());
    CHECK((scene.renderer.lastTop(surface) == skin::textured::tokyonight::ButtonDown.top));
    CHECK(scene.touchedOnly(button));

    // settled with it open: nothing written
    scene.frame();
    CHECK(scene.renderer.touchedIds.empty());

    // closed: the focused look back
    scene.key(keys::Escape);
    CHECK_FALSE(button->isDepressed());
    CHECK((scene.renderer.lastTop(surface) == skin::textured::tokyonight::ButtonFocus.top));
    CHECK(scene.touchedOnly(button));

    // opening one closes the other's
    other->openMenu();
    scene.frame();
    REQUIRE(other->isMenuOpen());
    scene.click(button);
    CHECK(button->isMenuOpen());
    CHECK_FALSE(other->isMenuOpen());
}

TEST_CASE("DropDownButton: disabled, neither a click nor a key opens it")
{
    Scene scene;
    controls::DropDownButton *button=scene.dropDown(40, 40);

    scene.frame();
    button->focus();
    button->setDisabled(true);
    scene.frame();

    scene.click(button);
    CHECK_FALSE(button->isMenuOpen());
    button->focus();
    for(int code : {int(keys::Space), int(keys::Return), int(keys::F4)})
        scene.key(code);
    scene.altKey(keys::Down);
    CHECK_FALSE(button->isMenuOpen());
    button->openMenu();
    CHECK_FALSE(button->isMenuOpen());
}

TEST_CASE("DropDownButton: deleting it deletes its menu and gives back every id")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        Scene scene;
        controls::DropDownButton *button=scene.dropDown(40, 40);
        const size_t menuId=button->getMenu()->getControlId();

        scene.frame();
        scene.click(button);
        REQUIRE(button->isMenuOpen());

        button->delayedDelete();
        scene.frame();
        scene.frame();
        CHECK(controls::Base::getControlById(menuId) == nullptr);
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
}
