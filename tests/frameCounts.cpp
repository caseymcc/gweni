/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  renderer::Base::frameStats(), writtenPrimitiveIds() and writtenTextIds() on
 *  the shared SlotRenderer, on every backend: frameStats() is getFrameStats()'s
 *  slot counts with its uploads and atlas uploads added up, marked counted; the
 *  written ids are exactly the slots the flush counted, by id: one changed rect
 *  gives its primitive id alone, one changed label its text id with as many
 *  glyph slots as textSlotsWritten, both in one frame both, a settled frame
 *  neither, a hide or a release the id it touched (a release in the next
 *  frame), and a label relaid into new
 *  slots (it outgrew its old ones) its own id for every slot written.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <vector>

namespace
{

using gweni::Rect;
using gweni::renderer::SlotRenderer;
using gweni::renderer::WrittenText;

Rect wholeView()
{
    const gweni::Point size=gweni::test::viewSize();

    return Rect(0, 0, size.x, size.y);
}

//! One frame: begin(), the draws, end() (which flushes).
template<typename Draw>
void frame(Draw &&draw)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.begin();
    draw(renderer);
    renderer.end();
}

void emptyFrame()
{
    frame([](SlotRenderer &) {});
}

//! frameStats() agrees with getFrameStats(), and the written ids with its counts.
void checkAgrees(const SlotRenderer &renderer)
{
    const gweni::renderer::FrameCounts counts=renderer.frameStats();
    const SlotRenderer::FrameStats &stats=renderer.getFrameStats();

    CHECK(counts.counted);
    CHECK(counts.frameIndex == stats.frameIndex);
    CHECK(counts.primitiveSlotsWritten == stats.primitiveSlotsWritten);
    CHECK(counts.textSlotsWritten == stats.textSlotsWritten);
    CHECK(counts.uploadCalls == stats.uploadCalls+stats.atlasUploadCalls);
    CHECK(counts.uploadBytes == stats.uploadBytes+stats.atlasUploadBytes);

    const std::vector<size_t> &primitives=renderer.writtenPrimitiveIds();

    CHECK(primitives.size() == stats.primitiveSlotsWritten);
    for(size_t i=1; i < primitives.size(); ++i)
        CHECK(primitives[i-1] < primitives[i]);

    size_t glyphs=0;
    const std::vector<WrittenText> &texts=renderer.writtenTextIds();

    for(size_t i=0; i < texts.size(); ++i)
    {
        CHECK(texts[i].glyphSlots > 0);
        if(i > 0)
            CHECK(texts[i-1].textId < texts[i].textId);
        glyphs+=texts[i].glyphSlots;
    }
    CHECK(glyphs == stats.textSlotsWritten);
}

//! Puts the clip region and the draw colour back for the next case.
struct Guard
{
    ~Guard()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setClipRegion(wholeView());
        renderer.setDrawColor(gweni::Color(255, 255, 255, 255));
    }
};

}//namespace

TEST_CASE("Frame counts: the written ids are the slots a frame wrote, by id")
{
    Guard guard;
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const gweni::Font &font=gweni::test::openSans();
    std::vector<size_t> rects(3);
    size_t label=0;

    for(size_t &id : rects)
        renderer.generatePrimitive(id);
    renderer.generateTextPrimitive(label);
    renderer.setClipRegion(wholeView());
    renderer.setDrawColor(gweni::Color(200, 30, 40, 255));

    std::vector<Rect> places={Rect(10, 10, 40, 20), Rect(60, 10, 40, 20), Rect(110, 10, 40, 20)};
    gweni::String text="Counted";

    auto draw=[&](SlotRenderer &r)
    {
        for(size_t i=0; i < rects.size(); ++i)
            r.drawFilledRect(rects[i], places[i], 1);
        r.renderText(label, font, gweni::Point(10, 40), 2, text);
    };

    // the first frame writes everything; then settled
    frame(draw);
    checkAgrees(renderer);
    frame(draw);
    checkAgrees(renderer);
    REQUIRE(renderer.getFrameStats().primitiveSlotsWritten == 0);

    SUBCASE("a settled frame writes no id")
    {
        frame(draw);
        checkAgrees(renderer);
        CHECK(renderer.writtenPrimitiveIds().empty());
        CHECK(renderer.writtenTextIds().empty());
        CHECK(renderer.frameStats().primitiveSlotsWritten == 0);
        CHECK(renderer.frameStats().textSlotsWritten == 0);
    }

    SUBCASE("one changed rect is its id alone")
    {
        places[1]=Rect(60, 12, 40, 20);
        frame(draw);
        checkAgrees(renderer);
        CHECK(renderer.writtenPrimitiveIds() == std::vector<size_t>{rects[1]});
        CHECK(renderer.writtenTextIds().empty());
    }

    SUBCASE("one changed label is its text id, every glyph slot it wrote")
    {
        text="Changed";
        frame(draw);
        checkAgrees(renderer);
        CHECK(renderer.writtenPrimitiveIds().empty());
        REQUIRE(renderer.writtenTextIds().size() == 1);
        CHECK(renderer.writtenTextIds()[0].textId == label);
        CHECK(renderer.writtenTextIds()[0].glyphSlots == renderer.getFrameStats().textSlotsWritten);
        CHECK(renderer.writtenTextIds()[0].glyphSlots > 0);
    }

    SUBCASE("a rect and a label in one frame, both")
    {
        places[0]=Rect(12, 10, 40, 20);
        text="Both of them";
        frame(draw);
        checkAgrees(renderer);
        CHECK(renderer.writtenPrimitiveIds() == std::vector<size_t>{rects[0]});
        REQUIRE(renderer.writtenTextIds().size() == 1);
        CHECK(renderer.writtenTextIds()[0].textId == label);
    }

    SUBCASE("a label that outgrew its slots, laid out in new ones, is still its own id")
    {
        text="A string much longer than the first one was";
        frame(draw);
        checkAgrees(renderer);
        REQUIRE(renderer.writtenTextIds().size() == 1);
        CHECK(renderer.writtenTextIds()[0].textId == label);
    }

    SUBCASE("a hide and a release are the ids they touched")
    {
        frame([&](SlotRenderer &r) { r.hidePrimitive(rects[2]); });
        checkAgrees(renderer);
        CHECK(renderer.writtenPrimitiveIds() == std::vector<size_t>{rects[2]});

        frame([&](SlotRenderer &r) { r.hideText(label); });
        checkAgrees(renderer);
        CHECK(renderer.writtenPrimitiveIds().empty());
    }

    // drawn whole again, then released: a release clears (hides) the slot, which
    // the next flush writes under the released id
    frame(draw);
    for(size_t id : rects)
        renderer.releasePrimitive(id);
    renderer.releaseTextPrimitive(label);
    emptyFrame();
    checkAgrees(renderer);

    std::vector<size_t> sorted=rects;

    std::sort(sorted.begin(), sorted.end());
    CHECK(renderer.writtenPrimitiveIds() == sorted);
    emptyFrame();
    checkAgrees(renderer);
    CHECK(renderer.writtenPrimitiveIds().empty());
}
