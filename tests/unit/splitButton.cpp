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
 *  - disabled, neither a click nor a key opens it, a click focuses it not and
 *    Tab skips it, and disabling it closes its open menu and leaves no focus
 *    on it, at once (the menu's hand-back skips it);
 *  - a submenu the mouse opens (Recent) under its open menu, its leaf
 *    clicked, gives the focus back to the button, the menu opened by a click
 *    or by Space; a press on that leaf let go off it leaves the submenu
 *    holding the focus, and two Escapes close both menus, the focus back;
 *  - a press on the open button let go off it leaves the focus on the button
 *    and the menu open, and Escape, Space, Return, Alt+Down or F4 closes it;
 *  - an item, chosen by Return or by a click, whose handler opens a dialog:
 *    the dialog closed, the focus is back on the button (the menu had given
 *    it back before the handler ran);
 *  - a dialog opened while its menu holds the focus, by a canvas accelerator
 *    (Ctrl+O) or directly (as a timer or a post would), the menu opened by
 *    Space or a click: the dialog closed, the focus is back on the button
 *    (Canvas::pushModal closes the menus before it reads the focus);
 *  - deleting the button deletes its menu, also one set to delete itself on
 *    close and taken off the canvas (a use-after-free here shows under ASAN).
 *
 *  SplitButton, on the same canvas:
 *  - its parts are laid out in one row, the main part as wide as its content
 *    and the arrow part ArrowWidth, a Divider unit apart, both its full height;
 *  - a click on the main part fires onPress and opens nothing, a click on the
 *    arrow part opens the menu below the control and fires nothing, the focus
 *    in the menu and back on the control (not a part) when it closes;
 *  - the control takes the focus as one: Space and Return press the main
 *    part, Alt+Down and F4 open the menu with its first item highlighted, a
 *    plain Down does nothing;
 *  - the toggle variant flips on each main press (onToggle), never on the
 *    arrow part's;
 *  - disabled, neither part nor any key does anything, a click on either part
 *    focuses it not and Tab skips it; enabled again it takes both, the arrow
 *    part neither;
 *  - its renderers: the main part square on its right with no border there,
 *    the arrow part square on its left with the chevron at z+1, the divider
 *    the SplitButtonDivider column between them; a hover of one part writes
 *    that part's slots alone, and the control's focus draws ButtonFocus in
 *    both parts; the divider takes the parts' border colour while the
 *    control is focused (ButtonFocus's) or disabled (the disabled main part's),
 *    redrawn by those changes and by the app's toggle of a disabled toggle
 *    one (ButtonOnDisabled's while on), and SplitButtonDivider otherwise;
 *  - a press on the open arrow part let go off it leaves the menu open with
 *    the focus on the control, and Escape, Alt+Down or F4 closes it, and
 *    Space or Return closes it and presses the main part once;
 *  - setMainItem makes a menu item the main part's action and text.
 */
#include "headlessFixture.h"

#include <gweni/controls/dialogs/query.h>
#include <gweni/controls/dropDownButton.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/splitButton.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
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

    void drawRoundedRect(size_t primitiveId, Rect rect, float, float, Color top, Color, Color, int) override
    {
        touchedIds.push_back(primitiveId);
        rects.push_back({primitiveId, top});
        rectsDrawn[primitiveId]=rect;
    }

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex, uint8_t edgeMask, float) override
    {
        drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
        edges[primitiveId]=edgeMask;
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
    //! The edge mask each slot was last drawn with through the overload
    //! taking one, and the rect it was last drawn at (kept across clear()).
    std::map<size_t, uint8_t> edges;
    std::map<size_t, Rect> rectsDrawn;
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

TEST_CASE("DropDownButton: Space, Return, Alt+Down and F4 open it with the first item highlighted, and Escape closes it with the focus back")
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

namespace
{

//! A SplitButton "Paste" at (x, y) with Paste / Paste special / Paste as text.
controls::SplitButton *makeSplit(Scene &scene, int x, int y)
{
    controls::SplitButton *split=scene.canvas->newChild<controls::SplitButton>();

    split->setText("Paste");
    split->setPos(x, y);
    split->getMenu()->addItem("Paste");
    split->getMenu()->addItem("Paste special");
    split->getMenu()->addItem("Paste as text");
    scene.frame();
    scene.frame();
    return split;
}

}//namespace

TEST_CASE("SplitButton: the parts in one row, the main part as wide as its content, the arrow part ArrowWidth, a Divider apart")
{
    Scene scene;
    controls::SplitButton *split=makeSplit(scene, 40, 40);
    controls::Button *main=split->getMainPart();
    controls::Button *arrow=split->getArrowPart();

    CHECK(main->getX() == 0);
    CHECK(main->getWidth() > 0);
    CHECK(main->getWidth() >= main->getPreferredSize().width);
    CHECK(arrow->getX() == main->getWidth()+controls::SplitButton::Divider);
    CHECK(arrow->getWidth() == controls::SplitButton::ArrowWidth);
    CHECK(arrow->getX()+arrow->getWidth() == split->getWidth());
    CHECK(main->getHeight() == split->getHeight());
    CHECK(arrow->getHeight() == split->getHeight());
    CHECK(split->getHeight() > 0);
}

TEST_CASE("SplitButton: the main part fires without opening, the arrow part opens without firing")
{
    Scene scene;
    controls::SplitButton *split=makeSplit(scene, 40, 40);
    Counter pressed;

    split->onPress.add(&pressed, &Counter::count);

    scene.click(split->getMainPart());
    CHECK(pressed.calls == 1);
    CHECK_FALSE(split->isMenuOpen());
    // the control took the focus, not the part
    CHECK(split->isFocussed());

    scene.click(split->getArrowPart());
    REQUIRE(split->isMenuOpen());
    CHECK(pressed.calls == 1);
    scene.frame();
    controls::Menu *menu=split->getMenu();
    // below the whole control, its left edge with the main part's
    CHECK(menu->getY() >= split->getY()+split->getHeight());
    CHECK(menu->getX() == split->getX());
    CHECK(menu->getAnchor() == split);
    CHECK(menu->isFocussed());
    CHECK(split->getArrowPart()->isDepressed());
    CHECK_FALSE(split->getMainPart()->isDepressed());

    scene.key(keys::Escape);
    CHECK_FALSE(split->isMenuOpen());
    CHECK(split->isFocussed());
    CHECK(pressed.calls == 1);

    // a second click on the arrow closes it again
    scene.click(split->getArrowPart());
    REQUIRE(split->isMenuOpen());
    scene.click(split->getArrowPart());
    CHECK_FALSE(split->isMenuOpen());
    CHECK(pressed.calls == 1);
}

TEST_CASE("SplitButton: focused as one, Space and Return press the main part, Alt+Down and F4 open the menu")
{
    Scene scene;
    controls::SplitButton *split=makeSplit(scene, 40, 40);
    Counter pressed;

    split->onPress.add(&pressed, &Counter::count);
    split->focus();
    REQUIRE(split->isFocussed());

    scene.key(keys::Space);
    CHECK(pressed.calls == 1);
    scene.key(keys::Return);
    CHECK(pressed.calls == 2);
    CHECK_FALSE(split->isMenuOpen());

    scene.key(keys::Down);
    CHECK_FALSE(split->isMenuOpen());

    for(int code : {int(keys::Down), int(keys::F4)})
    {
        CAPTURE(code);
        if(code == keys::Down)
            scene.altKey(code);
        else
            scene.key(code);
        REQUIRE(split->isMenuOpen());
        CHECK(split->getMenu()->isFocussed());
        CHECK(split->getMenu()->getHighlightedItem() != nullptr);
        CHECK(pressed.calls == 2);

        scene.key(keys::Escape);
        CHECK_FALSE(split->isMenuOpen());
        CHECK(split->isFocussed());
    }
}

TEST_CASE("SplitButton: the toggle variant flips on the main part's press, never on the arrow's")
{
    Scene scene;
    controls::SplitButton *split=makeSplit(scene, 40, 40);
    Counter toggled;
    Counter pressed;

    split->setIsToggle(true);
    split->onToggle.add(&toggled, &Counter::count);
    split->onPress.add(&pressed, &Counter::count);
    REQUIRE_FALSE(split->getToggleState());

    scene.click(split->getMainPart());
    CHECK(split->getToggleState());
    CHECK(toggled.calls == 1);
    CHECK(pressed.calls == 1);

    scene.click(split->getArrowPart());
    CHECK(split->isMenuOpen());
    CHECK(split->getToggleState());
    CHECK(toggled.calls == 1);
    scene.key(keys::Escape);

    scene.key(keys::Space);
    CHECK_FALSE(split->getToggleState());
    CHECK(toggled.calls == 2);
}

TEST_CASE("SplitButton: disabled, neither part nor a key does anything")
{
    Scene scene;
    controls::SplitButton *split=makeSplit(scene, 40, 40);
    Counter pressed;

    split->onPress.add(&pressed, &Counter::count);
    split->setIsToggle(true);
    split->focus();
    split->setDisabled(true);
    scene.frame();

    scene.click(split->getMainPart());
    scene.click(split->getArrowPart());
    split->focus();
    for(int code : {int(keys::Space), int(keys::Return), int(keys::F4)})
        scene.key(code);
    scene.altKey(keys::Down);

    CHECK(pressed.calls == 0);
    CHECK_FALSE(split->getToggleState());
    CHECK_FALSE(split->isMenuOpen());
    CHECK(split->getMainPart()->isDisabled());
    CHECK(split->getArrowPart()->isDisabled());
}

TEST_CASE("SplitButton: the parts joined by the edge mask and the divider, a hover of one part writing its slots alone, the focus in both")
{
    Scene scene;
    controls::SplitButton *split=makeSplit(scene, 40, 40);
    controls::Button *main=split->getMainPart();
    controls::Button *arrow=split->getArrowPart();

    REQUIRE(Scene::ids(main).size() == 1);
    REQUIRE(Scene::ids(arrow).size() == 2);
    REQUIRE(Scene::ids(split).size() == 1);
    const size_t mainSlot=Scene::ids(main)[0];
    const size_t arrowSlot=Scene::ids(arrow)[0];
    const size_t divider=Scene::ids(split)[0];

    CHECK(scene.renderer.edges[mainSlot] == (renderer::SquareTopRight|renderer::SquareBottomRight
        |renderer::NoBorderRight));
    CHECK(scene.renderer.edges[arrowSlot] == (renderer::SquareTopLeft|renderer::SquareBottomLeft
        |renderer::NoBorderLeft));
    // the divider: Divider units wide, between the parts, the full height
    REQUIRE(scene.renderer.rectsDrawn.count(divider) == 1);
    const Rect column=scene.renderer.rectsDrawn[divider];
    CHECK(column.w == controls::SplitButton::Divider);
    CHECK(column.h == split->getHeight());
    // the parts' rects are in the control's units, the divider's in its parent's
    CHECK(column.x-split->getX()+column.w == scene.renderer.rectsDrawn[arrowSlot].x);
    CHECK(column.x-split->getX() == scene.renderer.rectsDrawn[mainSlot].x+scene.renderer.rectsDrawn[mainSlot].w);
    CHECK(column.y == split->getY());

    // the chevron at z+1
    split->getArrowPart()->redraw();
    scene.frame();
    const SlotRecorder::Icon *icon=scene.renderer.lastIcon(Scene::ids(arrow)[1]);
    REQUIRE(icon != nullptr);
    CHECK(icon->kind == 1);
    CHECK(icon->direction == 0);
    CHECK(icon->zIndex == arrow->getZIndex()+1);

    // a hover of the main part writes its slot alone, then the arrow's its own
    const Point at=main->localPosToCanvas();
    scene.canvas->inputMouseMoved(at.x+2, at.y+main->getHeight()/2, 0, 0);
    scene.frame();
    REQUIRE(HoveredControl == main);
    CHECK((scene.renderer.lastTop(mainSlot) == skin::textured::tokyonight::ButtonHover.top));
    for(size_t id : scene.renderer.touchedIds)
        CHECK(id == mainSlot);

    const Point arrowAt=arrow->localPosToCanvas();
    scene.canvas->inputMouseMoved(arrowAt.x+arrow->getWidth()/2, arrowAt.y+arrow->getHeight()/2, 0, 0);
    scene.frame();
    REQUIRE(HoveredControl == arrow);
    CHECK((scene.renderer.lastTop(arrowSlot) == skin::textured::tokyonight::ButtonHover.top));
    for(size_t id : scene.renderer.touchedIds)
        CHECK(((id == mainSlot) || (id == Scene::ids(arrow)[0]) || (id == Scene::ids(arrow)[1])));

    // focused: both parts draw ButtonFocus (the mouse off them)
    scene.canvas->inputMouseMoved(400, 400, 0, 0);
    scene.frame();
    split->focus();
    scene.frame();
    CHECK((scene.renderer.lastTop(mainSlot) == skin::textured::tokyonight::ButtonFocus.top));
    CHECK((scene.renderer.lastTop(arrowSlot) == skin::textured::tokyonight::ButtonFocus.top));
}

TEST_CASE("SplitButton: setMainItem makes a menu item the main part's action and text")
{
    Scene scene;
    controls::SplitButton *split=makeSplit(scene, 40, 40);
    controls::MenuItem *special=split->getMenu()->addItem("Paste special");
    Counter selected;
    Counter pressed;

    special->onMenuItemSelected.add(&selected, &Counter::count);
    split->onPress.add(&pressed, &Counter::count);
    split->setMainItem(special);
    CHECK(split->getMainItem() == special);
    CHECK(split->getText() == "Paste special");

    scene.frame();
    scene.click(split->getMainPart());
    CHECK(selected.calls == 1);
    CHECK(pressed.calls == 1);
    CHECK_FALSE(split->isMenuOpen());

    split->setMainItem(nullptr);
    scene.click(split->getMainPart());
    CHECK(selected.calls == 1);
    CHECK(pressed.calls == 2);
}

TEST_CASE("DropDownButton: disabling it closes its open menu")
{
    Scene scene;
    controls::DropDownButton *button=scene.dropDown(40, 40);

    scene.frame();
    button->focus();
    scene.key(keys::F4);
    REQUIRE(button->isMenuOpen());

    button->setDisabled(true);
    scene.frame();
    CHECK_FALSE(button->isMenuOpen());
    CHECK(button->getMenu()->hidden());
}

TEST_CASE("DropDownButton: a press on the open button let go off it, then a key closes the menu")
{
    for(int code : {int(keys::Escape), int(keys::Space), int(keys::Return), int(keys::Down), int(keys::F4)})
    {
        CAPTURE(code);
        Scene scene;
        controls::DropDownButton *button=scene.dropDown(40, 40);

        scene.frame();
        scene.click(button);
        REQUIRE(button->isMenuOpen());

        // pressed on the button, released off it: no press, the menu open
        const Point at=button->localPosToCanvas();
        scene.clock+=1.0;
        scene.canvas->inputMouseMoved(at.x+10, at.y+10, 0, 0);
        input::onMouseClicked(scene.canvas, 0, true);
        scene.canvas->inputMouseMoved(400, 400, 0, 0);
        input::onMouseClicked(scene.canvas, 0, false);
        scene.frame();
        REQUIRE(button->isMenuOpen());
        REQUIRE(button->isFocussed());

        if(code == keys::Down)
            scene.altKey(code);
        else
            scene.key(code);
        CHECK_FALSE(button->isMenuOpen());
        CHECK(button->isFocussed());
    }
}

namespace
{

//! Opens a message dialog from a menu item's handler, as "Paste special…".
struct DialogOpener: public event::Handler
{
    controls::Canvas *canvas=nullptr;
    int opened=0;

    void open(event::Info)
    {
        ++opened;
        dialogs::message(canvas, "Paste special", "Nothing to paste.");
    }
};

}//namespace

TEST_CASE("DropDownButton: an item opening a dialog, chosen by Return or a click, leaves the focus on the button once the dialog closes")
{
    for(bool byClick : {false, true})
    {
        CAPTURE(byClick);
        Scene scene;
        controls::DropDownButton *button=scene.dropDown(40, 40);
        controls::MenuItem *item=Scene::firstItem(button);
        DialogOpener opener;

        opener.canvas=scene.canvas;
        item->onMenuItemSelected.add(&opener, &DialogOpener::open);
        scene.frame();

        if(byClick)
        {
            scene.click(button);
            REQUIRE(button->isMenuOpen());
            scene.frame();
            scene.click(item);
        }
        else
        {
            button->focus();
            scene.key(keys::Space);
            REQUIRE(button->getMenu()->getHighlightedItem() == item);
            scene.key(keys::Return);
        }
        REQUIRE(opener.opened == 1);
        CHECK_FALSE(button->isMenuOpen());
        REQUIRE(scene.canvas->modalCount() == 1);

        scene.key(keys::Escape);
        scene.frame();
        CHECK(scene.canvas->modalCount() == 0);
        CHECK((KeyboardFocus == button));
    }
}

TEST_CASE("DropDownButton: a dialog opened by an accelerator or directly while its menu holds the focus gives the focus back to the button")
{
    // byAccelerator: a canvas Ctrl+O, typed with the menu focused; else the
    // dialog opened straight away, as a canvas timer or a post would
    for(bool byAccelerator : {false, true})
    {
        for(bool byClick : {false, true})
        {
            CAPTURE(byAccelerator);
            CAPTURE(byClick);
            Scene scene;
            controls::DropDownButton *button=scene.dropDown(40, 40);
            DialogOpener opener;

            opener.canvas=scene.canvas;
            scene.canvas->addAccelerator("CTRL+O", &DialogOpener::open, &opener);
            scene.frame();

            if(byClick)
                scene.click(button);
            else
            {
                button->focus();
                scene.key(keys::Space);
            }
            REQUIRE(button->isMenuOpen());
            REQUIRE((KeyboardFocus == button->getMenu()));

            if(byAccelerator)
            {
                scene.canvas->inputModifierKey(keys::Control, true);
                scene.canvas->inputCharacter('o');
                scene.canvas->inputModifierKey(keys::Control, false);
            }
            else
                opener.open(event::Info(nullptr));
            scene.frame();
            REQUIRE(opener.opened == 1);
            CHECK_FALSE(button->isMenuOpen());
            REQUIRE(scene.canvas->modalCount() == 1);

            scene.key(keys::Escape);
            scene.frame();
            CHECK(scene.canvas->modalCount() == 0);
            CHECK((KeyboardFocus == button));
        }
    }
}

TEST_CASE("DropDownButton: deleting it deletes a menu that deletes itself on close, off the canvas")
{
    Scene scene;
    controls::DropDownButton *button=scene.dropDown(40, 40);
    controls::Menu *menu=button->getMenu();
    const size_t menuId=menu->getControlId();

    scene.frame();
    menu->setDeleteOnClose(true);
    // off any canvas: close() deletes it at once
    menu->setParent(nullptr);
    REQUIRE((menu->getCanvas() == nullptr));

    delete button;
    scene.frame();
    CHECK(controls::Base::getControlById(menuId) == nullptr);
}

TEST_CASE("SplitButton: the divider takes the parts' border colour while focused or disabled")
{
    Scene scene;
    controls::SplitButton *split=makeSplit(scene, 40, 40);
    const size_t divider=Scene::ids(split)[0];

    namespace tn=skin::textured::tokyonight;

    split->redraw();
    scene.frame();
    CHECK((scene.renderer.lastTop(divider) == tn::SplitButtonDivider.top));

    split->focus();
    scene.frame();
    CHECK((scene.renderer.lastTop(divider) == tn::ButtonFocus.border));

    split->unfocus();
    scene.frame();
    CHECK((scene.renderer.lastTop(divider) == tn::SplitButtonDivider.top));

    split->setDisabled(true);
    scene.frame();
    CHECK((scene.renderer.lastTop(divider) == tn::ButtonDisabled.border));

    split->setDisabled(false);
    scene.frame();
    CHECK((scene.renderer.lastTop(divider) == tn::SplitButtonDivider.top));

    // a disabled toggle the app turns on and off: the divider follows the
    // disabled main part's border, ButtonOnDisabled's while it is on
    split->setIsToggle(true);
    split->setDisabled(true);
    scene.frame();
    split->setToggleState(true);
    scene.frame();
    CHECK((scene.renderer.lastTop(divider) == tn::ButtonOnDisabled.border));
    split->setToggleState(false);
    scene.frame();
    CHECK((scene.renderer.lastTop(divider) == tn::ButtonDisabled.border));
}

TEST_CASE("SplitButton: a press on the open arrow let go off it, then Escape, Alt+Down or F4 closes the menu")
{
    for(int code : {int(keys::Escape), int(keys::Down), int(keys::F4)})
    {
        CAPTURE(code);
        Scene scene;
        controls::SplitButton *split=makeSplit(scene, 40, 40);
        controls::Button *arrow=split->getArrowPart();

        scene.click(arrow);
        REQUIRE(split->isMenuOpen());

        const Point at=arrow->localPosToCanvas();
        scene.clock+=1.0;
        scene.canvas->inputMouseMoved(at.x+arrow->getWidth()/2, at.y+arrow->getHeight()/2, 0, 0);
        input::onMouseClicked(scene.canvas, 0, true);
        scene.canvas->inputMouseMoved(400, 400, 0, 0);
        input::onMouseClicked(scene.canvas, 0, false);
        scene.frame();
        REQUIRE(split->isMenuOpen());
        REQUIRE(split->isFocussed());

        if(code == keys::Down)
            scene.altKey(code);
        else
            scene.key(code);
        CHECK_FALSE(split->isMenuOpen());
        CHECK(split->isFocussed());
    }
}

namespace
{

//! Adds Recent, a submenu holding Leaf, to \p button's menu.
struct Submenu
{
    controls::MenuItem *recent=nullptr;
    controls::MenuItem *leaf=nullptr;

    explicit Submenu(controls::DropDownButton *button)
    {
        recent=button->getMenu()->addItem("Recent");
        leaf=recent->getMenu()->addItem("Leaf");
    }
};

//! The mouse onto \p item, two frames for its submenu to open and lay out.
void hover(Scene &scene, controls::Base *item)
{
    const Point at=item->localPosToCanvas();

    scene.canvas->inputMouseMoved(at.x+item->getWidth()/2, at.y+item->getHeight()/2, 0, 0);
    scene.frame();
    scene.frame();
}

}//namespace

TEST_CASE("DropDownButton: a submenu the mouse opens, its leaf clicked, gives the focus back to the button")
{
    for(bool byKey : {false, true})
    {
        CAPTURE(byKey);
        Scene scene;
        controls::DropDownButton *button=scene.dropDown(40, 40);
        Submenu sub(button);

        scene.frame();
        if(byKey)
        {
            button->focus();
            scene.key(keys::Space);
        }
        else
            scene.click(button);
        REQUIRE(button->isMenuOpen());
        scene.frame();

        hover(scene, sub.recent);
        REQUIRE(sub.recent->isMenuOpen());

        scene.click(sub.leaf);
        CHECK_FALSE(button->isMenuOpen());
        CHECK_FALSE(sub.recent->isMenuOpen());
        CHECK((KeyboardFocus == button));
    }
}

TEST_CASE("DropDownButton: a press on a hover-opened submenu's leaf let go off it, then Escape closes both menus")
{
    Scene scene;
    controls::DropDownButton *button=scene.dropDown(40, 40);
    Submenu sub(button);

    scene.frame();
    scene.click(button);
    scene.frame();
    hover(scene, sub.recent);
    REQUIRE(sub.recent->isMenuOpen());

    const Point at=sub.leaf->localPosToCanvas();
    scene.clock+=1.0;
    scene.canvas->inputMouseMoved(at.x+5, at.y+5, 0, 0);
    input::onMouseClicked(scene.canvas, 0, true);
    scene.canvas->inputMouseMoved(600, 400, 0, 0);
    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();
    REQUIRE(sub.recent->isMenuOpen());
    CHECK((KeyboardFocus == sub.recent->getMenu()));

    scene.key(keys::Escape);
    CHECK_FALSE(sub.recent->isMenuOpen());
    CHECK(button->isMenuOpen());
    scene.key(keys::Escape);
    CHECK_FALSE(button->isMenuOpen());
    CHECK((KeyboardFocus == button));
}

TEST_CASE("DropDownButton and SplitButton: disabled, a click focuses neither, and one disabled with its menu open keeps no focus")
{
    Scene scene;
    controls::DropDownButton *button=scene.dropDown(40, 40);
    controls::SplitButton *split=makeSplit(scene, 40, 120);
    controls::DropDownButton *other=scene.dropDown(300, 40, "Other");

    scene.frame();
    button->setDisabled(true);
    split->setDisabled(true);
    other->focus();
    scene.frame();

    scene.click(button);
    CHECK_FALSE(button->isFocussed());
    CHECK_FALSE(button->isTabable());
    scene.click(split->getMainPart());
    CHECK_FALSE(split->isFocussed());
    scene.click(split->getArrowPart());
    CHECK_FALSE(split->isFocussed());
    CHECK_FALSE(split->isTabable());

    // enabled again, they take the focus and Tab, the arrow part neither
    button->setDisabled(false);
    split->setDisabled(false);
    CHECK(button->isTabable());
    CHECK(split->isTabable());
    CHECK_FALSE(split->getArrowPart()->isTabable());
    CHECK_FALSE(split->getArrowPart()->getKeyboardInputEnabled());
    scene.click(split->getMainPart());
    CHECK(split->isFocussed());

    // disabled while its menu holds the focus: the hand-back skips it
    for(controls::Base *control : {static_cast<controls::Base *>(other), static_cast<controls::Base *>(split)})
    {
        control->focus();
        scene.key(keys::F4);
        REQUIRE((KeyboardFocus != control));
        // at once, not only once the next frame drops a focus taking no keys
        control->setDisabled(true);
        CHECK_FALSE(control->isFocussed());
        scene.frame();
        CHECK_FALSE(control->isFocussed());
        control->setDisabled(false);
    }

    // disabled while it holds the focus itself, no menu open: dropped at once
    for(controls::Base *control : {static_cast<controls::Base *>(other), static_cast<controls::Base *>(split)})
    {
        control->focus();
        REQUIRE(control->isFocussed());
        control->setDisabled(true);
        CHECK_FALSE(control->isFocussed());
    }
}

TEST_CASE("SplitButton: a press on the open arrow let go off it, then Space or Return closes the menu and presses the main part")
{
    for(int code : {int(keys::Space), int(keys::Return)})
    {
        CAPTURE(code);
        Scene scene;
        controls::SplitButton *split=makeSplit(scene, 40, 40);
        controls::Button *arrow=split->getArrowPart();
        Counter pressed;

        split->onPress.add(&pressed, &Counter::count);
        scene.click(arrow);
        REQUIRE(split->isMenuOpen());

        const Point at=arrow->localPosToCanvas();
        scene.clock+=1.0;
        scene.canvas->inputMouseMoved(at.x+arrow->getWidth()/2, at.y+arrow->getHeight()/2, 0, 0);
        input::onMouseClicked(scene.canvas, 0, true);
        scene.canvas->inputMouseMoved(400, 400, 0, 0);
        input::onMouseClicked(scene.canvas, 0, false);
        scene.frame();
        REQUIRE(split->isMenuOpen());
        REQUIRE(split->isFocussed());

        scene.key(code);
        CHECK_FALSE(split->isMenuOpen());
        CHECK(pressed.calls == 1);
        CHECK(split->isFocussed());
    }
}
