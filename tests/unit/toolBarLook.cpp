/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured ToolBar renderers on a recording StubRenderer. Every toolbar
 *  type has a renderer: the bar paints the ToolBar surface, a tool button its
 *  one slot in ToolButton at rest, ToolButtonHover hovered, ToolButtonDown
 *  pressed and ToolButtonOn as a toggle that is on (disabled too, as a
 *  Button's on look is), with ButtonFocus's border
 *  while focused; the overflow button the same surfaces (Down while its menu
 *  is open) and the down chevron (icon kind 1) at its z+1. A hover on a tool
 *  button and moving off write that button's slot alone. A width change that
 *  moves one item into the menu writes no primitive slot but the moved item's,
 *  the overflow button's and the resized controls' (the container's and the
 *  bar's), and enters no control outside the container's subtree but the
 *  canvas: a sibling container is never laid out.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/separator.h>
#include <gweni/controls/toolBar.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

using namespace gweni;
using controls::ToolBar;
namespace tn=skin::textured::tokyonight;

namespace
{

//! Each slot's last rounded rect and icon, and the slots a frame changed: a
//! draw that repeats the slot's last one (a control rendered again with the
//! same look, as a clip region changed by a resize renders it) changes
//! nothing, as SlotRenderer's per-slot byte diff writes nothing for it.
class Recorder: public test::StubRenderer
{
public:
    using test::StubRenderer::drawRoundedRect;

    struct Draw
    {
        Rect rect;
        Color top, bottom, border;
        int z;
        bool shown=true;

        bool operator==(const Draw &o) const
        {
            return rect == o.rect && same(top, o.top) && same(bottom, o.bottom) && same(border, o.border)
                && z == o.z && shown == o.shown;
        }
        static bool same(const Color &a, const Color &b)
        {
            return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        }
    };

    struct Icon
    {
        Rect rect;
        int kind;
        int z;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex) override
    {
        Draw draw;

        draw.rect=rect;
        draw.top=top;
        draw.bottom=bottom;
        draw.border=borderColor;
        draw.z=zIndex;
        note(primitiveId, draw);
        test::StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom,
            borderColor, zIndex);
    }

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color, int zIndex) override
    {
        Draw draw;

        draw.rect=rect;
        draw.top=color;
        draw.z=zIndex+1000*kind+100000*direction;
        note(primitiveId, draw);
        icons[primitiveId]={rect, kind, zIndex};
        test::StubRenderer::drawIcon(primitiveId, rect, kind, direction, color, zIndex);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        Draw draw=draws[primitiveId];

        draw.shown=false;
        note(primitiveId, draw);
        test::StubRenderer::hidePrimitive(primitiveId);
    }

    void showPrimitive(int style, size_t primitiveId) override
    {
        Draw draw=draws[primitiveId];

        draw.shown=(style != 0);
        note(primitiveId, draw);
        test::StubRenderer::showPrimitive(style, primitiveId);
    }

    std::map<size_t, Draw> draws;
    std::map<size_t, Icon> icons;
    std::set<size_t> touched;

private:
    void note(size_t id, const Draw &draw)
    {
        const auto it=draws.find(id);

        if(it == draws.end() || !(it->second == draw))
            touched.insert(id);
        draws[id]=draw;
    }
};

//! An entry-counting control: a sibling of the toolbar's container.
class Block: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Block, controls::Base)
    {}

    GWENI_DRAWS_NOTHING

    int entries=0;

    void calculateSize(skin::Base *skin, Dim dim) override
    {
        ++entries;
        ParentClass::calculateSize(skin, dim);
    }

    void arrange(skin::Base *skin, Dim dim) override
    {
        ++entries;
        ParentClass::arrange(skin, dim);
    }
};

struct Scene
{
    Recorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Base *box=nullptr;
    Block *sibling=nullptr;
    ToolBar *bar=nullptr;
    std::vector<controls::ToolButton *> buttons;
    float clock=nextClock();

