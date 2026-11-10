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
 *  pixels of one chunk, one draw per chunk with a slot that shows, each slot
 *  clipped by its own box.
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
 *  primitive draw per chunk with a slot that shows. The text runs are clipped
 *  and laid out in two atlases; one is hidden and another shrunk in place, and
 *  they draw what a fresh layout of only what is shown draws. Hiding, showing
 *  and shrinking upload no index, chunks of 3 and 1 glyph slots draw the same
 *  pixels, and at 1 there is one text draw per glyph shown.
 *
 *  Only the primitive slots that can show are drawn (#431, #508): a chunk none
 *  of whose slots has a style other than 0 is not drawn, and a drawn chunk's
 *  draw is its runs of slots that can show, a gap of fewer than
 *  primitiveRangeGap() slots merged and at most maxPrimitiveRanges() ranges,
 *  FrameStats::primitiveSlotsDrawn and primitiveRanges being the slots and the
 *  ranges drawn, as expectedDraws works them out apart from the renderer. On a
 *  renderer of its own, whose mirror holds only the case's slots: three chunks
 *  of 1x1 rects with the middle chunk hidden draw two chunks, and one slot
 *  shown again brings it back as one slot in one range; a chunk's hidden tail
 *  is left out of its draw; the top ids of a large set released draw the
 *  remaining live extent, not the peak; the counts follow a buffer regrow and
 *  a new chunk size; a settled frame uploads nothing and builds no chunk's
 *  ranges. Every frame draws the pixels of the same mirror drawn whole
 *  (setDrawEveryPrimitiveSlot).
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>

#include <algorithm>
#include <set>
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
    // the slots reach into the second chunk, which draws them (and what an
    // earlier case left showing there) in ranges ending at the highest of them;
    // the first holds the ids taken on the way, never drawn, so it draws only if
    // an earlier case left a slot showing there
    const size_t secondReach=*std::max_element(high.begin(), high.end())-SlotRenderer::kSlotsPerChunk+1;
    const size_t slotsDrawn=renderer.getFrameStats().primitiveSlotsDrawn;

    CHECK(primitiveDraws() >= 1);
    CHECK(primitiveDraws() <= 2);
    CHECK(slotsDrawn >= gridCells);
    if(primitiveDraws() == 1)
        CHECK(slotsDrawn <= secondReach);
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

    // one slot per draw: a draw per slot that shows, the grid's and any an
    // earlier case left showing, and no other
    renderer.setIndexChunkSlots(1);
    CHECK(renderer.indexChunkSlots() == 1);
    const Frame single=drawFrame([&](SlotRenderer &renderer) { drawGrid(renderer, ids); });
    const size_t shownCount=primitiveDraws();

    CHECK(single.pixels == whole.pixels);
    CHECK(shownCount >= ids.size());
    CHECK(renderer.getFrameStats().primitiveSlotsDrawn == shownCount);
    CHECK(wholeDraws >= 1);
    CHECK(wholeDraws <= shownCount);

    // 5 slots per chunk splits the grid's slots between chunks: a draw per chunk
    // holding one of them at least, and at most one per shown slot
    std::vector<size_t> gridChunks;

    for(size_t id:ids)
        gridChunks.push_back(id/5);
    gridChunks.erase(std::unique(gridChunks.begin(), gridChunks.end()), gridChunks.end());

    renderer.setIndexChunkSlots(5);
    const Frame five=drawFrame([&](SlotRenderer &renderer) { drawGrid(renderer, ids); });

    CHECK(five.pixels == whole.pixels);
    CHECK(primitiveDraws() >= gridChunks.size());
    CHECK(primitiveDraws() <= shownCount);

    // clamped to [1, kSlotsPerChunk]
    renderer.setIndexChunkSlots(0);
    CHECK(renderer.indexChunkSlots() == 1);
    renderer.setIndexChunkSlots(SlotRenderer::kSlotsPerChunk+1);
    CHECK(renderer.indexChunkSlots() == SlotRenderer::kSlotsPerChunk);
}

