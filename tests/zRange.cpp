/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's z range, drawn offscreen and read back: every z index up to
 *  maxZIndex() (65535, the 16 bits of Vertex::z) is drawn, and a higher one in
 *  front of a lower one whatever their draw order, at the top of the range
 *  too (z 60000/60001 and 65534/65535; the old -10000 projection clipped them
 *  away). At 256 depth steps per z unit, the glyphs of a run longer than
 *  about 254 glyphs share depth values (glyph i is at z+(i+1)/(glyphs+1)), so
 *  a coverage glyph's empty pixels must write no depth: a long run's tail is
 *  drawn exactly as the same glyphs drawn as a short run of their own, whose
 *  glyphs are several depth steps apart.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::Rect;
using gweni::String;
using gweni::renderer::TextMode;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 255, 255);
const Color red(255, 0, 0, 255);
const Color green(0, 255, 0, 255);
const Color white(255, 255, 255, 255);

//! One frame read back from the offscreen framebuffer, top-down RGBA8.
struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }

    bool is(int x, int y, const Color &c) const
    {
        const Color p=at(x, y);

        return (p.r == c.r) && (p.g == c.g) && (p.b == c.b);
    }

    //! Pixels in columns [x0, x1) that are not the background.
    int drawnInColumns(int x0, int x1) const
    {
        int drawn=0;

        for(int y=0; y<gweni::test::viewSize().y; ++y)
            for(int x=x0; x<x1; ++x)
                if(!is(x, y, background))
                    ++drawn;
        return drawn;
    }

    //! Pixels in columns [x0, x1) that differ from \p other's.
    int differingInColumns(const Frame &other, int x0, int x1) const
    {
        int differing=0;

        for(int y=0; y<gweni::test::viewSize().y; ++y)
            for(int x=x0; x<x1; ++x)
                if(!other.is(x, y, at(x, y)))
                    ++differing;
        return differing;
    }
};

//! Clears to `background`, draws one frame with `draw` and reads it back.
template<typename Draw>
Frame drawFrame(Draw &&draw)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    gweni::test::clearView(background);

    renderer.begin();
    draw(renderer);
    renderer.end();

    Frame frame;

    gweni::test::readView(frame.pixels);
    return frame;
}

Rect wholeView()
{
    return Rect(Point(0, 0), gweni::test::viewSize());
}

//! Primitive ids for one test case, released (and that flushed) when it ends, so the
//! next case's frame starts empty.
struct Primitives
{
    explicit Primitives(size_t count)
        : ids(count)
    {
        for(size_t &id:ids)
            gweni::test::gpuRenderer().generatePrimitive(id);
        // slot order is draw order: ids[0] is drawn first
        std::sort(ids.begin(), ids.end());
    }

    ~Primitives()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        for(size_t id:ids)
            renderer.releasePrimitive(id);
        renderer.begin();
        renderer.end();
    }

    std::vector<size_t> ids;
};

//! Text ids for one test case, released when it ends.
struct Texts
{
    explicit Texts(size_t count)
        : ids(count)
    {
        for(size_t &id:ids)
            gweni::test::gpuRenderer().generateTextPrimitive(id);
    }

    ~Texts()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        for(size_t id:ids)
            renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
    }

    std::vector<size_t> ids;
};

}//namespace

TEST_CASE("z range: SlotRenderer reports 65535, the 16 bits of Vertex::z")
{
    CHECK(gweni::test::gpuRenderer().maxZIndex() == 65535);
    CHECK(SlotRenderer::kMaxZIndex == 65535);
}

TEST_CASE("z range: at the top of the range a higher z is drawn in front, in either draw order")
{
    Primitives prims(4);
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.setClipRegion(wholeView());

    for(int top:{ 60001, SlotRenderer::kMaxZIndex })
    {
        CAPTURE(top);

        // [20,120) x [20,100) and [70,170) x [50,130) overlap in [70,120) x [50,100);
        // the second pair the same, 200 px to the right
        const Frame frame=drawFrame([&](SlotRenderer &renderer)
            {
                // the higher z first: the lower one drawn after it must not cover it
                renderer.setDrawColor(red);
                renderer.drawFilledRect(prims.ids[0], Rect(20, 20, 100, 80), top);
                renderer.setDrawColor(green);
                renderer.drawFilledRect(prims.ids[1], Rect(70, 50, 100, 80), top-1);
                // the lower z first: the higher one drawn after it covers it
                renderer.setDrawColor(green);
                renderer.drawFilledRect(prims.ids[2], Rect(220, 20, 100, 80), top-1);
                renderer.setDrawColor(red);
                renderer.drawFilledRect(prims.ids[3], Rect(270, 50, 100, 80), top);
            });

        // each rect is drawn where it does not overlap
        CHECK(frame.is(30, 30, red));
        CHECK(frame.is(160, 120, green));
        CHECK(frame.is(230, 30, green));
        CHECK(frame.is(360, 120, red));
        // and the higher z wins the overlap
        CHECK(frame.is(95, 75, red));
        CHECK(frame.is(295, 75, red));
        CHECK(frame.is(5, 5, background));
    }
}

TEST_CASE("z range: a long coverage run's tail draws as the same glyphs in a short run")
{
    Texts texts(1);
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    Font font=gweni::test::openSans();

    font.size=12.0f;
    if(renderer.getTextMode(font) != TextMode::Coverage)
    {
        MESSAGE("skipped: GWENI_SDF_TEXT_MIN puts 12 px in the SDF range");
        return;
    }

    // "To": the o kerned in under the T's bar, inside the T's glyph box but not its
    // ink, so with the boxes writing depth a tied o would be lost
    String tail;
    for(int i=0; i<12; ++i)
        tail+="To ";
    String prefix;
    for(int i=0; i<100; ++i)
        prefix+="To ";

    const String run=prefix+tail;

    REQUIRE(run.size() > 300);
    renderer.setClipRegion(wholeView());

    // the run ends inside the right edge of the view, its prefix off to the left
    const int prefixWidth=renderer.measureText(font, prefix).x;
    const int tailWidth=renderer.measureText(font, tail).x;
    const int tailX=gweni::test::viewSize().x-tailWidth-8;
    const int y=60;
    const int z=1;

    REQUIRE(tailX > 0);

    const Frame whole=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(white);
            renderer.renderText(texts.ids[0], font, Point(tailX-prefixWidth, y), z, run);
        });
    const Frame alone=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(white);
            renderer.renderText(texts.ids[0], font, Point(tailX, y), z, tail);
        });

    const int x1=gweni::test::viewSize().x;

    REQUIRE(alone.drawnInColumns(tailX, x1) > 0);
    CHECK(whole.drawnInColumns(tailX, x1) == alone.drawnInColumns(tailX, x1));
    CHECK(whole.differingInColumns(alone, tailX, x1) == 0);
}
