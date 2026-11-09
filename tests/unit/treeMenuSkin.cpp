/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Two textured renderers #185 found unreachable (#219), through the real
 *  TexturedSkin lookup:
 *  - The panel behind a tree was registered as "Tree", a name no control type
 *    has, so a TreeControl fell through TreeNode to the Base renderer and drew
 *    nothing. It is "TreeControl" now: a TreeControl and a PropertyTree (which
 *    derives from it) get skin::textured::Tree, which paints PanelDark into its
 *    one slot.
 *  - A MenuItem's submenu chevron (RightArrow) had only an empty placeholder.
 *    It gets its own renderer, one style 5 chevron (kind 1, direction 3:
 *    right) inside its bounds, in the item's text colour, at its own z, which is
 *    after the item's surface. Hovering the item changes that colour, and
 *    MenuItem::updateColors repaints the chevron in it in the next frame; a
 *    settled frame draws nothing. Hidden, the chevron's slot is hidden.
 *  A frame in which only one of them is dirty touches no slot but its own.
 *
 *  A MenuItem's renderer holds two slots (#464): [0] its highlight, [1] the
 *  check of a checkable, checked item, a style 5 checkmark (kind 0) at the
 *  item's z+1, in its text colour, centred in the 24-unit icon column, with the
 *  item's first child numbered after it (depthLayers() 2). An unchecked item, a
 *  checked one that is not checkable, one on a MenuStrip itself and one in a
 *  menu with the icon margin disabled have slot [1] hidden; so does a hidden
 *  menu's. A checked item's image is hidden, the check in its place. A toggle
 *  or a change of checkable writes the item's slots alone, the check follows
 *  the hover and disabled colours, and a settled frame writes nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/menuStrip.h>
#include <gweni/controls/propertyTree.h>
#include <gweni/controls/treeControl.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

using namespace gweni;

namespace
{

//! Records the slots each frame draws into, shows or hides, and the icons.
class SlotRecorder: public test::StubRenderer
{
public:
    struct Icon
    {
        size_t id;
        Rect rect;
        int kind;
        int direction;
        Color color;
        int zIndex;
    };

    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color top, Color, Color, int) override
    {
        drawn.push_back(primitiveId);
        drawnTop.push_back(top);
    }

