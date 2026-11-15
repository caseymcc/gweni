/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas::setDrawBackground on a real Canvas with the StubRenderer, drawn by
 *  the textured skin's Canvas renderer (every other type a StubControl):
 *  - a new canvas draws no background: its frames take no slot and render
 *    nothing, as a canvas always did;
 *  - setDrawBackground(true) marks the canvas (StateChange_Background), and
 *    its next frame takes the canvas slot and paints it in the Canvas surface
 *    over the canvas's bounds;
 *  - setDrawBackground(false) marks it, and its next frame hides the slot,
 *    drawing nothing in it; true again paints it in the same slot;
 *  - setting the value it already has marks nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <map>
#include <set>

using namespace gweni;

namespace
{

//! Which slots are hidden, and the top colour and rect each rounded rect drew.
class BackgroundRecorder: public test::StubRenderer
{
public:
    std::set<size_t> hidden;
    std::map<size_t, Color> top;
    std::map<size_t, Rect> rects;

    void hidePrimitive(size_t id) override
    {
        hidden.insert(id);
        StubRenderer::hidePrimitive(id);
    }

    void drawRoundedRect(size_t id, Rect rect, float radius, float border,
        Color topColor, Color bottom, Color edge, int z) override
    {
        hidden.erase(id);
        top[id]=topColor;
        rects[id]=rect;
        StubRenderer::drawRoundedRect(id, rect, radius, border, topColor, bottom, edge, z);
    }
};

//! The textured Canvas renderer; a StubControl for every other type.
class CanvasSkin: public test::StubSkin
{
public:
    explicit CanvasSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Canvas")
            return std::make_unique<skin::textured::Canvas>();
        return StubSkin::getControlRenderer(typeName);
    }
};

void frame(controls::Canvas &canvas)
{
    canvas.updateCanvas();
    canvas.renderCanvas();
}

}//namespace

TEST_CASE("Canvas background: the canvas slot is shown with the background on and hidden with it off")
{
    BackgroundRecorder renderer;
    CanvasSkin skin(&renderer);
    controls::Canvas canvas(&skin);
    canvas.setSize(320, 200);

    CHECK_FALSE(canvas.drawsBackground());
    const size_t rendered=controls::getChangeStats().controls;
    frame(canvas);
    frame(canvas);
    CHECK(renderer.livePrimitives.empty());
    CHECK(controls::getChangeStats().controls == rendered);
    canvas.setDrawBackground(false);
    CHECK(canvas.getStateChange() == controls::StateChange_Nothing);

    canvas.setDrawBackground(true);
    CHECK(canvas.drawsBackground());
    CHECK((canvas.getStateChange() & controls::StateChange_Background) != 0);
    frame(canvas);
    // the canvas renderer's one slot, the only one this renderer handed out
    REQUIRE(renderer.livePrimitives.size() == 1);
    const size_t slot=*renderer.livePrimitives.begin();
    CHECK(renderer.hidden.count(slot) == 0);
    REQUIRE(renderer.top.count(slot) == 1);
    CHECK((renderer.top[slot] == skin::textured::tokyonight::Canvas.top));
    CHECK((renderer.rects[slot] == Rect(0, 0, 320, 200)));
    CHECK(canvas.getStateChange() == controls::StateChange_Nothing);

    canvas.setDrawBackground(false);
    CHECK_FALSE(canvas.drawsBackground());
    CHECK((canvas.getStateChange() & controls::StateChange_Background) != 0);
    renderer.top.clear();
    frame(canvas);
    CHECK(renderer.hidden.count(slot) == 1);
    CHECK(renderer.top.count(slot) == 0);

    // the same value again marks nothing
    canvas.setDrawBackground(false);
    CHECK(canvas.getStateChange() == controls::StateChange_Nothing);

    canvas.setDrawBackground(true);
    CHECK((canvas.getStateChange() & controls::StateChange_Background) != 0);
    frame(canvas);
    CHECK(renderer.hidden.count(slot) == 0);
    CHECK(renderer.top.count(slot) == 1);
    CHECK(renderer.livePrimitives.size() == 1);

    canvas.setDrawBackground(true);
    CHECK(canvas.getStateChange() == controls::StateChange_Nothing);
}