TEST_CASE("Index chunks: text split between chunks draws the same, hidden and shrunk runs nothing more")
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

namespace
{

//! A renderer of its own on the tests' context, drawing into their framebuffer,
//! so its mirror holds only the slots a case draws on it.
SlotRenderer *newOwnRenderer()
{
    // the renderer keeps a reference to its paths
    static gweni::platform::CurrentPath paths("resources");
    SlotRenderer *renderer=gweni::test::newRenderer(paths, Rect(Point(0, 0), gweni::test::viewSize()));

    renderer->init();
    return renderer;
}

//! A 1x1 rect per slot, its place and colour from the slot, so a slot that
//! drew in another's place or not at all changes the frame.
Rect pixelRect(size_t slot)
{
    const int width=gweni::test::viewSize().x;

    return Rect(static_cast<int>(slot%width), static_cast<int>((slot/width)%gweni::test::viewSize().y), 1, 1);
}

Color pixelColor(size_t slot)
{
    return Color(static_cast<unsigned char>(slot), static_cast<unsigned char>(slot>>8), 128, 255);
}

//! The primitive draws a mirror whose shown slots are `shown` makes: the
//! chunks holding one, and in each its runs of shown slots, a gap of fewer than
//! `gap` hidden slots merged, then the smallest gaps (the leftmost of equal
//! ones) merged until at most `maxRanges` are left; worked out here one gap at
//! a time, apart from the renderer's way.
struct Expected
{
    size_t draws=0;
    size_t ranges=0;
    size_t slots=0;
};

Expected expectedDraws(const std::set<size_t> &shown, size_t chunkSlots,
    size_t gap=SlotRenderer::kPrimitiveRangeGap, size_t maxRanges=SlotRenderer::kMaxPrimitiveRanges)
{
    Expected expected;
    // each chunk's runs [first, end)
    std::vector<std::pair<size_t, size_t>> runs;
    size_t chunk=SIZE_MAX;

    auto finishChunk=[&]()
    {
        if(runs.empty())
            return;
        while(runs.size() > maxRanges)
        {
            size_t smallest=0;

            for(size_t i=1; i+1 < runs.size(); ++i)
            {
                if(runs[i+1].first-runs[i].second < runs[smallest+1].first-runs[smallest].second)
                    smallest=i;
            }
            runs[smallest].second=runs[smallest+1].second;
            runs.erase(runs.begin()+std::ptrdiff_t(smallest+1));
        }
        ++expected.draws;
        expected.ranges+=runs.size();
        for(const auto &run:runs)
            expected.slots+=run.second-run.first;
        runs.clear();
    };

    for(size_t slot:shown)
    {
        if(slot/chunkSlots != chunk)
        {
            finishChunk();
            chunk=slot/chunkSlots;
        }
        if(!runs.empty() && (slot-runs.back().second < gap))
            runs.back().second=slot+1;
        else
            runs.emplace_back(slot, slot+1);
    }
    finishChunk();
    return expected;
}

//! One frame of renderer drawing `draw`, read back, then the same mirror drawn
//! whole, every slot of every chunk, which must give the same pixels. The
//! stats are the first frame's.
template<typename Draw>
SlotRenderer::FrameStats drawChecked(SlotRenderer &renderer, Draw &&draw, size_t mirrorSlots)
{
    gweni::test::clearView(background);
    renderer.begin();
    draw(renderer);
    renderer.end();

    const SlotRenderer::FrameStats stats=renderer.getFrameStats();
    std::vector<uint8_t> skipped;

    gweni::test::readView(skipped);

    renderer.setDrawEveryPrimitiveSlot(true);
    gweni::test::clearView(background);
    renderer.begin();
    renderer.end();
    renderer.setDrawEveryPrimitiveSlot(false);

    std::vector<uint8_t> whole;

    gweni::test::readView(whole);

    const SlotRenderer::FrameStats &wholeStats=renderer.getFrameStats();
    const size_t chunkSlots=renderer.indexChunkSlots();

    CHECK(skipped == whole);
    CHECK(wholeStats.uploadCalls == 0);
    CHECK(wholeStats.primitiveSlotsDrawn == mirrorSlots);
    CHECK(wholeStats.drawCalls-wholeStats.textDraws == (mirrorSlots+chunkSlots-1)/chunkSlots);
    CHECK(wholeStats.primitiveRanges == wholeStats.drawCalls-wholeStats.textDraws);
    return stats;
}

void checkDraws(const SlotRenderer::FrameStats &stats, const std::set<size_t> &shown, size_t chunkSlots)
{
    const Expected expected=expectedDraws(shown, chunkSlots);

    CHECK(stats.drawCalls-stats.textDraws == expected.draws);
    CHECK(stats.primitiveRanges == expected.ranges);
    CHECK(stats.primitiveSlotsDrawn == expected.slots);
}

//! The slots a mirror holds: up to the highest id generated on it
size_t mirrorSlotsFor(const std::vector<size_t> &ids)
{
    return *std::max_element(ids.begin(), ids.end())+1;
}

}//namespace