    void drawFilledRect(size_t primitiveId, Rect, int) override
    {
        drawn.push_back(primitiveId);
        drawnTop.push_back(Color());
    }

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color, int zIndex) override
    {
        icons.push_back({primitiveId, rect, kind, direction, color, zIndex});
    }

    void showPrimitive(int, size_t primitiveId) override
    {
        shown.push_back(primitiveId);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.push_back(primitiveId);
    }

    bool drewInto(size_t id, const Color &top) const
    {
        for(size_t i=0; i < drawn.size(); ++i)
        {
            if(drawn[i] == id && drawnTop[i] == top)
                return true;
        }
        return false;
    }

    //! The last icon drawn into the slot, or null.
    const Icon *lastIcon(size_t id) const
    {
        for(auto it=icons.rbegin(); it != icons.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    bool hid(size_t id) const
    {
        return std::find(hidden.begin(), hidden.end(), id) != hidden.end();
    }

    //! Every slot drawn into, shown or hidden since the last clear().
    std::vector<size_t> touched() const
    {
        std::vector<size_t> ids(drawn);

        for(const Icon &icon : icons)
            ids.push_back(icon.id);
        ids.insert(ids.end(), shown.begin(), shown.end());
        ids.insert(ids.end(), hidden.begin(), hidden.end());
        return ids;
    }

    void clear()
    {
        drawn.clear();
        drawnTop.clear();
        icons.clear();
        shown.clear();
        hidden.clear();
    }

    std::vector<size_t> drawn;
    std::vector<Color> drawnTop;
    std::vector<Icon> icons;
    std::vector<size_t> shown;
    std::vector<size_t> hidden;
};

//! A real Canvas drawn by the (un-init()'d) textured skin on a SlotRecorder.
//! Creating a renderer only looks up texture entry ids, which an unloaded skin
//! answers with "none", and the surfaces these renderers draw use no atlas.
struct Scene
{
    SlotRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;

    Scene()
    {
        // distinct, so a hover's colour change shows in the chevron
        skin.Colors.Button.Normal=Color(200, 200, 200, 255);
        skin.Colors.Button.Hover=Color(255, 255, 255, 255);
        skin.Colors.Button.Down=Color(100, 100, 255, 255);
        skin.Colors.Button.Disabled=Color(90, 90, 90, 255);

        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
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
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
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

    //! True if every slot touched since the last frame() is one of the controls'.
    bool touchedOnly(std::initializer_list<const controls::Base *> controls) const
    {
        std::vector<size_t> own;

        for(const controls::Base *control : controls)
            own.insert(own.end(), ids(control).begin(), ids(control).end());
        for(size_t id : renderer.touched())
        {
            if(std::find(own.begin(), own.end(), id) == own.end())
                return false;
        }
        return true;
    }
};

//! A menu on the canvas whose second item has a submenu, so it shows a chevron.
struct MenuScene: Scene
{
    controls::Menu *menu=nullptr;
    controls::MenuItem *plain=nullptr;
    controls::MenuItem *parent=nullptr;
    controls::Base *arrow=nullptr;

    MenuScene()
    {
        menu=canvas->newChild<controls::Menu>();
        menu->setPos(20, 20);
        plain=menu->addItem("Plain");
        parent=menu->addItem("Parent");
        parent->getMenu()->addItem("Child");
        menu->setHidden(false);

        for(controls::Base *child : parent->getChildren())
        {
            if(child->getTypeName() == std::string("RightArrow"))
                arrow=child;
        }
    }
};

}//namespace

TEST_CASE("the textured skin gives a TreeControl and a PropertyTree the Tree renderer, which paints PanelDark")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        Scene scene;
        controls::TreeControl *tree=scene.canvas->newChild<controls::TreeControl>();
        tree->setBounds(10, 10, 200, 150);
        tree->addNode("One")->addNode("Inside");
        controls::PropertyTree *properties=scene.canvas->newChild<controls::PropertyTree>();
        properties->setBounds(250, 10, 200, 150);

        for(controls::Base *control : {static_cast<controls::Base *>(tree), static_cast<controls::Base *>(properties)})
        {
            REQUIRE(control->getSkinControl() != nullptr);
            CHECK(dynamic_cast<const skin::textured::Tree *>(control->getSkinControl()) != nullptr);
            CHECK(dynamic_cast<const skin::textured::Base *>(control->getSkinControl()) == nullptr);
        }

        scene.frame();

        for(controls::Base *control : {static_cast<controls::Base *>(tree), static_cast<controls::Base *>(properties)})
        {
            REQUIRE(Scene::ids(control).size() == 1);
            CHECK(scene.renderer.drewInto(Scene::ids(control)[0], skin::textured::tokyonight::PanelDark.top));
        }

        // settled: nothing drawn
        scene.frame();
        CHECK(scene.renderer.touched().empty());

        // only the tree dirty: only its slot is written
        tree->redraw();
        scene.frame();
        CHECK_FALSE(scene.renderer.touched().empty());
        CHECK(scene.touchedOnly({tree}));
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
}

TEST_CASE("a MenuItem's submenu chevron gets its own renderer: a right chevron in the item's text colour")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        MenuScene scene;
        REQUIRE(scene.arrow != nullptr);
        REQUIRE(scene.arrow->getSkinControl() != nullptr);
        CHECK(dynamic_cast<const skin::textured::RightArrow *>(scene.arrow->getSkinControl()) != nullptr);

        // only the item with a submenu has one
        for(controls::Base *child : scene.plain->getChildren())
            CHECK(child->getTypeName() != std::string("RightArrow"));

        scene.frame();

        REQUIRE(Scene::ids(scene.arrow).size() == 1);
        const size_t slot=Scene::ids(scene.arrow)[0];
        const SlotRecorder::Icon *icon=scene.renderer.lastIcon(slot);
        REQUIRE(icon != nullptr);
        CHECK(icon->kind == 1);
        CHECK(icon->direction == 3);
        CHECK((icon->color == scene.parent->getTextColor()));
        CHECK((icon->color == scene.skin.Colors.Button.Normal));
        // in front of the item's surface, inside the chevron's own bounds
        CHECK(icon->zIndex == scene.arrow->getZIndex());
        CHECK(icon->zIndex > scene.parent->getZIndex());
        CHECK(icon->rect.w > 0);
        CHECK(icon->rect.h > 0);
        // (a render bounds is in the parent's coordinates)
        CHECK(icon->rect.x >= scene.arrow->getX());
        CHECK(icon->rect.y >= scene.arrow->getY());
        CHECK(icon->rect.x+icon->rect.w <= scene.arrow->getX()+scene.arrow->getWidth());
        CHECK(icon->rect.y+icon->rect.h <= scene.arrow->getY()+scene.arrow->getHeight());
        // the chevron sits at the item's right
        CHECK(scene.arrow->getX()+scene.arrow->getWidth() > scene.parent->getWidth()/2);

        // settled: nothing drawn
        scene.frame();
        CHECK(scene.renderer.touched().empty());

        // only the chevron dirty: only its slot is written
        scene.arrow->redraw();
        scene.frame();
        CHECK_FALSE(scene.renderer.touched().empty());
        CHECK(scene.touchedOnly({scene.arrow}));
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
}

