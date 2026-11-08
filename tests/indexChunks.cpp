/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's chunked draws: every primitive and glyph slot is a quad in a
 *  static 16-bit index buffer, and the slots are drawn in chunks of at most
 *  SlotRenderer::kSlotsPerChunk, each with its vertex attributes pointed at its
 *  first slot. A slot past 16383 (which a 16-bit index of slot*4 cannot reach)
 *  draws its own vertexes, and a chunk size of a few slots draws exactly the
 *  pixels of one chunk, one draw per chunk, each slot clipped by its own box.
 *  Text runs split between chunks draw as one, and a hidden run or a shrunk
 *  run's tail, which no index mirror blanks any more, draws nothing. Drawn
 *  offscreen and read back.
 *
 *  The renderer is shared with the other cases, so each case puts the clip
 *  region and the chunk size back when it ends.
 *
 *  The rects are a grid, each clipped to its left half, in slots past 16383
 *  (the second index chunk) and at chunk sizes of 1 and 5
 *  (setIndexChunkSlots): every chunk size gives the same pixels, with one
 *  primitive draw per chunk. The text runs are clipped and laid out in two
 *  atlases; one is hidden and another shrunk in place, and they draw what a
 *  fresh layout of only what is shown draws. Hiding, showing and shrinking
 *  upload no index, chunks of 3 and 1 glyph slots draw the same pixels, and at
 *  1 there is one text draw per glyph shown.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

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
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    gweni::test::clearView(background);

    renderer.begin();
    draw(renderer);
    renderer.end();

    Frame frame;

    gweni::test::readView(frame.pixels);
    return frame;
}

//! Puts the clip region back to the whole view and the chunk size back to what it
//! was, so the next case draws as before.
struct Guard
{
    size_t chunkSlots=gweni::test::gpuRenderer().indexChunkSlots();

    ~Guard()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setClipRegion(Rect(Point(0, 0), gweni::test::viewSize()));
        renderer.setIndexChunkSlots(chunkSlots);
    }
};

//! Primitive ids for one test case, released (and that flushed) when it ends.
struct Primitives
{
    ~Primitives()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

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

//! Text ids for one test case, released (and that flushed) when it ends.
struct Texts
{
    ~Texts()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

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
void drawGrid(SlotRenderer &renderer, const std::vector<size_t> &ids)
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
    const SlotRenderer::FrameStats &stats=gweni::test::gpuRenderer().getFrameStats();

    return stats.drawCalls-stats.textDraws;
}

}//namespace

TEST_CASE("Index chunks: slots past 16383 draw their own vertexes and clip boxes")
{
    Guard guard;
    Primitives prims;
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.setIndexChunkSlots(SlotRenderer::kSlotsPerChunk);

    // ids are handed out lowest first, so take them until gridCells of them lie
    // past the first chunk and past what a 16-bit slot*4 index reaches (16383)
    std::vector<size_t> high;

    while(high.size() < gridCells)
    {
        const size_t id=prims.generate();

        if(id > 16383)
            high.push_back(id);
    }

    const Frame frame=drawFrame([&](SlotRenderer &renderer) { drawGrid(renderer, high); });

    checkGrid(frame, gridCells);
    // the slots reach into the second chunk
    CHECK(primitiveDraws() == 2);
}

TEST_CASE("Index chunks: a few slots per chunk draw the same pixels, one draw per chunk")
{
    Guard guard;
    Primitives prims;
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    std::vector<size_t> ids;

    for(size_t i=0; i<gridCells; ++i)
        ids.push_back(prims.generate());
    std::sort(ids.begin(), ids.end());

    renderer.setIndexChunkSlots(SlotRenderer::kSlotsPerChunk);
    const Frame whole=drawFrame([&](SlotRenderer &renderer) { drawGrid(renderer, ids); });
    const size_t wholeDraws=primitiveDraws();

    checkGrid(whole, gridCells);

    // one slot per draw: the draw count is the slot count, which earlier cases
    // (the one past 16383 above) may have grown past one chunk
    renderer.setIndexChunkSlots(1);
    CHECK(renderer.indexChunkSlots() == 1);
    const Frame single=drawFrame([&](SlotRenderer &renderer) { drawGrid(renderer, ids); });
    const size_t slotCount=primitiveDraws();

    CHECK(single.pixels == whole.pixels);
    CHECK(slotCount > ids.back());
    CHECK(wholeDraws == (slotCount+SlotRenderer::kSlotsPerChunk-1)/SlotRenderer::kSlotsPerChunk);

    // 5 slots per chunk splits the grid's slots between chunks
    renderer.setIndexChunkSlots(5);
    const Frame five=drawFrame([&](SlotRenderer &renderer) { drawGrid(renderer, ids); });

    CHECK(five.pixels == whole.pixels);
    CHECK(primitiveDraws() == (slotCount+4)/5);

    // clamped to [1, kSlotsPerChunk]
    renderer.setIndexChunkSlots(0);
    CHECK(renderer.indexChunkSlots() == 1);
    renderer.setIndexChunkSlots(SlotRenderer::kSlotsPerChunk+1);
    CHECK(renderer.indexChunkSlots() == SlotRenderer::kSlotsPerChunk);
}

