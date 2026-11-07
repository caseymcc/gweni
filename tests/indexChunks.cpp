/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's chunked primitive draws: every slot is a quad in one static
 *  16-bit index buffer, and the slots are drawn in chunks of at most
 *  OpenGLCore::kSlotsPerChunk, each with its vertex attributes pointed at its
 *  first slot. A slot past 16383 (which a 16-bit index of slot*4 cannot reach)
 *  draws its own vertexes, and a chunk size of a few slots draws exactly the
 *  pixels of one chunk, one draw per chunk, each slot clipped by its own box.
 *  Drawn offscreen and read back.
 *
 *  The renderer is shared with the other cases, so each case puts the clip
 *  region and the chunk size back when it ends.
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
constexpr size_t gridCells=12;

//! One frame read back from the offscreen framebuffer, top-down RGBA8.
struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }

    bool is(int x, int y, const Color &color) const
    {
        const Color c=at(x, y);

        return (c.r == color.r) && (c.g == color.g) && (c.b == color.b);
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

//! Puts the clip region back to the whole view and the chunk size back to what it
//! was, so the next case draws as before.
struct Guard
{
    size_t chunkSlots=gweni::test::gpuRenderer().indexChunkSlots();

    ~Guard()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        renderer.setClipRegion(Rect(Point(0, 0), gweni::test::viewSize()));
        renderer.setIndexChunkSlots(chunkSlots);
    }
};

//! Primitive ids for one test case, released (and that flushed) when it ends.
struct Primitives
{
    ~Primitives()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        for(size_t id:ids)
            renderer.releasePrimitive(id);
        renderer.begin();
        renderer.end();
    }

    size_t generate()
    {
        size_t id=0;

        gweni::test::gpuRenderer().generatePrimitive(id);
        ids.push_back(id);
        return id;
    }

    std::vector<size_t> ids;
};

//! Cell i of a 6x2 grid of 60x60 rects.
Rect cell(size_t i)
{
    return Rect(10+static_cast<int>(i%6)*70, 10+static_cast<int>(i/6)*70, 60, 60);
}

Color cellColor(size_t i)
{
    return Color(static_cast<unsigned char>(20+i*19), static_cast<unsigned char>(240-i*17), 60, 255);
}

//! Draws cell i into slot ids[i], the whole cell, clipped to its left half: a slot
//! that took another's clip box would show its right half or lose its left.
void drawGrid(OpenGLCore &renderer, const std::vector<size_t> &ids)
{
    for(size_t i=0; i<ids.size(); ++i)
    {
        const Rect rect=cell(i);

        renderer.setClipRegion(Rect(rect.x, rect.y, rect.w/2, rect.h));
        renderer.setDrawColor(cellColor(i));
        renderer.drawFilledRect(ids[i], rect, static_cast<int>(i+1));
    }
}

//! Every cell's left half in its colour, its right half and the gaps background.
void checkGrid(const Frame &frame, size_t cells)
{
    for(size_t i=0; i<cells; ++i)
    {
        const Rect rect=cell(i);
        const Color color=cellColor(i);

        CAPTURE(i);
        CHECK(frame.is(rect.x, rect.y, color));
        CHECK(frame.is(rect.x+rect.w/2-1, rect.y+rect.h-1, color));
        CHECK(frame.is(rect.x+rect.w/2, rect.y, background));
        CHECK(frame.is(rect.x+rect.w-1, rect.y+rect.h-1, background));
        CHECK(frame.is(rect.x-1, rect.y, background));
    }
}

//! The primitive draws of the last frame: its draws less its text draws.
size_t primitiveDraws()
{
    const OpenGLCore::FrameStats &stats=gweni::test::gpuRenderer().getFrameStats();

    return stats.drawCalls-stats.textDraws;
}

}//namespace

TEST_CASE("Index chunks: slots past 16383 draw their own vertexes and clip boxes")
{
    Guard guard;
    Primitives prims;
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    renderer.setIndexChunkSlots(OpenGLCore::kSlotsPerChunk);

    // ids are handed out lowest first, so take them until gridCells of them lie
    // past the first chunk and past what a 16-bit slot*4 index reaches (16383)
    std::vector<size_t> high;

    while(high.size() < gridCells)
    {
        const size_t id=prims.generate();

        if(id > 16383)
            high.push_back(id);
    }

    const Frame frame=drawFrame([&](OpenGLCore &renderer) { drawGrid(renderer, high); });

    checkGrid(frame, gridCells);
    // the slots reach into the second chunk
    CHECK(primitiveDraws() == 2);
}

TEST_CASE("Index chunks: a few slots per chunk draw the same pixels, one draw per chunk")
{
    Guard guard;
    Primitives prims;
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    std::vector<size_t> ids;

    for(size_t i=0; i<gridCells; ++i)
        ids.push_back(prims.generate());
    std::sort(ids.begin(), ids.end());

    renderer.setIndexChunkSlots(OpenGLCore::kSlotsPerChunk);
    const Frame whole=drawFrame([&](OpenGLCore &renderer) { drawGrid(renderer, ids); });
    const size_t wholeDraws=primitiveDraws();

    checkGrid(whole, gridCells);

    // one slot per draw: the draw count is the slot count, which earlier cases
    // (the one past 16383 above) may have grown past one chunk
    renderer.setIndexChunkSlots(1);
    CHECK(renderer.indexChunkSlots() == 1);
    const Frame single=drawFrame([&](OpenGLCore &renderer) { drawGrid(renderer, ids); });
    const size_t slotCount=primitiveDraws();

    CHECK(single.pixels == whole.pixels);
    CHECK(slotCount > ids.back());
    CHECK(wholeDraws == (slotCount+OpenGLCore::kSlotsPerChunk-1)/OpenGLCore::kSlotsPerChunk);

    // 5 slots per chunk splits the grid's slots between chunks
    renderer.setIndexChunkSlots(5);
    const Frame five=drawFrame([&](OpenGLCore &renderer) { drawGrid(renderer, ids); });

    CHECK(five.pixels == whole.pixels);
    CHECK(primitiveDraws() == (slotCount+4)/5);

    // clamped to [1, kSlotsPerChunk]
    renderer.setIndexChunkSlots(0);
    CHECK(renderer.indexChunkSlots() == 1);
    renderer.setIndexChunkSlots(OpenGLCore::kSlotsPerChunk+1);
    CHECK(renderer.indexChunkSlots() == OpenGLCore::kSlotsPerChunk);
}