TEST_CASE("hovering a MenuItem with a submenu repaints its chevron in the hover colour in the next frame")
{
    MenuScene scene;
    REQUIRE(scene.arrow != nullptr);
    scene.frame();
    const size_t slot=Scene::ids(scene.arrow)[0];

    scene.moveOnto(scene.parent);
    REQUIRE(scene.parent->isHovered());
    const SlotRecorder::Icon *icon=scene.renderer.lastIcon(slot);
    REQUIRE(icon != nullptr);
    CHECK((icon->color == scene.skin.Colors.Button.Hover));
    // (hovering it also opens its submenu, which draws) the other row and the
    // menu's backdrop are left alone
    for(size_t id : scene.renderer.touched())
    {
        CHECK(std::count(Scene::ids(scene.plain).begin(), Scene::ids(scene.plain).end(), id) == 0);
        CHECK(std::count(Scene::ids(scene.menu).begin(), Scene::ids(scene.menu).end(), id) == 0);
    }

    // settled while hovered: nothing drawn
    scene.frame();
    CHECK(scene.renderer.touched().empty());

    // off the item: back to the normal colour
    scene.moveOnto(scene.plain);
    REQUIRE_FALSE(scene.parent->isHovered());
    icon=scene.renderer.lastIcon(slot);
    REQUIRE(icon != nullptr);
    CHECK((icon->color == scene.skin.Colors.Button.Normal));

    // leaving the item without a submenu leaves the chevron alone
    scene.canvas->inputMouseMoved(600, 400, 0, 0);
    scene.frame();
    REQUIRE_FALSE(scene.plain->isHovered());
    CHECK(scene.renderer.lastIcon(slot) == nullptr);
}

TEST_CASE("a hidden menu hides its chevron's slot")
{
    MenuScene scene;
    REQUIRE(scene.arrow != nullptr);
    scene.frame();
    const size_t slot=Scene::ids(scene.arrow)[0];

    scene.menu->setHidden(true);
    scene.frame();
    CHECK(scene.renderer.hid(slot));
    CHECK(scene.renderer.lastIcon(slot) == nullptr);

    scene.menu->setHidden(false);
    scene.frame();
    CHECK(scene.renderer.lastIcon(slot) != nullptr);
}

namespace
{

//! A menu of four rows: not checkable, checkable and unchecked, checkable and
//! checked, and checked but not checkable.
struct CheckScene: Scene
{
    controls::Menu *menu=nullptr;
    controls::MenuItem *plain=nullptr;
    controls::MenuItem *unchecked=nullptr;
    controls::MenuItem *checked=nullptr;
    controls::MenuItem *notCheckable=nullptr;

    CheckScene()
    {
        menu=canvas->newChild<controls::Menu>();
        menu->setPos(20, 20);
        plain=menu->addItem("Plain");
        unchecked=menu->addItem("Unchecked");
        unchecked->setCheckable(true);
        checked=menu->addItem("Checked");
        checked->setCheckable(true);
        checked->setChecked(true);
        notCheckable=menu->addItem("Not checkable");
        notCheckable->setChecked(true);
        menu->setHidden(false);
    }

