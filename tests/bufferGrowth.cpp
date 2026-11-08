/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's device buffers grow geometrically (#323). A mirror that grows
 *  by a slot a frame regrows its vertex buffer and clip-box texture
 *  (FrameStats::bufferRegrows) O(log K) times over K frames, each regrow
 *  sending the whole mirror once; every other frame sends only the slots that
 *  changed and the ones the mirror gained, never the whole mirror. Two cases,
 *  each on a renderer of its own (so its mirrors start empty):
 *
 *  - a text run grown by one glyph a frame for K frames, its end kept in view.
 *    The run gets slack when it outgrows its slots, so it moves O(log K) times.
 *    Every frame draws, pixel for pixel, what a fresh layout of the same string
 *    on a new text id of a second renderer draws, and its glyph vertexes are
 *    that layout's.
 *  - one new primitive id a frame for K frames, each drawing a rect of its own
 *    colour in a grid cell of its own: every rect drawn so far shows its colour
 *    every frame, the regrown buffers included.
 *
 *  After the growth a frame with no change uploads nothing.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>
#include <gweni/primitiveIds.h>

#include <cmath>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::Rect;
using gweni::String;
using gweni::renderer::OpenGLCore;

const Color background(0, 0, 0, 255);
const Color white(255, 255, 255, 255);

//! The frames each case grows over
const size_t K=200;

void clearFrame()
{
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

//! The offscreen framebuffer, top-down RGBA8.
std::vector<uint8_t> readFrame()
{
    const Point size=gweni::test::viewSize();
    std::vector<uint8_t> pixels;

    gweni::capture::readFramebufferRgba(size.x, size.y, pixels);
    return pixels;
}

//! A renderer of its own on the tests' context, drawing into their framebuffer.
OpenGLCore *newRenderer()
{
    // the renderer keeps a reference to its paths
    static gweni::platform::CurrentPath paths("resources");
    OpenGLCore *renderer=new OpenGLCore(paths, Rect(Point(0, 0), gweni::test::viewSize()));

    renderer->init();
    return renderer;
}

//! Regrows of one buffer that grows from nothing to `slots` slots, each to at
//! least 1.5 times what it held: the first, then one per factor of 1.5.
size_t regrowBound(size_t slots)
{
    return 2+size_t(std::ceil(std::log(double(slots))/std::log(1.5)));
}

std::vector<uint8_t> textBytes(OpenGLCore &renderer, size_t textId)
{
    const OpenGLCore::MirrorSnapshot snapshot=renderer.snapshotMirror();
    const auto it=snapshot.texts.find(textId);

    REQUIRE(it != snapshot.texts.end());
    return it->second;
}

}//namespace

TEST_CASE("Buffer growth: a text run grown a glyph a frame regrows O(log K) times and draws a fresh layout")
{
    OpenGLCore *renderer=newRenderer();
    // the fresh layouts, on a renderer of their own so they leave the grown
    // run's mirror alone
    OpenGLCore *freshRenderer=newRenderer();
    Font font=gweni::test::openSans();

    REQUIRE(renderer->loadFont(font) == Font::Status::Loaded);
    REQUIRE(freshRenderer->loadFont(font) == Font::Status::Loaded);

    // what one glyph slot uploads at most: its 4 vertexes and its clip box
    const size_t slotBytes=4*sizeof(ftgl::glyph_vertex_t)+4*sizeof(uint16_t);
    const Point view=gweni::test::viewSize();

    size_t textId=0;
    size_t regrows=0;
    size_t regrowFrames=0;
    size_t mirrorSlots=0;
    String text;

    renderer->generateTextPrimitive(textId);

    for(size_t frame=0; frame<K; ++frame)
    {
        text+=char('a'+frame%26);

        // the end of the run in view, so the newest glyphs are on the screen
        const Point pos(std::min(8, view.x-8-renderer->measureText(font, text).x), 60);

        clearFrame();
        renderer->begin();
        renderer->setDrawColor(white);
        renderer->renderText(textId, font, pos, 1, text);
        renderer->end();

        const OpenGLCore::FrameStats stats=renderer->getFrameStats();
        const std::vector<uint8_t> grown=readFrame();
        const std::vector<uint8_t> grownBytes=textBytes(*renderer, textId);

        regrows+=stats.bufferRegrows;
        mirrorSlots=std::max(mirrorSlots, stats.mirrorBytes/slotBytes);

        if(stats.bufferRegrows > 0)
            ++regrowFrames;
        else
        {
            // no full resend: the run's slots, its slack and the mirror's new tail
            INFO("frame ", frame, " upload_bytes ", stats.uploadBytes);
            CHECK(stats.uploadBytes <= (2*text.size()+8)*slotBytes);
        }

        // the same string laid out fresh on a new id
        size_t freshId=0;

        freshRenderer->generateTextPrimitive(freshId);
        clearFrame();
        freshRenderer->begin();
        freshRenderer->setDrawColor(white);
        freshRenderer->renderText(freshId, font, pos, 1, text);
        freshRenderer->end();

        const std::vector<uint8_t> fresh=readFrame();
        const std::vector<uint8_t> freshBytes=textBytes(*freshRenderer, freshId);

        freshRenderer->releaseTextPrimitive(freshId);

        INFO("frame ", frame, " glyphs ", text.size());
        CHECK(grown == fresh);
        CHECK(grownBytes == freshBytes);
    }

    INFO("regrows ", regrows, " in ", regrowFrames, " frames, mirror up to ", mirrorSlots, " slots");
    // a vertex buffer and a clip-box texture, each O(log) of the slots
    CHECK(regrows > 0);
    CHECK(regrows <= 2*regrowBound(mirrorSlots));
    CHECK(regrowFrames < K/8);

    // a frame with no change uploads nothing, and regrows nothing
    renderer->begin();
    renderer->end();
    CHECK(renderer->getFrameStats().uploadBytes == 0);
    CHECK(renderer->getFrameStats().bufferRegrows == 0);

    renderer->releaseTextPrimitive(textId);
    renderer->freeFont(font);
    delete renderer;
    freshRenderer->begin();
    freshRenderer->end();
    freshRenderer->freeFont(font);
    delete freshRenderer;
}