TEST_CASE("Index chunks: a chunk with nothing shown is not drawn, and a drawn one draws its runs of shown slots")
{
    SlotRenderer *renderer=newOwnRenderer();
    const size_t chunkSlots=SlotRenderer::kSlotsPerChunk;

    renderer->setIndexChunkSlots(chunkSlots);

    // ids are process-wide and handed out lowest first: take them until one lies
    // past the third chunk, so every free id of the three chunks is ours (ids
    // held elsewhere stay style 0 in this mirror), then give that one back
    std::vector<size_t> ids;
    size_t mirrorSlots=0;

    for(;;)
    {
        size_t id=0;

        renderer->generatePrimitive(id);
        mirrorSlots=std::max(mirrorSlots, id+1);
        if(id >= 3*chunkSlots)
        {
            renderer->releasePrimitive(id);
            break;
        }
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());

    std::set<size_t> shown(ids.begin(), ids.end());

    REQUIRE(ids.size() > 2*chunkSlots);

    auto drawAll=[&](SlotRenderer &renderer)
    {
        for(size_t id:ids)
        {
            renderer.setDrawColor(pixelColor(id));
            renderer.drawFilledRect(id, pixelRect(id), 1);
        }
    };

    SlotRenderer::FrameStats stats=drawChecked(*renderer, drawAll, mirrorSlots);

    CHECK(stats.drawCalls-stats.textDraws == 3);
    checkDraws(stats, shown, chunkSlots);

    // every slot of the middle chunk hidden: one draw less, the other two as they were
    const size_t allDrawn=stats.primitiveSlotsDrawn;
    std::vector<size_t> middle;

    for(size_t id:ids)
    {
        if((id >= chunkSlots) && (id < 2*chunkSlots))
            middle.push_back(id);
    }

    stats=drawChecked(*renderer, [&](SlotRenderer &renderer)
    {
        for(size_t id:middle)
            renderer.hidePrimitive(id);
    }, mirrorSlots);
    for(size_t id:middle)
        shown.erase(id);
    CHECK(stats.drawCalls-stats.textDraws == 2);
    CHECK(stats.primitiveSlotsDrawn <= allDrawn-middle.size());
    checkDraws(stats, shown, chunkSlots);

    // one slot shown again brings its chunk back, as that one slot in one range
    const size_t again=middle[100];
    const size_t middleHiddenDrawn=stats.primitiveSlotsDrawn;
    const size_t middleHiddenRanges=stats.primitiveRanges;

    stats=drawChecked(*renderer, [&](SlotRenderer &renderer) { renderer.showPrimitive(1, again); }, mirrorSlots);
    shown.insert(again);
    CHECK(stats.drawCalls-stats.textDraws == 3);
    CHECK(stats.primitiveSlotsDrawn == middleHiddenDrawn+1);
    CHECK(stats.primitiveRanges == middleHiddenRanges+1);
    checkDraws(stats, shown, chunkSlots);

    // the last slots of the first chunk hidden: its ranges end at its last shown one
    std::vector<size_t> tail;

    for(size_t id:ids)
    {
        if(id < chunkSlots)
            tail.push_back(id);
    }
    tail.erase(tail.begin(), tail.end()-50);

    const size_t tailBefore=stats.primitiveSlotsDrawn;

    stats=drawChecked(*renderer, [&](SlotRenderer &renderer)
    {
        for(size_t id:tail)
            renderer.hidePrimitive(id);
    }, mirrorSlots);
    for(size_t id:tail)
        shown.erase(id);
    CHECK(stats.drawCalls-stats.textDraws == 3);
    CHECK(stats.primitiveSlotsDrawn <= tailBefore-50);
    checkDraws(stats, shown, chunkSlots);

    // the middle chunk's one shown slot hidden again: it is not drawn
    stats=drawChecked(*renderer, [&](SlotRenderer &renderer) { renderer.hidePrimitive(again); }, mirrorSlots);
    shown.erase(again);
    CHECK(stats.drawCalls-stats.textDraws == 2);
    checkDraws(stats, shown, chunkSlots);

    // a settled frame uploads nothing, builds no chunk's ranges and draws the same
    renderer->begin();
    renderer->end();
    CHECK(renderer->getFrameStats().primitiveSlotsWritten == 0);
    CHECK(renderer->getFrameStats().uploadCalls == 0);
    CHECK(renderer->getFrameStats().primitiveRangeBuilds == 0);
    checkDraws(renderer->getFrameStats(), shown, chunkSlots);

    // another chunk size counts the same shown slots in its own chunks
    renderer->setIndexChunkSlots(1000);
    stats=drawChecked(*renderer, [](SlotRenderer &) {}, mirrorSlots);
    checkDraws(stats, shown, 1000);
    CHECK(stats.uploadCalls == 0);

    // and keeps them as slots are hidden and shown
    stats=drawChecked(*renderer, [&](SlotRenderer &renderer)
    {
        for(size_t id=3000; id<5000; ++id)
            renderer.hidePrimitive(id);
        renderer.showPrimitive(1, middle[5]);
    }, mirrorSlots);
    for(size_t id=3000; id<5000; ++id)
        shown.erase(id);
    shown.insert(middle[5]);
    checkDraws(stats, shown, 1000);

    renderer->setIndexChunkSlots(chunkSlots);
    stats=drawChecked(*renderer, [](SlotRenderer &) {}, mirrorSlots);
    checkDraws(stats, shown, chunkSlots);

    for(size_t id:ids)
        renderer->releasePrimitive(id);
    renderer->begin();
    renderer->end();
    CHECK(renderer->getFrameStats().primitiveSlotsDrawn == 0);
    CHECK(renderer->getFrameStats().drawCalls == 0);
    delete renderer;
}

