/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's two ways of drawing a primitive chunk's ranges of shown slots
 *  (#508): one glMultiDrawElements per chunk (desktop GL), or, where the
 *  context has no multi-draw (GLES 3.0, WebGL 2), the same ranges drawn one
 *  glDrawElements each (setUseMultiDraw(false), GWENI_NO_MULTI_DRAW=1). Both
 *  must draw the same pixels and count the same FrameStats: one draw per chunk
 *  either way, the same ranges and slots drawn, and switching uploads nothing.
 *
 *  The rects are a grid of 10x10 cells, each its own colour and its own clip
 *  box, with runs of them hidden between shown ones, so a chunk's draw has
 *  several ranges, more than maxPrimitiveRanges() at a chunk of the default
 *  size; at 7 slots a chunk the runs are split between chunks too. Drawn
 *  offscreen and read back. The renderer is shared with the other cases, so the
 *  case puts the multi-draw, the clip region and the chunk size back.
 *
 *  GL only (the test suite "gl_only"): the two ways are GL's (glMultiDrawElements
 *  or a loop of glDrawElements).
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "glRenderer.h"

#include <vector>

TEST_SUITE_BEGIN("gl_only");

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::OpenGLCore;

const Color background(0, 0, 64, 255);

std::vector<uint8_t> drawFrame()
{
    OpenGLCore &renderer=gweni::test::glRenderer();
    const Point size=gweni::test::viewSize();
    std::vector<uint8_t> pixels;

    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    renderer.begin();
    renderer.end();
    gweni::capture::readFramebufferRgba(size.x, size.y, pixels);
    return pixels;
}

//! Puts the multi-draw, the clip region and the chunk size back as they were,
//! and releases the case's primitive ids.
struct Guard
{
    bool multiDraw=gweni::test::glRenderer().useMultiDraw();
    size_t chunkSlots=gweni::test::glRenderer().indexChunkSlots();
    std::vector<size_t> ids;

    ~Guard()
    {
        OpenGLCore &renderer=gweni::test::glRenderer();

        for(size_t id:ids)
            renderer.releasePrimitive(id);
        renderer.setUseMultiDraw(multiDraw);
        renderer.setIndexChunkSlots(chunkSlots);
        renderer.setClipRegion(Rect(Point(0, 0), gweni::test::viewSize()));
        renderer.begin();
        renderer.end();
    }
};

}//namespace

TEST_CASE("Primitive draws: a loop of glDrawElements draws what glMultiDrawElements draws")
{
    Guard guard;
    OpenGLCore &renderer=gweni::test::glRenderer();
    const Rect view(Point(0, 0), gweni::test::viewSize());
    constexpr int columns=20;
    constexpr int cells=200;

    // desktop GL 3.3 has glMultiDrawElements, and the GLES tree's llvmpipe
    // context GL_EXT_multi_draw_arrays, so the shared renderer uses it unless
    // GWENI_NO_MULTI_DRAW asked for the loop
    REQUIRE(renderer.glCapabilities().multiDraw);
    renderer.setUseMultiDraw(true);
    REQUIRE(renderer.useMultiDraw());

    renderer.begin();
    for(int i=0; i<cells; ++i)
    {
        size_t id=0;
        const Rect cell((i%columns)*12+4, (i/columns)*12+4, 10, 10);

        renderer.generatePrimitive(id);
        guard.ids.push_back(id);
        // each clipped to its own left 7 columns, so a slot that read another's
        // clip box would draw differently
        renderer.setClipRegion(Rect(cell.x, cell.y, 7, cell.h));
        renderer.setDrawColor(Color((unsigned char)(i*37), (unsigned char)(255-i), (unsigned char)(i*11), 255));
        renderer.drawFilledRect(id, cell, 1);
    }
    // runs of 1 to 5 hidden between runs of 3 shown: gaps of 1 merged, the
    // others ranges of their own, past the cap of a chunk
    for(int i=0, gap=1; i+3 < cells; i+=3+gap, gap=gap%5+1)
    {
        for(int j=0; (j<gap) && (i+3+j < cells); ++j)
            renderer.hidePrimitive(guard.ids[size_t(i+3+j)]);
    }
    renderer.end();

    for(size_t chunkSlots:{ OpenGLCore::kSlotsPerChunk, size_t(7) })
    {
        CAPTURE(chunkSlots);
        renderer.setIndexChunkSlots(chunkSlots);
        renderer.setUseMultiDraw(true);

        const std::vector<uint8_t> multi=drawFrame();
        const OpenGLCore::FrameStats multiStats=renderer.getFrameStats();

        renderer.setUseMultiDraw(false);
        CHECK_FALSE(renderer.useMultiDraw());

        const std::vector<uint8_t> loop=drawFrame();
        const OpenGLCore::FrameStats &loopStats=renderer.getFrameStats();

        CHECK(loop == multi);
        CHECK(loopStats.drawCalls == multiStats.drawCalls);
        CHECK(loopStats.primitiveRanges == multiStats.primitiveRanges);
        CHECK(loopStats.primitiveSlotsDrawn == multiStats.primitiveSlotsDrawn);
        CHECK(loopStats.uploadCalls == 0);
        // several ranges in a chunk, the hidden runs left out
        CHECK(multiStats.primitiveRanges > multiStats.drawCalls-multiStats.textDraws);
    }

    // and the cells are drawn, not an empty frame: every shown cell's left part
    const std::vector<uint8_t> frame=drawFrame();
    size_t ink=0;

    for(size_t i=0; i < frame.size(); i+=4)
    {
        if((frame[i] != background.r) || (frame[i+1] != background.g) || (frame[i+2] != background.b))
            ++ink;
    }
    CHECK(ink > 50*7*10);
}

TEST_SUITE_END();