TEST_CASE("Buffer growth: one new primitive slot a frame regrows O(log K) times and every slot draws its rect")
{
    OpenGLCore *renderer=newRenderer();
    const Point view=gweni::test::viewSize();

    // Ids are process-wide and released ones are handed out first: hold those,
    // so each id below is new and grows the mirror by its slot.
    std::vector<size_t> held(gweni::releasedPrimitiveIdCount());

    for(size_t &id:held)
        renderer->generatePrimitive(id);

    // a grid of 16 px cells, 14 px rects of distinct colours
    const int columns=view.x/16;
    REQUIRE(size_t(columns*(view.y/16)) >= K);

    auto cellColor=[](size_t i) { return Color(uint8_t(40+(i*37)%200), uint8_t(40+(i*91)%200), uint8_t(40+i%200), 255); };
    auto cellRect=[&](size_t i) { return Rect(int(i%columns)*16+1, int(i/columns)*16+1, 14, 14); };

    std::vector<size_t> ids;
    size_t regrows=0;
    size_t regrowFrames=0;
    size_t firstSlots=0;
    size_t lastSlots=0;

    renderer->begin();
    renderer->end();

    for(size_t frame=0; frame<K; ++frame)
    {
        size_t id=0;

        renderer->generatePrimitive(id);
        ids.push_back(id);

        clearFrame();
        renderer->begin();
        renderer->setDrawColor(cellColor(frame));
        renderer->drawFilledRect(id, cellRect(frame), 1);
        renderer->end();

        const OpenGLCore::FrameStats stats=renderer->getFrameStats();
        const size_t slots=stats.mirrorBytes/(4*32+4*sizeof(uint16_t));

        if(frame == 0)
            firstSlots=slots;
        lastSlots=slots;
        regrows+=stats.bufferRegrows;
        if(stats.bufferRegrows > 0)
            ++regrowFrames;
        else
        {
            // the new slot alone
            INFO("frame ", frame, " upload_bytes ", stats.uploadBytes);
            CHECK(stats.uploadBytes <= 4*32+4*sizeof(uint16_t));
        }

        // every rect drawn so far shows its colour at its centre
        const std::vector<uint8_t> pixels=readFrame();
        size_t wrong=0;

        for(size_t i=0; i<=frame; ++i)
        {
            const Rect rect=cellRect(i);
            const size_t at=(size_t(rect.y+7)*view.x+size_t(rect.x+7))*4;
            const Color color=cellColor(i);

            if((pixels[at] != color.r) || (pixels[at+1] != color.g) || (pixels[at+2] != color.b))
                ++wrong;
        }
        INFO("frame ", frame);
        CHECK(wrong == 0);
    }

    INFO("regrows ", regrows, " in ", regrowFrames, " frames, mirror ", firstSlots, " -> ", lastSlots, " slots");
    CHECK(lastSlots >= firstSlots+K-1);
    // at most one regrow a buffer per factor of 1.5 the mirror grew by
    CHECK(regrows <= 2*(2+size_t(std::ceil(std::log(double(lastSlots)/double(firstSlots))/std::log(1.5)))));
    CHECK(regrowFrames < K/8);

    renderer->begin();
    renderer->end();
    CHECK(renderer->getFrameStats().uploadBytes == 0);
    CHECK(renderer->getFrameStats().bufferRegrows == 0);

    for(size_t id:ids)
        renderer->releasePrimitive(id);
    for(size_t id:held)
        renderer->releasePrimitive(id);
    renderer->begin();
    renderer->end();
    delete renderer;
}