TEST_CASE("Index chunks: releasing the top slots draws the live extent, not the peak, across a regrow")
{
    SlotRenderer *renderer=newOwnRenderer();
    const size_t chunkSlots=SlotRenderer::kSlotsPerChunk;

    renderer->setIndexChunkSlots(chunkSlots);

    std::vector<size_t> ids;

    auto generate=[&](size_t count)
    {
        for(size_t i=0; i<count; ++i)
        {
            size_t id=0;

            renderer->generatePrimitive(id);
            ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());
    };
    auto drawIds=[&](size_t from)
    {
        return [&ids, from](SlotRenderer &renderer)
        {
            for(size_t i=from; i<ids.size(); ++i)
            {
                renderer.setDrawColor(pixelColor(ids[i]));
                renderer.drawFilledRect(ids[i], pixelRect(ids[i]), 1);
            }
        };
    };

    generate(1000);

    SlotRenderer::FrameStats stats=drawChecked(*renderer, drawIds(0), mirrorSlotsFor(ids));
    std::set<size_t> shown(ids.begin(), ids.end());

    CHECK(stats.bufferRegrows > 0);
    checkDraws(stats, shown, chunkSlots);

    // many more past the vertex buffer's capacity: a regrow, counted the same
    const size_t before=ids.size();

    generate(24000);
    stats=drawChecked(*renderer, drawIds(0), mirrorSlotsFor(ids));
    shown.insert(ids.begin(), ids.end());
    CHECK(stats.bufferRegrows > 0);
    CHECK(ids.size() == before+24000);
    checkDraws(stats, shown, chunkSlots);
    CHECK(stats.drawCalls-stats.textDraws == expectedDraws(shown, chunkSlots).draws);

    // the top 20000 released: the draws stop at the highest live slot
    const size_t peak=mirrorSlotsFor(ids);
    const std::vector<size_t> top(ids.end()-20000, ids.end());

    stats=drawChecked(*renderer, [&](SlotRenderer &renderer)
    {
        for(size_t id:top)
            renderer.releasePrimitive(id);
    }, peak);
    ids.resize(ids.size()-20000);
    for(size_t id:top)
        shown.erase(id);
    checkDraws(stats, shown, chunkSlots);
    // the live extent: up to the highest id still drawn, and less than the peak
    CHECK(stats.primitiveSlotsDrawn <= ids.back()+1);
    CHECK(stats.primitiveSlotsDrawn < peak);

    // a settled frame uploads nothing, builds no chunk's ranges and draws the same
    renderer->begin();
    renderer->end();
    CHECK(renderer->getFrameStats().primitiveSlotsWritten == 0);
    CHECK(renderer->getFrameStats().uploadCalls == 0);
    CHECK(renderer->getFrameStats().primitiveRangeBuilds == 0);
    checkDraws(renderer->getFrameStats(), shown, chunkSlots);

    for(size_t id:ids)
        renderer->releasePrimitive(id);
    renderer->begin();
    renderer->end();
    CHECK(renderer->getFrameStats().primitiveSlotsDrawn == 0);
    delete renderer;
}