    static float nextClock()
    {
        static float start=0.0f;

        start+=1000.0f;
        return start;
    }

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 400);
        box=canvas->newChild<controls::Base>();
        box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        box->setBounds(0, 0, 700, 60);
        sibling=canvas->newChild<Block>();
        sibling->setBounds(0, 200, 100, 50);
        bar=box->newChild<ToolBar>();
        for(const char *text : {"Open", "Save", "Cut", "Copy"})
            buttons.push_back(bar->addButton(text));
        bar->addSeparator();
        buttons.push_back(bar->addToggle("Bold"));
        settle();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    const controls::LayoutStats &frame()
    {
        renderer.touched.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i < 3; ++i)
            frame();
    }

    void moveTo(controls::Base *over)
    {
        const Point at=over->localPosToCanvas();

        canvas->inputMouseMoved(at.x+over->getWidth()/2, at.y+over->getHeight()/2, 0, 0);
        frame();
    }

    void moveOff()
    {
        canvas->inputMouseMoved(799, 399, 0, 0);
        frame();
    }

    static std::vector<size_t> slotsOf(const controls::Base *control)
    {
        std::vector<size_t> primitives, texts;

        if(control->getSkinControl())
            control->getSkinControl()->appendIds(primitives, texts);
        return primitives;
    }

    //! Narrows the container a unit a frame until no more than \p shown of
    //! the buttons show (the toggle counted), checking that each frame writes
    //! no slot but the container's, the bar's, the overflow button's and
    //! those of the items it hid or showed.
    void narrowTo(size_t shown)
    {
        for(int width=box->getWidth(); width > 0; --width)
        {
            size_t visible=0;

            for(controls::ToolButton *button : buttons)
                visible+=button->hidden()?0:1;
            if(visible <= shown)
                return;

            std::vector<bool> hidden;

            for(controls::Base *item : bar->getItems())
                hidden.push_back(item->hidden());

            box->setBounds(0, 0, width-1, 60);
            CHECK(frame().passes == 1);

            std::set<size_t> allowed;

            for(controls::Base *control : {box, static_cast<controls::Base *>(bar),
                static_cast<controls::Base *>(bar->getOverflowButton())})
            {
                for(size_t id : slotsOf(control))
                    allowed.insert(id);
            }
            for(size_t i=0; i < hidden.size(); ++i)
            {
                if(hidden[i] != bar->getItems()[i]->hidden())
                    for(size_t id : slotsOf(bar->getItems()[i]))
                        allowed.insert(id);
            }
            for(size_t id : renderer.touched)
            {
                INFO("width " << width-1 << ", slot " << id);
                CHECK(allowed.count(id) == 1);
            }
        }
    }
};

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

}//namespace

TEST_CASE("ToolBar look: every toolbar type has a renderer, the bar and its buttons in their surfaces")
{
    Scene scene;

    scene.canvas->newChild<controls::internal::ToolBarDropDown>();
    scene.canvas->newChild<controls::internal::ToolBarSplitButton>();
    scene.frame();
    for(const char *type : {"ToolBar", "ToolButton", "ToolBarOverflowButton", "ToolBarDropDown",
        "ToolBarSplitButton"})
    {
        const auto &missing=controls::typesWithoutSkinRenderer();

        CHECK(std::find(missing.begin(), missing.end(), type) == missing.end());
    }

    const std::vector<size_t> bar=Scene::slotsOf(scene.bar);

    REQUIRE(bar.size() == 1);
    CHECK(sameColor(scene.renderer.draws[bar[0]].top, tn::ToolBar.top));

    const std::vector<size_t> open=Scene::slotsOf(scene.buttons[0]);

    REQUIRE(open.size() == 1);
    CHECK(sameColor(scene.renderer.draws[open[0]].top, tn::ToolButton.top));
    CHECK(sameColor(scene.renderer.draws[open[0]].border, tn::ToolButton.border));

    // the toggle on, focused
    controls::ToolButton *bold=scene.buttons.back();

    bold->setToggleState(true);
    scene.frame();
    const size_t boldSlot=Scene::slotsOf(bold)[0];
    CHECK(sameColor(scene.renderer.draws[boldSlot].top, tn::ToolButtonOn.top));
    bold->focus();
    scene.frame();
    CHECK(sameColor(scene.renderer.draws[boldSlot].border, tn::ButtonFocus.border));
}

