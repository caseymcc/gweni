/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's Timeline drawn offscreen on the backend under test at
 *  scales 1 and 1.5: where two bars of a lane overlap, the one added later
 *  shows, whether it is the shorter one over a long bar or the long one over a
 *  short bar. A lane's bar slots follow the bars' indices, so the later bar's
 *  slot is the one drawn second, and at the same z the depth test would keep
 *  the earlier one's pixels: what puts the later bar in front is its overlap
 *  level's layer. The bars are square at whole units, so their device pixels
 *  are covered fully: the column at the later bar's left edge is its border
 *  and the one before it the earlier bar's fill, with no blend. The hover
 *  look over the later bar draws its border in TimelineBarHover's colour and,
 *  inside it, the bar's fill with TimelineBarHover's blended over it. The z layers are checked headless in tests/unit/timelineRenderer.cpp.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/timeline.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::controls::Timeline;
using gweni::controls::TimelineBar;
namespace tn=gweni::skin::textured::tokyonight;

const Color Early(220, 40, 40);
const Color Late(40, 200, 60);

bool same(const Color &a, const Color &b)
{
    return std::abs(a.r-b.r) <= 1 && std::abs(a.g-b.g) <= 1 && std::abs(a.b-b.b) <= 1;
}

struct Scene
{
    float scale;
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;
    Timeline *timeline;
    std::vector<uint8_t> pixels;

    explicit Scene(float s):
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

        timeline=canvas->newChild<Timeline>();
        timeline->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
        timeline->setBounds(7, 9, 300, 120);
        timeline->setCaptionColumnWidth(40);
        timeline->setUtcOffset(gweni::controls::axis::fixedUtcOffset(0));
        timeline->setXRange(0.0, 100.0);
        timeline->addLane("a");
        timeline->addLane("b");
        // a short bar added over a long one, and a long one over a short one
        timeline->addBar(0, TimelineBar(10, 90, 1).withColor(Early));
        timeline->addBar(0, TimelineBar(40, 60, 2).withColor(Late));
        timeline->addBar(1, TimelineBar(40, 60, 3).withColor(Early));
        timeline->addBar(1, TimelineBar(10, 90, 4).withColor(Late));
    }

    ~Scene()
    {
        delete canvas;
        delete skin;

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        // scale 1 for the next Scene, as tests/tabJoin.cpp's
        renderer.setScale(1.0f);
        renderer.begin();
        renderer.end();
    }

    void settle()
    {
        for(int i=0; i < 3; ++i)
        {
            gweni::test::clearView(Color(0, 0, 0, 255));
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
        gweni::test::readView(pixels);
    }

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }

    int snap(int v) const { return gweni::renderer::Base::snap(float(v), scale); }

    //! The bar's rect in canvas units.
    Rect bar(size_t lane, size_t index) const
    {
        Rect rect;

        REQUIRE(timeline->getBarRect(lane, index, rect));
        const Point origin=timeline->localPosToCanvas(Point(rect.x, rect.y));

        return Rect(origin.x, origin.y, rect.w, rect.h);
    }
};

}//namespace

TEST_CASE("Timeline: the later of two overlapping bars shows, over a long bar or under a short one")
{
    for(const float scale : {1.0f, 1.5f})
    {
        CAPTURE(scale);

        Scene scene(scale);

        scene.settle();

        // lane 0: the short later bar over the long earlier one
        const Rect longBar=scene.bar(0, 0);
        const Rect shortBar=scene.bar(0, 1);
        const int middleY=scene.snap(shortBar.y+shortBar.h/2);

        CHECK(scene.timeline->getOverlapLevel(0, 1) == 1);
        CHECK(same(scene.at(scene.snap(shortBar.x+shortBar.w/2), middleY), Late));
        CHECK(same(scene.at(scene.snap(longBar.x+5), middleY), Early));
        // its left column is its border, the one before it the long bar's fill
        CHECK(same(scene.at(scene.snap(shortBar.x), middleY), tn::TimelineBar.border));
        CHECK(same(scene.at(scene.snap(shortBar.x)-1, middleY), Early));
        CHECK(same(scene.at(scene.snap(shortBar.x+shortBar.w)-1, middleY), tn::TimelineBar.border));
        CHECK(same(scene.at(scene.snap(shortBar.x+shortBar.w), middleY), Early));

        // lane 1: the long later bar over the short earlier one, which is gone
        const Rect over=scene.bar(1, 1);
        const Rect under=scene.bar(1, 0);
        const int laneY=scene.snap(over.y+over.h/2);

        CHECK(scene.timeline->getOverlapLevel(1, 1) == 1);
        for(int x=scene.snap(under.x)-1; x <= scene.snap(under.x+under.w); ++x)
        {
            CAPTURE(x);
            CHECK(same(scene.at(x, laneY), Late));
        }

        // the hover look over the short later bar: its border, and the bar's
        // fill lightened by the hover's inside it, opaque
        const Point mouse(shortBar.x+shortBar.w/2, shortBar.y+shortBar.h/2);

        scene.canvas->inputMouseMoved(mouse.x, mouse.y, 0, 0);
        scene.settle();
        REQUIRE(scene.timeline->isHovering());
        CHECK(scene.timeline->getHoverIndex() == 1);
        CHECK(same(scene.at(scene.snap(shortBar.x+shortBar.w/2), scene.snap(shortBar.y)),
            tn::TimelineBarHover.border));
        CHECK(same(scene.at(scene.snap(shortBar.x), middleY), tn::TimelineBarHover.border));
        const Color tint=tn::TimelineBarHover.top;
        auto blend=[&tint](int under, int top) { return (under*(255-tint.a)+top*tint.a+127)/255; };
        const Color lit(blend(Late.r, tint.r), blend(Late.g, tint.g), blend(Late.b, tint.b));

        CHECK(same(scene.at(scene.snap(shortBar.x+shortBar.w/2), middleY), lit));
        CHECK(same(scene.at(scene.snap(shortBar.x)-1, middleY), Early));
    }
}