    static size_t checkSlot(const controls::Base *item)
    {
        return ids(item)[1];
    }
};

//! The check drawn for \p item is a kind-0 icon at its z+1, centred in its
//! left text padding (the icon column) and vertically, inside its bounds.
void checkCheckIcon(const SlotRecorder::Icon *icon, controls::MenuItem *item)
{
    REQUIRE(icon != nullptr);
    CHECK(icon->kind == 0);
    CHECK(icon->direction == 0);
    CHECK(icon->zIndex == item->getZIndex()+1);

    // (a render bounds is in the parent's coordinates)
    const int column=item->getTextPadding().left;
    REQUIRE(column > 0);
    CHECK(icon->rect.w > 0);
    CHECK(icon->rect.w == icon->rect.h);
    CHECK(icon->rect.x >= item->getX());
    CHECK(icon->rect.x+icon->rect.w <= item->getX()+column);
    CHECK(std::abs(2*(icon->rect.x-item->getX())+icon->rect.w-column) <= 1);
    CHECK(icon->rect.y >= item->getY());
    CHECK(icon->rect.y+icon->rect.h <= item->getY()+item->getHeight());
    CHECK(std::abs(2*(icon->rect.y-item->getY())+icon->rect.h-item->getHeight()) <= 1);
}

}//namespace

TEST_CASE("a checkable, checked MenuItem draws a check in its second slot, in the icon column, at z+1")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        CheckScene scene;
        scene.frame();

        for(controls::MenuItem *item : {scene.plain, scene.unchecked, scene.checked, scene.notCheckable})
        {
            REQUIRE(item->getSkinControl() != nullptr);
            CHECK(dynamic_cast<const skin::textured::MenuItem *>(item->getSkinControl()) != nullptr);
            REQUIRE(Scene::ids(item).size() == 2);
            CHECK(const_cast<skin::Control *>(item->getSkinControl())->depthLayers() == 2);
            // the item's own children are numbered after the check
            for(controls::Base *child : item->getChildren())
                CHECK(child->getZIndex() >= item->getZIndex()+2);
        }

        const SlotRecorder::Icon *icon=scene.renderer.lastIcon(CheckScene::checkSlot(scene.checked));
        checkCheckIcon(icon, scene.checked);
        CHECK((icon->color == scene.checked->getTextColor()));
        CHECK((icon->color == scene.skin.Colors.Button.Normal));

        for(controls::MenuItem *item : {scene.plain, scene.unchecked, scene.notCheckable})
        {
            CHECK(scene.renderer.hid(CheckScene::checkSlot(item)));
            CHECK(scene.renderer.lastIcon(CheckScene::checkSlot(item)) == nullptr);
        }

        // settled: nothing drawn
        scene.frame();
        CHECK(scene.renderer.touched().empty());
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
}

TEST_CASE("toggling a MenuItem's check or checkable writes that item's slots alone")
{
    CheckScene scene;
    scene.frame();

    // unchecked
    scene.checked->setChecked(false);
    scene.frame();
    CHECK(scene.renderer.hid(CheckScene::checkSlot(scene.checked)));
    CHECK(scene.renderer.lastIcon(CheckScene::checkSlot(scene.checked)) == nullptr);
    CHECK(scene.touchedOnly({scene.checked}));

    scene.frame();
    CHECK(scene.renderer.touched().empty());

    // checked again
    scene.checked->setChecked(true);
    scene.frame();
    checkCheckIcon(scene.renderer.lastIcon(CheckScene::checkSlot(scene.checked)), scene.checked);
    CHECK(scene.touchedOnly({scene.checked}));

    // made checkable: the checked item that was not shows its check
    scene.notCheckable->setCheckable(true);
    scene.frame();
    checkCheckIcon(scene.renderer.lastIcon(CheckScene::checkSlot(scene.notCheckable)), scene.notCheckable);
    CHECK(scene.touchedOnly({scene.notCheckable}));

    // and not checkable again: hidden
    scene.notCheckable->setCheckable(false);
    scene.frame();
    CHECK(scene.renderer.hid(CheckScene::checkSlot(scene.notCheckable)));
    CHECK(scene.renderer.lastIcon(CheckScene::checkSlot(scene.notCheckable)) == nullptr);
    CHECK(scene.touchedOnly({scene.notCheckable}));

    // a click on a checkable row toggles it; the menu closes
    scene.unchecked->doAction();
    CHECK(scene.unchecked->isChecked());
    scene.frame();
    scene.menu->setHidden(false);
    scene.frame();
    checkCheckIcon(scene.renderer.lastIcon(CheckScene::checkSlot(scene.unchecked)), scene.unchecked);
}

