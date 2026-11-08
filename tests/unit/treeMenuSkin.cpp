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
 */
#include "headlessFixture.h"

#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/propertyTree.h>
#include <gweni/controls/treeControl.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
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
