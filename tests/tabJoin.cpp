/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's notebook tabs joined to their page on the device-pixel
 *  grid, drawn offscreen on the backend under test at scales 1, 1.25, 1.5, 1.75
 *  and 2, for a strip on each of the four sides and the TabControl at a few
 *  positions (so its edges land on every phase of a fractional scale).
 *
 *  The page frame's border on the strip's side is max(1, snap(1)) px wide,
 *  while the strip's logical unit of overlap snaps to 1 or 2 px by its position.
 *  Under the selected tab, between its side borders, every pixel of the
 *  frame's border rows (columns) and of the first page row is the page fill: no
 *  border line runs across the tab's opening. The tab's side borders end on the
 *  frame's border, so the first page row under them is the page fill too. Under
 *  an unselected tab the frame's border rows are the frame's border colour, the
 *  border running on under it. The geometry is checked headless in
 *  tests/unit/tabLook.cpp and the reach's quad in tests/unit/roundedEdges.cpp.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/tabControl.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

using gweni::Color;
using gweni::DockPosition;
using gweni::Point;
namespace tn=gweni::skin::textured::tokyonight;

//! One frame read back from the offscreen framebuffer, top-down RGBA8.
struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }
};

bool near(const Color &a, const Color &b)
{
    return std::abs(a.r-b.r) <= 1 && std::abs(a.g-b.g) <= 1 && std::abs(a.b-b.b) <= 1;
}

//! A textured-skin canvas over the whole view at \p scale holding one TabControl.
struct Scene
{
    float scale;
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;
    gweni::controls::TabControl *control;

    Scene(float s, DockPosition side, Point at):
        scale(s)
    {
        const Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setScale(scale);
        canvas->setSize(static_cast<int>(std::lround(viewSize.x/scale)),
            static_cast<int>(std::lround(viewSize.y/scale)));

        control=canvas->newChild<gweni::controls::TabControl>();
        control->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
        control->setBounds(at.x, at.y, 150, 70);
        for(const char *caption : {"Main", "Edit", "View"})
            control->addPage(caption);
        control->setTabStripPosition(side);
    }

    ~Scene()
    {
        delete canvas;
        delete skin;

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        // flush the released slots; the canvas's clip, offset and scale are
        // reset before the next case (test::resetDrawState), and the next
        // Scene's canvas sets its own
        renderer.begin();
        renderer.end();
    }

    Frame settle()
    {
        Frame shot;

        for(int i=0; i < 3; ++i)
        {
            gweni::test::clearView(Color(0, 0, 0, 255));
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
        gweni::test::readView(shot.pixels);
        return shot;
    }

    int snap(int v) const { return gweni::renderer::Base::snap(float(v), scale); }
};

//! A pixel addressed along the strip's row and across it.
Color pixel(const Frame &shot, DockPosition side, int along, int across)
{
    const bool row=(side == DockPosition::Top) || (side == DockPosition::Bottom);

    return row?shot.at(along, across):shot.at(across, along);
}

}//namespace