TEST_CASE("a MenuItem's check follows its hover and disabled text colours")
{
    CheckScene scene;
    scene.frame();
    const size_t slot=CheckScene::checkSlot(scene.checked);

    scene.moveOnto(scene.checked);
    REQUIRE(scene.checked->isHovered());
    const SlotRecorder::Icon *icon=scene.renderer.lastIcon(slot);
    checkCheckIcon(icon, scene.checked);
    CHECK((icon->color == scene.skin.Colors.Button.Hover));
    CHECK((icon->color == scene.checked->getTextColor()));

    scene.moveOnto(scene.plain);
    REQUIRE_FALSE(scene.checked->isHovered());
    icon=scene.renderer.lastIcon(slot);
    REQUIRE(icon != nullptr);
    CHECK((icon->color == scene.skin.Colors.Button.Normal));

    scene.checked->setDisabled(true);
    scene.frame();
    icon=scene.renderer.lastIcon(slot);
    checkCheckIcon(icon, scene.checked);
    CHECK((icon->color == scene.skin.Colors.Button.Disabled));
}

TEST_CASE("a hidden menu hides its items' checks")
{
    CheckScene scene;
    scene.frame();
    const size_t slot=CheckScene::checkSlot(scene.checked);

    scene.menu->setHidden(true);
    scene.frame();
    CHECK(scene.renderer.hid(slot));
    CHECK(scene.renderer.lastIcon(slot) == nullptr);

    scene.menu->setHidden(false);
    scene.frame();
    checkCheckIcon(scene.renderer.lastIcon(slot), scene.checked);
}

TEST_CASE("a checked item's image gives its place to the check")
{
    Scene scene;
    controls::Menu *menu=scene.canvas->newChild<controls::Menu>();
    menu->setPos(20, 20);
    controls::MenuItem *item=menu->addItem("With image", "icon.png");
    item->setCheckable(true);
    item->setChecked(true);
    menu->setHidden(false);
    scene.frame();

    controls::Base *image=nullptr;
    for(controls::Base *child : item->getChildren())
    {
        if(child->getTypeName() == std::string("ImagePanel"))
            image=child;
    }
    REQUIRE(image != nullptr);
    CHECK_FALSE(image->visible());
    checkCheckIcon(scene.renderer.lastIcon(Scene::ids(item)[1]), item);

    item->setChecked(false);
    scene.frame();
    CHECK(image->visible());
    CHECK(scene.renderer.hid(Scene::ids(item)[1]));
}

TEST_CASE("an item on a MenuStrip, or in a menu with no icon margin, has no column and draws no check")
{
    Scene scene;
    controls::MenuStrip *strip=scene.canvas->newChild<controls::MenuStrip>();
    controls::MenuItem *onStrip=strip->addItem("Strip");
    onStrip->setCheckable(true);
    onStrip->setChecked(true);
    REQUIRE(onStrip->onStrip());

    controls::Menu *menu=scene.canvas->newChild<controls::Menu>();
    menu->setPos(20, 60);
    menu->setDisableIconMargin(true);
    controls::MenuItem *noMargin=menu->addItem("No margin");
    noMargin->setCheckable(true);
    noMargin->setChecked(true);
    REQUIRE(noMargin->getTextPadding().left == 0);
    menu->setHidden(false);
    scene.frame();

    for(controls::MenuItem *item : {onStrip, noMargin})
    {
        REQUIRE(Scene::ids(item).size() == 2);
        CHECK(scene.renderer.hid(Scene::ids(item)[1]));
        CHECK(scene.renderer.lastIcon(Scene::ids(item)[1]) == nullptr);
    }

    // and the checked strip item's submenu items draw theirs
    controls::MenuItem *inSubmenu=onStrip->getMenu()->addItem("Inside");
    inSubmenu->setCheckable(true);
    inSubmenu->setChecked(true);
    onStrip->openMenu();
    scene.frame();
    checkCheckIcon(scene.renderer.lastIcon(Scene::ids(inSubmenu)[1]), inSubmenu);
}
