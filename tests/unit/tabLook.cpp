/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's notebook tabs (tokyoNight.h, "Tabs"), drawn on a real
 *  Canvas with the (un-init()'d) textured skin onto a recording StubRenderer,
 *  for a strip on each of the four sides.
 *
 *  Every tab is drawn square on the corners facing the page and without a
 *  border on that side (the RoundedEdge mask). The selected tab is its whole
 *  bounds in TabActive, the page frame's flat fill and border, with no accent;
 *  an unselected one is TabInactive, drawn UnselectedOuterInset shorter on the
 *  side away from the page, PageOverlap short of the page and UnselectedEndInset
 *  in at each end along the row, and hovered it is TabHover in the same rect.
 *  A selected tab does not change as it is hovered, a disabled one draws as an
 *  unselected one and does not hover, and the tab's text does not move with its
 *  state. The page frame (TabControlInner) is drawn in the TabControl surface,
 *  rounded on every corner and bordered on every side, and the TabControl
 *  itself has no primitive and draws nothing. Moving the strip to another side
 *  at run time draws the tabs for that side, Top to Bottom included, where a
 *  tab's bounds stay as they were. Selecting another tab draws into the two
 *  tabs' slots alone, never the frame's, and hovering a tab into its one slot.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/tabControl.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <vector>

using namespace gweni;
namespace tn=skin::textured::tokyonight;

namespace
{

//! Each slot's last rounded rect, and every slot a frame drew into.
class RectRecorder: public test::StubRenderer
{
public:
    using test::StubRenderer::drawRoundedRect;

    struct Draw
    {
        Rect rect;
        Color top, bottom, border;
        float radius, borderWidth;
        uint8_t edges;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex) override
    {
        draws[primitiveId]={rect, top, bottom, borderColor, radius, border, 0};
        touched.push_back(primitiveId);
        test::StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom,
            borderColor, zIndex);
    }

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex, uint8_t edges) override
    {
        drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
        draws[primitiveId].edges=edges;
    }

    std::map<size_t, Draw> draws;
    std::vector<size_t> touched;
};

struct Scene
{
    RectRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::TabControl *control=nullptr;
    float clock=10.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(500, 400);
        control=canvas->newChild<controls::TabControl>();
        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(10, 10, 300, 150);
        for(const char *caption : {"main.cpp", "app.h", "app.cpp"})
            control->addPage(caption);
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

    void frame()
    {
        renderer.touched.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
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
        canvas->inputMouseMoved(499, 399, 0, 0);
        frame();
    }