TEST_CASE("Textured tabs: the selected tab covers the page frame's border rows at any scale, an unselected one leaves them")
{
    const struct
    {
        const char *name;
        DockPosition dock;
    } sides[]=
    {
        {"Top", DockPosition::Top}, {"Bottom", DockPosition::Bottom},
        {"Left", DockPosition::Left}, {"Right", DockPosition::Right}
    };

    for(const float scale : {1.0f, 1.25f, 1.5f, 1.75f, 2.0f})
    {
        for(const auto &side : sides)
        {
            for(const Point at : {Point(2, 2), Point(3, 5), Point(5, 3), Point(6, 7)})
            {
                CAPTURE(scale);
                const std::string sideName=side.name;
                CAPTURE(sideName);
                CAPTURE(at.x);
                CAPTURE(at.y);

                Scene scene(scale, side.dock, at);

                scene.control->getTab(1)->doAction();

                const Frame shot=scene.settle();
                const bool row=(side.dock == DockPosition::Top) || (side.dock == DockPosition::Bottom);
                const int border=std::max(1, scene.snap(1));
                gweni::controls::Base *frame=const_cast<gweni::controls::Base *>(scene.control->getInnerPanel());
                const Point frameAt=frame->localPosToCanvas(Point(0, 0));

                // the whole TabControl is in the view
                REQUIRE(scene.snap(at.x+150) <= gweni::test::viewSize().x);
                REQUIRE(scene.snap(at.y+70) <= gweni::test::viewSize().y);

                // the frame's border rows (columns) on the strip's side, and the
                // first page row (column) inside them
                int borderFirst, pageRow;

                switch(side.dock)
                {
                case DockPosition::Bottom:
                    borderFirst=scene.snap(frameAt.y+frame->getHeight())-border;
                    pageRow=borderFirst-1;
                    break;
                case DockPosition::Left:
                    borderFirst=scene.snap(frameAt.x);
                    pageRow=borderFirst+border;
                    break;
                case DockPosition::Right:
                    borderFirst=scene.snap(frameAt.x+frame->getWidth())-border;
                    pageRow=borderFirst-1;
                    break;
                default:
                    borderFirst=scene.snap(frameAt.y);
                    pageRow=borderFirst+border;
                    break;
                }

                // the frame's straight edge along the row, between its corner arcs
                const int radius=scene.snap(int(tn::TabControl.radius));
                const int straight0=scene.snap(row?frameAt.x:frameAt.y)+radius;
                const int straight1=scene.snap(row?frameAt.x+frame->getWidth()
                    :frameAt.y+frame->getHeight())-radius;

                for(int i=0; i < 3; ++i)
                {
                    gweni::controls::TabButton *tab=scene.control->getTab(i);
                    const Point tabAt=tab->localPosToCanvas(Point(0, 0));
                    const int a0=scene.snap(row?tabAt.x:tabAt.y);
                    const int a1=scene.snap(row?tabAt.x+tab->getWidth():tabAt.y+tab->getHeight());
                    const bool selected=tab->isActive();
                    const Color under=selected?tn::TabControl.top:tn::TabControl.border;
                    int wrong=0;

                    CAPTURE(i);
                    CAPTURE(selected);
                    CAPTURE(borderFirst);
                    CAPTURE(pageRow);
                    // the first two tabs are on the straight edge (the first tab's
                    // square corner within the arc's last px at a scale where
                    // snap(radius) is above radius*scale, a twentieth of a px off
                    // the straight edge there); the last may reach past the frame's
                    // far corner, and is checked up to it
                    if(i < 2)
                    {
                        REQUIRE(a0+1 >= straight0);
                        REQUIRE(a1 <= straight1);
                    }

                    // inside the tab's side borders (and an unselected tab's end inset)
                    for(int along=std::max(a0+2*border+1, straight0);
                        along < std::min(a1-2*border-1, straight1); ++along)
                    {
                        for(int across=borderFirst; across < borderFirst+border; ++across)
                        {
                            if(!near(pixel(shot, side.dock, along, across), under))
                                ++wrong;
                        }
                        if(!near(pixel(shot, side.dock, along, pageRow), tn::TabControl.top))
                            ++wrong;
                    }
                    CHECK(wrong == 0);

                    // the selected tab's side borders stop on the frame's border
                    if(selected)
                    {
                        CHECK(near(pixel(shot, side.dock, a0, pageRow), tn::TabControl.top));
                        CHECK(near(pixel(shot, side.dock, a1-1, pageRow), tn::TabControl.top));
                        CHECK(near(pixel(shot, side.dock, a0, borderFirst), tn::TabActive.border));
                        CHECK(near(pixel(shot, side.dock, a1-1, borderFirst), tn::TabActive.border));
                    }
                }
            }
        }
    }
}