TEST_CASE("ToolBar look: a disabled toggle that is on keeps its on look")
{
    Scene scene;
    controls::ToolButton *bold=scene.buttons.back();

    bold->setToggleState(true);
    bold->setDisabled(true);
    scene.frame();
    const size_t boldSlot=Scene::slotsOf(bold)[0];
    CHECK(sameColor(scene.renderer.draws[boldSlot].top, tn::ToolButtonOn.top));

    // off and disabled, the flat rest look
    bold->setToggleState(false);
    scene.frame();
    CHECK(sameColor(scene.renderer.draws[boldSlot].top, tn::ToolButton.top));
}

TEST_CASE("ToolBar look: a hover on a tool button writes its slot alone")
{
    Scene scene;
    controls::ToolButton *save=scene.buttons[1];
    const std::vector<size_t> slots=Scene::slotsOf(save);

    REQUIRE(slots.size() == 1);
    scene.moveOff();
    scene.moveTo(save);
    CHECK(scene.renderer.touched == std::set<size_t>(slots.begin(), slots.end()));
    CHECK(sameColor(scene.renderer.draws[slots[0]].top, tn::ToolButtonHover.top));

    scene.frame();
    CHECK(scene.renderer.touched.empty());

    scene.moveOff();
    CHECK(scene.renderer.touched == std::set<size_t>(slots.begin(), slots.end()));
    CHECK(sameColor(scene.renderer.draws[slots[0]].top, tn::ToolButton.top));
}

TEST_CASE("ToolBar look: the overflow button draws its chevron over its surface, pressed while open")
{
    Scene scene;

    scene.narrowTo(3);
    scene.settle();

    controls::internal::ToolBarOverflowButton *more=scene.bar->getOverflowButton();
    const std::vector<size_t> slots=Scene::slotsOf(more);

    REQUIRE_FALSE(more->hidden());
    REQUIRE(slots.size() == 2);
    CHECK(scene.renderer.icons[slots[1]].kind == 1);
    CHECK(scene.renderer.icons[slots[1]].z == more->getZIndex()+1);
    CHECK(sameColor(scene.renderer.draws[slots[0]].top, tn::ToolButton.top));

    more->openMenu(controls::MenuFocus::None);
    scene.frame();
    CHECK(sameColor(scene.renderer.draws[slots[0]].top, tn::ToolButtonDown.top));
}

TEST_CASE("ToolBar look: a width change moving one item writes its slots, the button's and the resized controls' alone")
{
    Scene scene;

    // into the menu one at a time, then back
    std::vector<int> widths;

    for(size_t shown=scene.buttons.size(); shown > 1; --shown)
    {
        const int before=scene.box->getWidth();

        scene.narrowTo(shown-1);
        widths.push_back(before);
    }

    std::set<size_t> allowed;

    for(controls::Base *control : {static_cast<controls::Base *>(scene.box),
        static_cast<controls::Base *>(scene.bar), static_cast<controls::Base *>(scene.bar->getOverflowButton())})
    {
        for(size_t id : Scene::slotsOf(control))
            allowed.insert(id);
    }

    // one more step, the moved item's slots and the separator's allowed too
    for(size_t shown=1; shown < scene.buttons.size(); ++shown)
    {
        std::set<size_t> expected=allowed;
        std::vector<controls::Base *> moved;

        for(controls::Base *item : scene.bar->getItems())
        {
            if(item->hidden() && !gweni_cast<controls::Separator>(item))
            {
                moved.push_back(item);
                break;
            }
        }
        // the first hidden button comes back (with the separator before it)
        for(controls::Base *item : scene.bar->getItems())
        {
            if(item->hidden())
                for(size_t id : Scene::slotsOf(item))
                    expected.insert(id);
            if(item == moved.front())
                break;
        }

        const int entries=scene.sibling->entries;

        INFO("shown " << shown);
        scene.box->setBounds(0, 0, widths[widths.size()-shown], 60);
        const controls::LayoutStats &stats=scene.frame();

        CHECK(stats.passes == 1);
        CHECK(scene.sibling->entries == entries);
        CHECK_FALSE(moved.front()->hidden());
        for(size_t id : scene.renderer.touched)
        {
            INFO("slot " << id);
            CHECK(expected.count(id) == 1);
        }
    }
}