// needs_text: a Vulkan run leaves it out until renderer::Vulkan draws text (#372)
TEST_CASE("Index chunks: text split between chunks draws the same, hidden and shrunk runs nothing more" * doctest::test_suite("needs_text"))
{
    Guard guard;
    Texts texts;
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Color white(255, 255, 255, 255);
    // two atlases: a coverage size and (by default) an SDF one
    const gweni::Font small=openSansAt(11.0f);
    const gweni::Font large=openSansAt(24.0f);
    // no spaces: each is one glyph slot per character
    const gweni::String straddling="Straddling";
    const gweni::String wide="WWWWWWWWWW";
    const gweni::String shrunk="WW";
    // each run cut by a clip region of its own, so a glyph that read another
    // slot's clip box (a chunk's slotBase left out) draws differently
    const Rect clipA(10, 0, 40, 60);
    const Rect clipC(10, 100, 35, 60);
    const Rect view(Point(0, 0), gweni::test::viewSize());

    // what other cases left drawing, one draw per glyph slot at one slot a chunk
    renderer.setIndexChunkSlots(1);
    drawFrame([](SlotRenderer &) {});
    const size_t baseSingle=renderer.getFrameStats().textDraws;

    renderer.setIndexChunkSlots(SlotRenderer::kSlotsPerChunk);

    const size_t a=texts.generate();
    const size_t b=texts.generate();
    const size_t c=texts.generate();

    drawFrame([&](SlotRenderer &renderer)
    {
        renderer.setDrawColor(white);
        renderer.setClipRegion(clipA);
        renderer.renderText(a, small, Point(10, 30), 1, straddling);
        renderer.setClipRegion(view);
        renderer.renderText(b, small, Point(10, 80), 1, "Hidden");
        renderer.setClipRegion(clipC);
        renderer.renderText(c, large, Point(10, 130), 1, wide);
    });

    // b hidden and c shrunk in place: its tail keeps its old glyphs' vertexes
    const Frame whole=drawFrame([&](SlotRenderer &renderer)
    {
        renderer.hideText(b);
        renderer.setDrawColor(white);
        renderer.setClipRegion(clipC);
        renderer.renderText(c, large, Point(10, 130), 1, shrunk);
    });

    // c's two glyphs' vertexes and nothing else: no indexes to upload
    CHECK(renderer.getFrameStats().uploadCalls == 1);
    CHECK(renderer.getFrameStats().uploadBytes == shrunk.size()*4*sizeof(ftgl::glyph_vertex_t));

    // a fresh layout of only what is shown, on new ids, draws the same
    {
        Texts fresh;
        const size_t d=fresh.generate();
        const size_t e=fresh.generate();

        const Frame reference=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.hideText(a);
            renderer.hideText(c);
            renderer.setDrawColor(white);
            renderer.setClipRegion(clipA);
            renderer.renderText(d, small, Point(10, 30), 1, straddling);
            renderer.setClipRegion(clipC);
            renderer.renderText(e, large, Point(10, 130), 1, shrunk);
        });

        CHECK(reference.pixels == whole.pixels);
    }

    // and that is some text, not an empty frame
    bool ink=false;

    for(int y=0; (y < gweni::test::viewSize().y) && !ink; ++y)
    {
        for(int x=0; (x < gweni::test::viewSize().x) && !ink; ++x)
            ink=!whole.is(x, y, background);
    }
    CHECK(ink);

    // hiding and showing writes nothing either
    const Frame shown=drawFrame([&](SlotRenderer &renderer)
    {
        renderer.showText(a);
        renderer.showText(c);
    });

    CHECK(shown.pixels == whole.pixels);
    CHECK(renderer.getFrameStats().uploadCalls == 0);

    // three slots a chunk: every run of more than three glyphs is split between
    // chunks, and the draws between atlases and chunks
    renderer.setIndexChunkSlots(3);
    const Frame three=drawFrame([](SlotRenderer &) {});

    CHECK(three.pixels == whole.pixels);
    CHECK(renderer.getFrameStats().uploadCalls == 0);

    // one slot a chunk: one draw per (atlas, chunk) with something to draw is one
    // per glyph shown, b's and c's tail not among them
    renderer.setIndexChunkSlots(1);
    const Frame single=drawFrame([](SlotRenderer &) {});

    CHECK(single.pixels == whole.pixels);
    CHECK(renderer.getFrameStats().textDraws == baseSingle+straddling.size()+shrunk.size());
}
