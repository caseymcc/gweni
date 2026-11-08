/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  renderer::Base::frameStats(), writtenPrimitiveIds() and writtenTextIds()
 *  without a device: renderer::Base's defaults say "not counted" (counted
 *  false, every count 0, no ids), and StubRenderer counts its own draws per
 *  frame (end()): every primitive draw, show, hide and release writes its id
 *  once however often it was touched, a renderText writes its text id's
 *  codepoints as glyph slots (an empty string none), a show or hide of a text
 *  its last glyph count, the counts add up to the lists, and a frame with no
 *  draws writes nothing.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

using namespace gweni;
using renderer::WrittenText;

namespace
{

struct NoPaths: ResourcePaths
{
    String getPath(Type, String const &name) override { return name; }
};

//! renderer::Base with nothing overridden
struct PlainRenderer: renderer::Base
{
    explicit PlainRenderer(ResourcePaths &paths): Base(paths) {}

    //IResourceLoader, nothing to load
    Font::Status loadFont(const Font &) override { return Font::Status::Loaded; }
    void freeFont(const Font &) override {}
    Texture::Status loadTexture(Texture &) override { return Texture::Status::Loaded; }
    void freeTexture(const Texture &) override {}
    TextureData getTextureData(const Texture &) const override { return TextureData(); }
};

}//namespace

TEST_CASE("Frame counts: renderer::Base does not count")
{
    NoPaths paths;
    PlainRenderer renderer(paths);

    renderer.begin();
    renderer.drawFilledRect(3, Rect(0, 0, 10, 10), 1);
    renderer.end();

    const renderer::FrameCounts counts=renderer.frameStats();

    CHECK_FALSE(counts.counted);
    CHECK(counts.frameIndex == 0);
    CHECK(counts.primitiveSlotsWritten == 0);
    CHECK(counts.textSlotsWritten == 0);
    CHECK(counts.uploadCalls == 0);
    CHECK(counts.uploadBytes == 0);
    CHECK(renderer.writtenPrimitiveIds().empty());
    CHECK(renderer.writtenTextIds().empty());
}

TEST_CASE("Frame counts: StubRenderer counts the ids its draws wrote in a frame")
{
    test::StubRenderer renderer;
    size_t a=0, b=0, c=0, text=0, empty=0;

    renderer.generatePrimitive(a);
    renderer.generatePrimitive(b);
    renderer.generatePrimitive(c);
    renderer.generateTextPrimitive(text);
    renderer.generateTextPrimitive(empty);

    // before any frame ends: counted is false
    CHECK_FALSE(renderer.frameStats().counted);

    renderer.begin();
    renderer.drawFilledRect(b, Rect(0, 0, 10, 10), 1);
    renderer.drawRoundedRect(b, Rect(0, 0, 10, 10), 2, 1, Color(), Color(), Color(), 1);
    renderer.drawIcon(a, Rect(0, 0, 10, 10), 2, 0, Color(), 2);
    // through renderer::Base's default, which draws a rounded rect
    renderer.drawCheckerRect(c, Rect(0, 0, 10, 10), 1, Color(), Color(), Color(), 0, 0, Color());
    renderer.renderText(text, Font(), Point(0, 0), 3, "h\xc3\xa9llo"); // 5 codepoints, 6 bytes
    renderer.renderText(empty, Font(), Point(0, 0), 3, "");
    renderer.end();

    renderer::FrameCounts counts=renderer.frameStats();

    CHECK(counts.counted);
    CHECK(counts.frameIndex == 1);
    CHECK(counts.primitiveSlotsWritten == 3);
    CHECK(counts.textSlotsWritten == 5);

    std::vector<size_t> ids={a, b, c};

    std::sort(ids.begin(), ids.end());
    CHECK(renderer.writtenPrimitiveIds() == ids);
    CHECK(renderer.writtenTextIds() == std::vector<WrittenText>{{text, 5}});

    // a frame with no draws writes nothing
    renderer.begin();
    renderer.end();
    counts=renderer.frameStats();
    CHECK(counts.frameIndex == 2);
    CHECK(counts.primitiveSlotsWritten == 0);
    CHECK(counts.textSlotsWritten == 0);
    CHECK(renderer.writtenPrimitiveIds().empty());
    CHECK(renderer.writtenTextIds().empty());

    // a hide, a show and a text hide; a release counts too
    renderer.begin();
    renderer.hidePrimitive(a);
    renderer.showPrimitive(1, b);
    renderer.hideText(text);
    renderer.end();
    ids={a, b};
    std::sort(ids.begin(), ids.end());
    CHECK(renderer.writtenPrimitiveIds() == ids);
    CHECK(renderer.writtenTextIds() == std::vector<WrittenText>{{text, 5}});
    CHECK(renderer.frameStats().textSlotsWritten == 5);

    renderer.releasePrimitive(c);
    renderer.begin();
    renderer.end();
    CHECK(renderer.writtenPrimitiveIds() == std::vector<size_t>{c});

    renderer.releasePrimitive(a);
    renderer.releasePrimitive(b);
    renderer.releaseTextPrimitive(text);
    renderer.releaseTextPrimitive(empty);
}

TEST_CASE("Frame counts: a settled canvas on StubRenderer writes nothing")
{
    test::StubRenderer renderer;
    test::StubSkin skin(&renderer);
    controls::Canvas *canvas=new controls::Canvas(&skin);

    canvas->setSize(200, 100);
    canvas->newChild<controls::Base>("panel")->newChild<test::StubText>("text");

    for(int i=0; i < 3; ++i)
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
    CHECK(renderer.frameStats().counted);
    CHECK(renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(renderer.frameStats().textSlotsWritten == 0);
    CHECK(renderer.writtenPrimitiveIds().empty());

    delete canvas;
}
