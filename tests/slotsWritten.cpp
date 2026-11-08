/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore::FrameStats::primitiveSlotsWritten is the number of distinct
 *  primitive slots a frame's flush uploads, vertexes or clip box: a slot a
 *  hidePrimitive or showPrimitive changed counts as one a draw changed does, a
 *  hide or show that changes nothing counts nothing, a slot counts once however
 *  many calls touched it in the frame (two draws, a draw, a clip-box change and
 *  a hide), a clip-box change alone counts, and a releasePrimitive between
 *  frames counts in the next frame's flush. A hide uploads exactly that one
 *  slot's vertexes. A Selectable's hover-off frame, which hides the row's slot,
 *  writes exactly 1, as its hover-on frame does.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/selectable.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <vector>

namespace
{

using gweni::Rect;
using gweni::renderer::OpenGLCore;

//! A slot's vertexes: 4 vertexes of 32 bytes (OpenGLCore::Vertex)
const size_t SlotVertexBytes=4*32;

Rect wholeView()
{
    const gweni::Point size=gweni::test::viewSize();

    return Rect(0, 0, size.x, size.y);
}

//! One frame: begin(), the draws, end() (which flushes).
template<typename Draw>
const OpenGLCore::FrameStats &frame(Draw &&draw)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    renderer.begin();
    draw(renderer);
    renderer.end();
    return renderer.getFrameStats();
}

const OpenGLCore::FrameStats &emptyFrame()
{
    return frame([](OpenGLCore &) {});
}

//! Puts the clip region and the draw colour back for the next case.
struct Guard
{
    ~Guard()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        renderer.setClipRegion(wholeView());
        renderer.setDrawColor(gweni::Color(255, 255, 255, 255));
    }
};

}//namespace

TEST_CASE("Slots written: a hide or show counts once, a no-op hide or show not at all")
{
    Guard guard;
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    size_t id=0;

    renderer.generatePrimitive(id);
    renderer.setClipRegion(wholeView());
    renderer.setDrawColor(gweni::Color(200, 30, 40, 255));

    auto draw=[&](OpenGLCore &r) { r.drawFilledRect(id, Rect(10, 10, 40, 20), 1); };

    CHECK(frame(draw).primitiveSlotsWritten == 1);
    // settled: the same draw writes nothing
    CHECK(frame(draw).primitiveSlotsWritten == 0);
    CHECK(renderer.getFrameStats().uploadBytes == 0);

    // a hide in the frame: that one slot, and only its vertexes, uploaded
    const OpenGLCore::FrameStats &hidden=frame([&](OpenGLCore &r) { r.hidePrimitive(id); });

    CHECK(hidden.primitiveSlotsWritten == 1);
    CHECK(hidden.uploadBytes == SlotVertexBytes);

    // hiding a hidden slot changes nothing
    CHECK(frame([&](OpenGLCore &r) { r.hidePrimitive(id); }).primitiveSlotsWritten == 0);
    CHECK(renderer.getFrameStats().uploadBytes == 0);

    // shown again with its style (1, a fill)
    const OpenGLCore::FrameStats &shown=frame([&](OpenGLCore &r) { r.showPrimitive(1, id); });

    CHECK(shown.primitiveSlotsWritten == 1);
    CHECK(shown.uploadBytes == SlotVertexBytes);

    // showing a shown slot with its style changes nothing
    CHECK(frame([&](OpenGLCore &r) { r.showPrimitive(1, id); }).primitiveSlotsWritten == 0);

    // a hide between frames, outside begin()..end(), counts in the next flush
    renderer.hidePrimitive(id);
    CHECK(emptyFrame().primitiveSlotsWritten == 1);
    CHECK(emptyFrame().primitiveSlotsWritten == 0);

    renderer.releasePrimitive(id);
    emptyFrame();
}