TEST_CASE("Index chunks: a hidden run inside a chunk whose last slot shows is left out of its draw")
{
    SlotRenderer *renderer=newOwnRenderer();
    const size_t chunkSlots=SlotRenderer::kSlotsPerChunk;
    constexpr size_t window=1000;

    renderer->setIndexChunkSlots(chunkSlots);
    REQUIRE(renderer->primitiveRangeGap() == 2);
    REQUIRE(renderer->maxPrimitiveRanges() == 16);

    // ids are process-wide, so some may be held elsewhere: take them until the
    // last `window` are consecutive and inside one chunk; those alone are drawn,
    // every other slot of this mirror staying style 0
    std::vector<size_t> ids;
    size_t first=0;

    for(;;)
    {
        size_t id=0;

        renderer->generatePrimitive(id);
        ids.push_back(id);
        if((ids.size() >= window) && (ids.back()-ids[ids.size()-window] == window-1)
            && (ids[ids.size()-window]/chunkSlots == ids.back()/chunkSlots))
        {
            first=ids[ids.size()-window];
            break;
        }
        REQUIRE(ids.size() < 4*chunkSlots);
    }
    std::sort(ids.begin(), ids.end());

    const size_t mirrorSlots=mirrorSlotsFor(ids);
    std::set<size_t> shown;

    SlotRenderer::FrameStats stats=drawChecked(*renderer, [&](SlotRenderer &renderer)
    {
        for(size_t slot=first; slot<first+window; ++slot)
        {
            renderer.setDrawColor(pixelColor(slot));
            renderer.drawFilledRect(slot, pixelRect(slot), 1);
        }
    }, mirrorSlots);
    for(size_t slot=first; slot<first+window; ++slot)
        shown.insert(slot);
    CHECK(stats.drawCalls-stats.textDraws == 1);
    CHECK(stats.primitiveRanges == 1);
    CHECK(stats.primitiveSlotsDrawn == window);
    CHECK(stats.primitiveRangeBuilds == 1);

    // 200 slots hidden in the middle, the last one still shown: two ranges
    // around them, where the draw up to the last shown slot drew all 1000
    stats=drawChecked(*renderer, [&](SlotRenderer &renderer)
    {
        for(size_t slot=first+300; slot<first+500; ++slot)
            renderer.hidePrimitive(slot);
    }, mirrorSlots);
    for(size_t slot=first+300; slot<first+500; ++slot)
        shown.erase(slot);
    CHECK(stats.drawCalls-stats.textDraws == 1);
    CHECK(stats.primitiveRanges == 2);
    CHECK(stats.primitiveSlotsDrawn == window-200);
    checkDraws(stats, shown, chunkSlots);

    // a gap of one slot (fewer than primitiveRangeGap()) is drawn with its runs
    stats=drawChecked(*renderer, [&](SlotRenderer &renderer) { renderer.hidePrimitive(first+100); }, mirrorSlots);
    shown.erase(first+100);
    CHECK(stats.primitiveRanges == 2);
    CHECK(stats.primitiveSlotsDrawn == window-200);
    CHECK(stats.primitiveRangeBuilds == 1);
    checkDraws(stats, shown, chunkSlots);

    // 20 more gaps, of 2 to 21 slots, each followed by 5 shown: 22 runs, past
    // maxPrimitiveRanges(), so the 6 smallest gaps (2..7) are drawn with their
    // runs and 16 ranges are left, the gaps of 8..21 and the 200 not drawn
    stats=drawChecked(*renderer, [&](SlotRenderer &renderer)
    {
        size_t slot=first+520;

        for(size_t gap=2; gap<=21; ++gap)
        {
            for(size_t i=0; i<gap; ++i, ++slot)
            {
                renderer.hidePrimitive(slot);
                shown.erase(slot);
            }
            slot+=5;
        }
        REQUIRE(slot == first+850);
    }, mirrorSlots);
    CHECK(stats.drawCalls-stats.textDraws == 1);
    CHECK(stats.primitiveRanges == 16);
    CHECK(stats.primitiveSlotsDrawn == window-200-(8+21)*14/2);
    CHECK(stats.primitiveRangeBuilds == 1);
    checkDraws(stats, shown, chunkSlots);

    const SlotRenderer::FrameStats capped=stats;

    // a settled frame builds no range list and draws the same
    renderer->begin();
    renderer->end();
    CHECK(renderer->getFrameStats().uploadCalls == 0);
    CHECK(renderer->getFrameStats().primitiveRangeBuilds == 0);
    CHECK(renderer->getFrameStats().primitiveRanges == capped.primitiveRanges);
    CHECK(renderer->getFrameStats().primitiveSlotsDrawn == capped.primitiveSlotsDrawn);

    // nor does a frame that redraws a shown slot in another colour, which
    // changes no slot's visibility
    stats=drawChecked(*renderer, [&](SlotRenderer &renderer)
    {
        renderer.setDrawColor(Color(255, 255, 255, 255));
        renderer.drawFilledRect(first+10, pixelRect(first+10), 1);
    }, mirrorSlots);
    CHECK(stats.primitiveSlotsWritten == 1);
    CHECK(stats.primitiveRangeBuilds == 0);
    CHECK(stats.primitiveRanges == capped.primitiveRanges);
    CHECK(stats.primitiveSlotsDrawn == capped.primitiveSlotsDrawn);

    // showing the merged one-slot gap again builds the chunk's ranges again,
    // the same ones
    stats=drawChecked(*renderer, [&](SlotRenderer &renderer) { renderer.showPrimitive(1, first+100); }, mirrorSlots);
    shown.insert(first+100);
    CHECK(stats.primitiveRangeBuilds == 1);
    CHECK(stats.primitiveRanges == capped.primitiveRanges);
    CHECK(stats.primitiveSlotsDrawn == capped.primitiveSlotsDrawn);
    checkDraws(stats, shown, chunkSlots);

    for(size_t id:ids)
        renderer->releasePrimitive(id);
    renderer->begin();
    renderer->end();
    CHECK(renderer->getFrameStats().primitiveSlotsDrawn == 0);
    delete renderer;
}
