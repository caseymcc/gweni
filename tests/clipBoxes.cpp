/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's shader clipping: every primitive and glyph slot carries the
 *  clipRegion() current when it was drawn, and the fragment shaders discard what
 *  lies outside it. Drawn offscreen and read back.
 *
 *  The renderer is shared with the other cases, so each case puts the clip
 *  region back to the whole view when it ends (ClipGuard).
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <algorithm>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::OpenGLCore;

const Color background(0, 0, 255, 255);
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

    bool isBackground(int x, int y) const
    {
        const Color c=at(x, y);

        return (c.r == background.r) && (c.g == background.g) && (c.b == background.b);
    }

    bool isWhite(int x, int y) const
    {
        const Color c=at(x, y);

        return (c.r == white.r) && (c.g == white.g) && (c.b == white.b);
    }

    //! Pixels in columns [x0, x1) that are not the background.
    int drawnInColumns(int x0, int x1) const
    {
        const Point size=gweni::test::viewSize();
        int drawn=0;

        for(int y=0; y<size.y; ++y)
            for(int x=x0; x<x1; ++x)
                if(!isBackground(x, y))
                    ++drawn;
        return drawn;
    }
};

//! Clears to `background`, draws one frame with `draw` and reads it back.
template<typename Draw>
Frame drawFrame(Draw &&draw)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Point size=gweni::test::viewSize();

    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    renderer.begin();
    draw(renderer);
    renderer.end();

    Frame frame;

    gweni::capture::readFramebufferRgba(size.x, size.y, frame.pixels);
    return frame;
}

Rect wholeView()
{
    return Rect(Point(0, 0), gweni::test::viewSize());
}

//! Puts the clip region back to the whole view, so the next case draws unclipped.
struct ClipGuard
{
    ~ClipGuard() { gweni::test::gpuRenderer().setClipRegion(wholeView()); }
};

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
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        for(size_t id:ids)
            renderer.releasePrimitive(id);
        renderer.begin();
        renderer.end();
    }

    std::vector<size_t> ids;
};

}//namespace

TEST_CASE("Clip boxes: a primitive is cut at the region it was drawn in, after a hidden slot")
{
    ClipGuard guard;
    Primitives prims(3);
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    // ids[0] is drawn unclipped and hidden: a hidden slot still counts in
    // gl_PrimitiveID, so the slots after it must still find their own boxes
    renderer.setClipRegion(wholeView());
    renderer.setDrawColor(white);
    renderer.drawFilledRect(prims.ids[0], wholeView(), 1);
    renderer.hidePrimitive(prims.ids[0]);

    const Frame frame=drawFrame([&](OpenGLCore &renderer)
        {
            renderer.setDrawColor(white);
            renderer.setClipRegion(Rect(40, 30, 100, 50));
            renderer.drawFilledRect(prims.ids[1], wholeView(), 2);
            renderer.setClipRegion(Rect(200, 30, 100, 50));
            renderer.drawRoundedRect(prims.ids[2], wholeView(), 0.0f, 0.0f, white, white, white, 3);
        });

    // the fill: [40,140) x [30,80)
    CHECK(frame.isWhite(40, 30));
    CHECK(frame.isWhite(139, 79));
    CHECK(frame.isBackground(39, 30));
    CHECK(frame.isBackground(40, 29));
    CHECK(frame.isBackground(140, 79));
    CHECK(frame.isBackground(139, 80));

    // the rounded rect: [200,300) x [30,80)
    CHECK(frame.isWhite(250, 55));
    CHECK(frame.isBackground(199, 55));
    CHECK(frame.isBackground(300, 55));
    CHECK(frame.isBackground(250, 29));
    CHECK(frame.isBackground(250, 80));

    CHECK(frame.isBackground(170, 55));
    CHECK(frame.isBackground(5, 5));
}

TEST_CASE("Clip boxes: a region past the top-left edge is clamped, an empty one draws nothing")
{
    ClipGuard guard;
    Primitives prims(2);

    const Frame frame=drawFrame([&](OpenGLCore &renderer)
        {
            renderer.setDrawColor(white);
            // a negative corner must not wrap in uint16_t: this keeps [0,40) x [0,30)
            renderer.setClipRegion(Rect(-20, -20, 60, 50));
            renderer.drawFilledRect(prims.ids[0], wholeView(), 1);
            // as addClipRegion leaves a region that misses its parent: negative width
            renderer.setClipRegion(Rect(200, 40, -10, 50));
            renderer.drawFilledRect(prims.ids[1], wholeView(), 2);
        });

    CHECK(frame.isWhite(0, 0));
    CHECK(frame.isWhite(39, 29));
    CHECK(frame.isBackground(40, 0));
    CHECK(frame.isBackground(0, 30));

    CHECK(frame.isBackground(195, 60));
    CHECK(frame.isBackground(200, 60));
}

TEST_CASE("Clip boxes: text is cut, and follows a new region when moved or only re-clipped")
{
    ClipGuard guard;
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const gweni::String text="MMMMMMMMMMMMMMMM";
    const int cut=30;
    const int width=gweni::test::viewSize().x;
    size_t textId=0;

    renderer.generateTextPrimitive(textId);

    auto drawText=[&](Point pos, Rect clip)
    {
        return drawFrame([&](OpenGLCore &renderer)
            {
                renderer.setDrawColor(white);
                renderer.setClipRegion(clip);
                renderer.renderText(textId, gweni::test::openSans(), pos, 1, text);
            });
    };

    SUBCASE("laid out")
    {
        const Frame frame=drawText(Point(4, 40), Rect(0, 0, cut, 160));

        CHECK(frame.drawnInColumns(0, cut) > 0);
        CHECK(frame.drawnInColumns(cut, width) == 0);
    }

    SUBCASE("moved: the unchanged string takes the region it is drawn in now")
    {
        drawText(Point(4, 40), Rect(0, 0, cut, 160));

        const Frame frame=drawText(Point(8, 60), wholeView());

        CHECK(frame.drawnInColumns(cut, width) > 0);
    }

    SUBCASE("not moved, only re-clipped")
    {
        const Frame unclipped=drawText(Point(4, 40), wholeView());

        CHECK(unclipped.drawnInColumns(cut, width) > 0);

        const Frame clipped=drawText(Point(4, 40), Rect(0, 0, cut, 160));

        CHECK(gweni::test::gpuRenderer().getFrameStats().textSlotsWritten > 0);
        CHECK(clipped.drawnInColumns(0, cut) > 0);
        CHECK(clipped.drawnInColumns(cut, width) == 0);

        // the same string, place and region again: nothing to upload
        drawText(Point(4, 40), Rect(0, 0, cut, 160));
        CHECK(gweni::test::gpuRenderer().getFrameStats().textSlotsWritten == 0);
        CHECK(gweni::test::gpuRenderer().getFrameStats().uploadBytes == 0);
    }

    renderer.releaseTextPrimitive(textId);
    renderer.begin();
    renderer.end();
}