    void clickOn(controls::Base *over)
    {
        const Point at=over->localPosToCanvas();

        clock+=1.0f;
        canvas->inputMouseMoved(at.x+over->getWidth()/2, at.y+over->getHeight()/2, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    //! A control's one primitive slot.
    static size_t slotOf(const controls::Base *control)
    {
        std::vector<size_t> primitives, texts;

        control->getSkinControl()->appendIds(primitives, texts);
        REQUIRE(primitives.size() == 1);
        return primitives[0];
    }

    const RectRecorder::Draw &drawOf(const controls::Base *control)
    {
        const size_t id=slotOf(control);

        REQUIRE(renderer.draws.count(id) == 1);
        return renderer.draws.at(id);
    }
};

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool sameRect(const Rect &a, const Rect &b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

//! The draw is \p surface's colours, radius and border.
void checkSurface(const RectRecorder::Draw &draw, const tn::Surface &surface)
{
    CHECK(sameColor(draw.top, surface.top));
    CHECK(sameColor(draw.bottom, surface.bottom));
    CHECK(sameColor(draw.border, surface.border));
    CHECK(draw.radius == surface.radius);
    CHECK(draw.borderWidth == surface.borderWidth);
}

const struct
{
    const char *name;
    DockPosition dock;
    uint8_t edges;  //!< square on the page's side, no border there
} g_sides[]=
{
    {"Top", DockPosition::Top,
        renderer::SquareBottomLeft | renderer::SquareBottomRight | renderer::NoBorderBottom},
    {"Bottom", DockPosition::Bottom,
        renderer::SquareTopLeft | renderer::SquareTopRight | renderer::NoBorderTop},
    {"Left", DockPosition::Left,
        renderer::SquareTopRight | renderer::SquareBottomRight | renderer::NoBorderRight},
    {"Right", DockPosition::Right,
        renderer::SquareTopLeft | renderer::SquareBottomLeft | renderer::NoBorderLeft}
};

//! An unselected tab's drawn rect within its bounds \p b, for a strip on \p dock.
Rect unselectedRect(const Rect &b, DockPosition dock)
{
    const int outer=skin::textured::TabButton::UnselectedOuterInset;
    const int inner=controls::TabStrip::PageOverlap;
    const int end=skin::textured::TabButton::UnselectedEndInset;

    switch(dock)
    {
    case DockPosition::Bottom:
        return Rect(b.x+end, b.y+inner, b.w-2*end, b.h-inner-outer);
    case DockPosition::Left:
        return Rect(b.x+outer, b.y+end, b.w-outer-inner, b.h-2*end);
    case DockPosition::Right:
        return Rect(b.x+inner, b.y+end, b.w-inner-outer, b.h-2*end);
    default:
        return Rect(b.x+end, b.y+outer, b.w-2*end, b.h-outer-inner);
    }
}

}//namespace

TEST_CASE("Textured tabs: the selected tab is its bounds in TabActive, the others shorter, square and borderless on the page's side")
{
    for(const auto &side : g_sides)
    {
        CAPTURE(side.name);

        Scene scene;

        scene.control->setTabStripPosition(side.dock);
        scene.settle();

        for(int i=0; i < scene.control->tabCount(); ++i)
        {
            controls::TabButton *tab=scene.control->getTab(i);
            const RectRecorder::Draw &draw=scene.drawOf(tab);

            CAPTURE(i);
            CHECK(draw.edges == side.edges);
            if(tab->isActive())
            {
                CHECK(sameRect(draw.rect, tab->getRenderBounds()));
                checkSurface(draw, tn::TabActive);
            }
            else
            {
                CHECK(sameRect(draw.rect, unselectedRect(tab->getRenderBounds(), side.dock)));
                checkSurface(draw, tn::TabInactive);
            }
        }

        // the selected tab is the frame's fill and border
        CHECK(sameColor(tn::TabActive.top, tn::TabControl.top));
        CHECK(sameColor(tn::TabActive.bottom, tn::TabControl.top));
        CHECK(sameColor(tn::TabControl.bottom, tn::TabControl.top));
        CHECK(sameColor(tn::TabActive.border, tn::TabControl.border));

        // the frame is rounded and bordered all round; the TabControl draws nothing
        const RectRecorder::Draw &frame=scene.drawOf(scene.control->getInnerPanel());

        CHECK(frame.edges == 0);
        checkSurface(frame, tn::TabControl);

        std::vector<size_t> primitives, texts;

        scene.control->getSkinControl()->appendIds(primitives, texts);
        CHECK(primitives.empty());
    }
}

TEST_CASE("Textured tabs: hover gives an unselected tab TabHover in the same rect, a selected or disabled one stays")
{
    Scene scene;
    controls::TabButton *selected=scene.control->getTab(0);
    controls::TabButton *other=scene.control->getTab(1);
    controls::TabButton *disabled=scene.control->getTab(2);

    disabled->setDisabled(true);
    scene.settle();
    REQUIRE(selected->isActive());

    const Rect otherRect=scene.drawOf(other).rect;
    const Rect otherText=other->getChild(0)->getBounds();

    scene.moveTo(other);
    checkSurface(scene.drawOf(other), tn::TabHover);
    CHECK(sameRect(scene.drawOf(other).rect, otherRect));
    CHECK(scene.drawOf(other).edges == g_sides[0].edges);
    CHECK(sameRect(other->getChild(0)->getBounds(), otherText));
    // the hover is the hovered tab's one slot
    CHECK(scene.renderer.touched == std::vector<size_t>{Scene::slotOf(other)});

    scene.moveOff();
    checkSurface(scene.drawOf(other), tn::TabInactive);
    CHECK(sameRect(scene.drawOf(other).rect, otherRect));

    scene.moveTo(selected);
    checkSurface(scene.drawOf(selected), tn::TabActive);
    CHECK(sameRect(scene.drawOf(selected).rect, selected->getRenderBounds()));

    scene.moveTo(disabled);
    checkSurface(scene.drawOf(disabled), tn::TabInactive);
    CHECK(sameRect(scene.drawOf(disabled).rect,
        unselectedRect(disabled->getRenderBounds(), DockPosition::Top)));
}

TEST_CASE("Textured tabs: selecting another tab draws into the two tabs' slots alone")
{
    Scene scene;
    controls::TabButton *first=scene.control->getTab(0);
    controls::TabButton *second=scene.control->getTab(1);
    const size_t frameSlot=Scene::slotOf(scene.control->getInnerPanel());
    const Rect secondText=second->getChild(0)->getBounds();

    scene.moveTo(second);
    scene.clickOn(second);
    REQUIRE(second->isActive());

    std::vector<size_t> touched=scene.renderer.touched;

    std::sort(touched.begin(), touched.end());
    touched.erase(std::unique(touched.begin(), touched.end()), touched.end());

    std::vector<size_t> expected{Scene::slotOf(first), Scene::slotOf(second)};

    std::sort(expected.begin(), expected.end());
    CHECK(touched == expected);
    CHECK(std::find(touched.begin(), touched.end(), frameSlot) == touched.end());
    checkSurface(scene.drawOf(second), tn::TabActive);
    CHECK(sameRect(scene.drawOf(second).rect, second->getRenderBounds()));
    checkSurface(scene.drawOf(first), tn::TabInactive);
    CHECK(sameRect(second->getChild(0)->getBounds(), secondText));
}

TEST_CASE("Textured tabs: a strip moved Top to Bottom at run time draws its tabs for the new side")
{
    Scene scene;
    controls::TabButton *tab=scene.control->getTab(1);
    const Rect bounds=tab->getRenderBounds();

    scene.control->setTabStripPosition(DockPosition::Bottom);
    scene.settle();
    // the same bounds in the strip, drawn for the other side
    CHECK(sameRect(tab->getRenderBounds(), bounds));
    CHECK(scene.drawOf(tab).edges == g_sides[1].edges);
    CHECK(sameRect(scene.drawOf(tab).rect, unselectedRect(bounds, DockPosition::Bottom)));
}