TEST_CASE("Slots written: a slot touched by several calls in a frame counts once")
{
    Guard guard;
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    std::vector<size_t> ids(2);

    for(size_t &id : ids)
        renderer.generatePrimitive(id);
    renderer.setClipRegion(wholeView());
    renderer.setDrawColor(gweni::Color(200, 30, 40, 255));

    auto settle=[&]()
    {
        frame([&](OpenGLCore &r)
            {
                r.drawFilledRect(ids[0], Rect(10, 10, 40, 20), 1);
                r.drawFilledRect(ids[1], Rect(60, 10, 40, 20), 1);
            });
        REQUIRE(frame([&](OpenGLCore &r)
            {
                r.drawFilledRect(ids[0], Rect(10, 10, 40, 20), 1);
                r.drawFilledRect(ids[1], Rect(60, 10, 40, 20), 1);
            }).primitiveSlotsWritten == 0);
    };

    settle();

    SUBCASE("two draws that change it")
    {
        const OpenGLCore::FrameStats &stats=frame([&](OpenGLCore &r)
            {
                r.drawFilledRect(ids[0], Rect(12, 10, 40, 20), 1);
                r.drawFilledRect(ids[0], Rect(14, 10, 40, 20), 1);
            });

        CHECK(stats.primitiveSlotsWritten == 1);
    }

    SUBCASE("a draw with a new clip box, then a hide")
    {
        const OpenGLCore::FrameStats &stats=frame([&](OpenGLCore &r)
            {
                r.setClipRegion(Rect(0, 0, 30, 30));
                r.drawFilledRect(ids[0], Rect(12, 10, 40, 20), 1);
                r.hidePrimitive(ids[0]);
                r.setClipRegion(wholeView());
            });

        CHECK(stats.primitiveSlotsWritten == 1);
    }

    SUBCASE("a clip box changed alone, next to a slot whose vertexes changed")
    {
        const OpenGLCore::FrameStats &stats=frame([&](OpenGLCore &r)
            {
                r.drawFilledRect(ids[0], Rect(12, 10, 40, 20), 1);
                r.setClipRegion(Rect(0, 0, 80, 30));
                r.drawFilledRect(ids[1], Rect(60, 10, 40, 20), 1);
                r.setClipRegion(wholeView());
            });

        CHECK(stats.primitiveSlotsWritten == 2);
    }

    for(size_t id : ids)
        renderer.releasePrimitive(id);
    emptyFrame();
}

TEST_CASE("Slots written: a release between frames counts in the next frame")
{
    Guard guard;
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    size_t id=0;

    renderer.generatePrimitive(id);
    renderer.setClipRegion(wholeView());
    renderer.setDrawColor(gweni::Color(200, 30, 40, 255));

    auto draw=[&](OpenGLCore &r) { r.drawFilledRect(id, Rect(10, 10, 40, 20), 1); };

    frame(draw);
    REQUIRE(frame(draw).primitiveSlotsWritten == 0);

    // as a control deleted in the update phase: outside begin()..end()
    renderer.releasePrimitive(id);

    const OpenGLCore::FrameStats &released=emptyFrame();

    CHECK(released.primitiveSlotsWritten == 1);
    CHECK(released.uploadBytes == SlotVertexBytes);
    CHECK(emptyFrame().primitiveSlotsWritten == 0);
}

TEST_CASE("Slots written: a Selectable's hover-off frame writes its one slot")
{
    const gweni::Point viewSize=gweni::test::viewSize();
    // as in textRunCache.cpp: no setDefaultFont, so deleting the skin keeps test::openSans()
    gweni::skin::textured::TexturedSkin *skin=
        new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());

    REQUIRE(skin->init("DefaultSkin.json"));

    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);

    canvas->setSize(viewSize.x, viewSize.y);

    gweni::controls::Selectable *row=canvas->newChild<gweni::controls::Selectable>();

    row->setText("Row");

    auto settle=[&]()
    {
        for(int i=0; i<3; ++i)
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
    };
    auto step=[&]()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return gweni::test::gpuRenderer().getFrameStats().primitiveSlotsWritten;
    };

    gweni::input::onMouseMoved(canvas, viewSize.x-1, viewSize.y-1, 0, 0);
    settle();
    CHECK(step() == 0);

    // hover on: the row's slot is drawn
    const gweni::Point centre=row->localPosToCanvas(gweni::Point(row->getWidth()/2, row->getHeight()/2));

    gweni::input::onMouseMoved(canvas, centre.x, centre.y, 0, 0);
    CHECK(step() == 1);
    CHECK(row->isHovered());
    CHECK(step() == 0);

    // hover off: the row's slot is hidden, and that is one slot written
    gweni::input::onMouseMoved(canvas, viewSize.x-1, viewSize.y-1, 0, 0);
    CHECK(step() == 1);
    CHECK(step() == 0);

    delete canvas;
    delete skin;

    OpenGLCore &renderer=gweni::test::gpuRenderer();

    renderer.begin();
    renderer.end();
}
