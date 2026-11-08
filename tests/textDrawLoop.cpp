/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's two ways of drawing an atlas's text: one glMultiDrawElements per
 *  chunk of its runs (desktop GL), or, where the context has no multi-draw
 *  (GLES 3.0, WebGL 2), the same ranges drawn one glDrawElements each
 *  (setMultiDrawText(false), GWENI_NO_MULTI_DRAW=1). Both must draw the same
 *  pixels and count the same FrameStats: the draws are one per (atlas, chunk)
 *  either way, and switching uploads nothing.
 *
 *  The runs are laid out in two atlases (a coverage size and an SDF one), with
 *  a hidden run between two shown ones of the same atlas, so its draw has two
 *  ranges; at three slots a chunk every run is split between chunks too.
 *  Drawn offscreen and read back. The renderer is shared with the other cases,
 *  so each case puts the multi-draw, the clip region and the chunk size back.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::OpenGLCore;

const Color background(0, 0, 64, 255);

std::vector<uint8_t> drawFrame()
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Point size=gweni::test::viewSize();
    std::vector<uint8_t> pixels;

    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    renderer.begin();
    renderer.end();
    gweni::capture::readFramebufferRgba(size.x, size.y, pixels);
    return pixels;
}

//! Puts the multi-draw, the clip region and the chunk size back as they were.
struct Guard
{
    bool multiDraw=gweni::test::gpuRenderer().multiDrawText();
    size_t chunkSlots=gweni::test::gpuRenderer().indexChunkSlots();

    ~Guard()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        renderer.setMultiDrawText(multiDraw);
        renderer.setIndexChunkSlots(chunkSlots);
        renderer.setClipRegion(Rect(Point(0, 0), gweni::test::viewSize()));
    }
};

//! Text ids for one test case, released (and that flushed) when it ends.
struct Texts
{
    ~Texts()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        for(size_t id:ids)
            renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
    }

    size_t generate()
    {
        size_t id=0;

        gweni::test::gpuRenderer().generateTextPrimitive(id);
        ids.push_back(id);
        return id;
    }

    std::vector<size_t> ids;
};

gweni::Font openSansAt(float size)
{
    gweni::Font font=gweni::test::openSans();

    font.size=size;
    return font;
}

}//namespace

TEST_CASE("Text draws: a loop of glDrawElements draws what glMultiDrawElements draws")
{
    Guard guard;
    Texts texts;
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Color white(255, 255, 255, 255);
    const gweni::Font small=openSansAt(11.0f);
    const gweni::Font large=openSansAt(24.0f);
    const Rect view(Point(0, 0), gweni::test::viewSize());

    // desktop GL 3.3 has glMultiDrawElements, so the shared renderer uses it
    // unless GWENI_NO_MULTI_DRAW asked for the loop
    REQUIRE(renderer.glCapabilities().multiDraw);
    renderer.setMultiDrawText(true);
    REQUIRE(renderer.multiDrawText());

    const size_t first=texts.generate();
    const size_t hidden=texts.generate();
    const size_t last=texts.generate();
    const size_t other=texts.generate();

    renderer.begin();
    renderer.setDrawColor(white);
    renderer.setClipRegion(view);
    renderer.renderText(first, small, Point(10, 30), 1, "First run");
    renderer.renderText(hidden, small, Point(10, 60), 1, "Hidden run");
    renderer.renderText(last, small, Point(10, 90), 1, "Last run");
    // clipped, so a glyph that read another slot's clip box would draw differently
    renderer.setClipRegion(Rect(10, 100, 70, 60));
    renderer.renderText(other, large, Point(10, 130), 1, "Clipped");
    renderer.hideText(hidden);
    renderer.end();

    for(size_t chunkSlots:{ OpenGLCore::kSlotsPerChunk, size_t(3) })
    {
        CAPTURE(chunkSlots);
        renderer.setIndexChunkSlots(chunkSlots);
        renderer.setMultiDrawText(true);

        const std::vector<uint8_t> multi=drawFrame();
        const OpenGLCore::FrameStats multiStats=renderer.getFrameStats();

        renderer.setMultiDrawText(false);
        CHECK_FALSE(renderer.multiDrawText());

        const std::vector<uint8_t> loop=drawFrame();
        const OpenGLCore::FrameStats &loopStats=renderer.getFrameStats();

        CHECK(loop == multi);
        CHECK(loopStats.textDraws == multiStats.textDraws);
        CHECK(loopStats.drawCalls == multiStats.drawCalls);
        CHECK(loopStats.uploadCalls == 0);
        // two atlases, one with a range on each side of the hidden run, at least
        CHECK(multiStats.textDraws >= 2);
    }

    // and that is some text, not an empty frame
    const std::vector<uint8_t> frame=drawFrame();
    size_t ink=0;

    for(size_t i=0; i < frame.size(); i+=4)
    {
        if((frame[i] != background.r) || (frame[i+1] != background.g) || (frame[i+2] != background.b))
            ++ink;
    }
    CHECK(ink > 100);
}
