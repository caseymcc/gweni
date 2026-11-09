/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A released id holds what a never-written one does (issue #479). The id banks
 *  hand the lowest free id out first, so a control made after another is
 *  deleted gets that control's ids, and a skin that hides a slot and later shows
 *  it back without redrawing it (showPrimitive) would start from the dead
 *  control's bytes. So releasePrimitive zeroes the slot's vertexes, not only its
 *  style (the clip box, which every draw writes, is left): the reused id reads
 *  as all zero bytes in snapshotMirror(), as a slot the mirror has just grown by
 *  does, whether the slot was shown or already hidden when it was released, and
 *  a show of it draws nothing. The release frame writes that one slot, its
 *  vertexes alone (tests/slotsWritten.cpp). A released text id's
 *  run snapshots as a new one's: not hidden, no glyphs.
 *
 *  The banks are shared by every case in the process, so each case first takes
 *  every id already released (and gives them back at its end), so the id it
 *  releases is the lowest free one and comes back to it.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/primitiveIds.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 255, 255);
const Color fill(200, 120, 40, 255);

bool allZero(const std::vector<uint8_t> &bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](uint8_t b) { return b == 0; });
}

//! Pixels of the whole view that are not the background.
size_t drawnPixels()
{
    std::vector<uint8_t> pixels;
    size_t drawn=0;

    gweni::test::readView(pixels);
    for(size_t i=0; i+3<pixels.size(); i+=4)
    {
        if((pixels[i] != background.r) || (pixels[i+1] != background.g) || (pixels[i+2] != background.b))
            ++drawn;
    }
    return drawn;
}

void drawRect(SlotRenderer &renderer, size_t id)
{
    renderer.drawRoundedRect(id, Rect(20, 20, 60, 30), 4, 1, fill, fill, Color(255, 255, 255, 255), 3);
}

}//namespace

TEST_CASE("Released slots: a released primitive slot reads as a never-written one and shows nothing")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    std::vector<size_t> held(gweni::releasedPrimitiveIdCount());

    for(size_t &id:held)
        renderer.generatePrimitive(id);

    for(bool hiddenFirst:{false, true})
    {
        INFO("hidden before the release: ", hiddenFirst);

        size_t id=0;

        renderer.generatePrimitive(id);
        renderer.begin();
        drawRect(renderer, id);
        if(hiddenFirst)
            renderer.hidePrimitive(id);
        renderer.end();

        {
            const SlotRenderer::MirrorSnapshot drawn=renderer.snapshotMirror();

            REQUIRE(drawn.primitives.count(id) == 1);
            CHECK_FALSE(allZero(drawn.primitives.at(id)));
        }

        // the release frame writes that one slot
        renderer.begin();
        renderer.releasePrimitive(id);
        renderer.end();
        CHECK(renderer.getFrameStats().primitiveSlotsWritten == 1);

        // the lowest free id comes back, holding a never-written slot's bytes
        size_t reused=0;
        size_t fresh=0;

        renderer.generatePrimitive(reused);
        REQUIRE(reused == id);
        renderer.generatePrimitive(fresh);

        const SlotRenderer::MirrorSnapshot snapshot=renderer.snapshotMirror();

        REQUIRE(snapshot.primitives.count(reused) == 1);
        REQUIRE(snapshot.primitives.count(fresh) == 1);
        CHECK(allZero(snapshot.primitives.at(reused)));
        CHECK(snapshot.primitives.at(reused) == snapshot.primitives.at(fresh));

        // a skin showing the reused slot back without drawing it gets nothing on
        // screen, not the released rect
        gweni::test::clearView(background);
        renderer.begin();
        renderer.showPrimitive(4, reused);
        renderer.end();
        CHECK(drawnPixels() == 0);

        renderer.begin();
        renderer.releasePrimitive(fresh);
        renderer.releasePrimitive(reused);
        renderer.end();
    }

    renderer.begin();
    for(size_t id:held)
        renderer.releasePrimitive(id);
    renderer.end();
}

TEST_CASE("Released slots: a released text id's run snapshots as a new one's")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    std::vector<size_t> held(gweni::releasedTextIdCount());

    for(size_t &id:held)
        renderer.generateTextPrimitive(id);

    for(bool hiddenFirst:{false, true})
    {
        INFO("hidden before the release: ", hiddenFirst);

        size_t id=0;

        renderer.generateTextPrimitive(id);
        renderer.begin();
        renderer.setDrawColor(fill);
        renderer.renderText(id, gweni::test::openSans(), gweni::Point(10, 40), 5, "Item 12345");
        if(hiddenFirst)
            renderer.hideText(id);
        renderer.end();

        {
            const SlotRenderer::MirrorSnapshot drawn=renderer.snapshotMirror();

            REQUIRE(drawn.texts.count(id) == 1);
            CHECK(drawn.texts.at(id).size() > 1);
        }

        renderer.begin();
        renderer.releaseTextPrimitive(id);
        renderer.end();

        size_t reused=0;
        size_t fresh=0;

        renderer.generateTextPrimitive(reused);
        REQUIRE(reused == id);
        renderer.generateTextPrimitive(fresh);

        const SlotRenderer::MirrorSnapshot snapshot=renderer.snapshotMirror();

        REQUIRE(snapshot.texts.count(reused) == 1);
        REQUIRE(snapshot.texts.count(fresh) == 1);
        CHECK(snapshot.texts.at(reused) == std::vector<uint8_t>{0});
        CHECK(snapshot.texts.at(reused) == snapshot.texts.at(fresh));

        renderer.begin();
        renderer.releaseTextPrimitive(fresh);
        renderer.releaseTextPrimitive(reused);
        renderer.end();
    }

    renderer.begin();
    for(size_t id:held)
        renderer.releaseTextPrimitive(id);
    renderer.end();
}
