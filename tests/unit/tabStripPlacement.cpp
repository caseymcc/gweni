/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Where a TabControl's strip sits against its page frame (the TabControlInner
 *  panel), on a real Canvas with the (un-init()'d) textured skin, for a strip on
 *  each of the four sides.
 *
 *  The strip reaches TabStrip::PageOverlap (1) unit into the page on the side
 *  facing it, a negative margin on that side alone, so the tabs' last row (or
 *  column) is the frame's first: the selected tab, drawn over the frame, opens
 *  into the page inside its own bounds. Every tab spans the strip across the
 *  row, overlap included. The row starts TabStrip::LeadingPadding (6, the
 *  textured page frame's corner radius) units in along the strip, so a first
 *  tab's edge meets the frame's straight border. Docking the strip on another
 *  side at run time sets the margin and padding of the new side and clears the
 *  old one's, and a strip moved back is placed as it first was. The shown page
 *  keeps its 6-unit margin inside the frame. A Bottom strip that overflows
 *  centres its scroll buttons on the row as a Top one does.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/controls/tabControl.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(500, 400);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        delete canvas;
    }

    //! A fixed 300x150 TabControl with \p count pages, the first selected.
    controls::TabControl *tabControl(int count, int width=300)
    {
        static const char *const captions[]={"main.cpp", "app.h", "app.cpp", "window.h",
            "window.cpp", "render.cpp", "shaders.glsl", "README.md"};
        controls::TabControl *control=canvas->newChild<controls::TabControl>();

        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(10, 10, width, 150);
        for(int i=0; i < count; ++i)
            control->addPage(captions[i]);
        return control;
    }

    void settle()
    {
        for(int i=0; i < 3; ++i)
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
    }
};

//! The TabControl's page frame, its inner panel.
const controls::Base *pageFrame(controls::TabControl *control)
{
    return control->getInnerPanel();
}

bool sameMargin(const Margin &a, const Margin &b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

const struct
{
    const char *name;
    DockPosition dock;
    Margin margin;      //!< the strip's: -PageOverlap on the side facing the page
    bool horizontal;    //!< a row of tabs along x
} g_sides[]=
{
    {"Top", DockPosition::Top, Margin(0, 0, 0, -controls::TabStrip::PageOverlap), true},
    {"Bottom", DockPosition::Bottom, Margin(0, -controls::TabStrip::PageOverlap, 0, 0), true},
    {"Left", DockPosition::Left, Margin(0, 0, -controls::TabStrip::PageOverlap, 0), false},
    {"Right", DockPosition::Right, Margin(-controls::TabStrip::PageOverlap, 0, 0, 0), false}
};

//! The strip of \p control docked as \p side says, its tabs and frame placed so.
void checkPlacement(controls::TabControl *control, const decltype(g_sides[0]) &side)
{
    controls::TabStrip *strip=control->getTabStrip();
    const controls::Base *frame=pageFrame(control);
    const int overlap=controls::TabStrip::PageOverlap;

    REQUIRE(frame);
    CHECK((strip->getDock() == side.dock));
    CHECK(sameMargin(strip->getMargin(), side.margin));

    const Rect s=strip->getBounds();
    const Rect f=frame->getBounds();

    // the strip reaches `overlap` into the frame on the side facing it
    switch(side.dock)
    {
    case DockPosition::Top:
        CHECK(f.y == s.y+s.h-overlap);
        break;
    case DockPosition::Bottom:
        CHECK(f.y+f.h == s.y+overlap);
        break;
    case DockPosition::Left:
        CHECK(f.x == s.x+s.w-overlap);
        break;
    default:
        CHECK(f.x+f.w == s.x+overlap);
        break;
    }

    // the row starts LeadingPadding in, and each tab spans the strip across it
    const controls::TabButton *first=control->getTab(0);

    CHECK((side.horizontal ? first->getX() : first->getY()) == controls::TabStrip::LeadingPadding);
    for(int i=0; i < control->tabCount(); ++i)
    {
        const Rect t=control->getTab(i)->getBounds();

        CAPTURE(i);
        if(side.horizontal)
        {
            CHECK(t.y == 0);
            CHECK(t.h == s.h);
        }
        else
        {
            CHECK(t.x == 0);
            CHECK(t.w == s.w);
        }
    }

    // the shown page keeps its margin inside the frame
    controls::Base *page=control->getCurrentButton()->getPage();

    CHECK(page->getX() == 6);
    CHECK(page->getY() == 6);
    CHECK(page->getWidth() == f.w-12);
    CHECK(page->getHeight() == f.h-12);
}

}//namespace

TEST_CASE("TabStrip on each side reaches PageOverlap into the page frame, its row LeadingPadding in")
{
    for(const auto &side : g_sides)
    {
        CAPTURE(side.name);

        Scene scene;
        controls::TabControl *control=scene.tabControl(3);

        control->setTabStripPosition(side.dock);
        scene.settle();
        checkPlacement(control, side);
    }
}

TEST_CASE("TabStrip moved to another side at run time is placed for the new side, and back as it was")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl(3);

    scene.settle();

    const Rect firstStrip=control->getTabStrip()->getBounds();
    const Rect firstFrame=pageFrame(control)->getBounds();

    for(const auto &side : g_sides)
    {
        CAPTURE(side.name);
        control->setTabStripPosition(side.dock);
        scene.settle();
        checkPlacement(control, side);
    }

    control->setTabStripPosition(DockPosition::Top);
    scene.settle();
    checkPlacement(control, g_sides[0]);
    CHECK((control->getTabStrip()->getBounds() == firstStrip));
    CHECK((pageFrame(control)->getBounds() == firstFrame));
}

TEST_CASE("A Bottom TabStrip that overflows centres its scroll buttons on the row")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl(8, 200);

    control->setTabStripPosition(DockPosition::Bottom);
    scene.settle();
    REQUIRE(control->isOverflowing());

    const Rect s=control->getTabStrip()->getBounds();

    for(controls::Base *child : control->getActualChildren())
    {
        if(!gweni_cast<controls::internal::ScrollBarButton>(child) || child->hidden())
            continue;

        const Rect b=child->getBounds();

        CHECK(b.y == s.y+(s.h-controls::TabControl::ButtonSize)/2);
        CHECK(b.x+b.w <= control->getWidth());
    }
}
