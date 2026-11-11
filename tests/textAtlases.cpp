/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Every font SlotRenderer loads has its own glyph atlas, and a text run must be
 *  drawn with the atlas of the font it was laid out in, whatever else is on
 *  screen. Each case draws a run in the shared OpenSans 11 and a run in OpenSans
 *  24 in one frame, in separate bands of the view, and compares each band with
 *  the same run drawn alone in a frame of its own: byte for byte, since the two
 *  runs do not overlap and each glyph goes through the same draw either way.
 *
 *  Each case skips itself when GWENI_SDF_TEXT_MIN puts both sizes in the SDF
 *  range, where they share the face's one atlas.
 *
 *  GWENI_TEXT_ATLASES_PNG=<path> writes the two-font frame of the first case
 *  there, to look at.
 *
 *  Also: one font freed while its run is still allocated, and a steady
 *  two-font frame uploading nothing.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 255, 255);
const Color white(255, 255, 255, 255);

//! The rows [0, c_bandSplit) hold the small run, the rows below it the large one.
const int c_bandSplit=70;

//! One renderText call.
struct TextDraw
{
    Font font;
    Point pos;
    String text;
};

Font largeFont()
{
    Font font=gweni::test::openSans();

    font.size=24;
    return font;
}

TextDraw smallDraw()
{
    return TextDraw{ gweni::test::openSans(), Point(8, 40), "Small OpenSans 11 text" };
}

TextDraw largeDraw()
{
    return TextDraw{ largeFont(), Point(8, 120), "Large OpenSans 24" };
}

void clearFrame()
{
    gweni::test::clearView(background);
}

//! The offscreen framebuffer, top-down RGBA8.
std::vector<uint8_t> readFrame()
{
    std::vector<uint8_t> pixels;

    gweni::test::readView(pixels);
    return pixels;
}

//! The rows [y0, y1) of a frame.
std::vector<uint8_t> band(const std::vector<uint8_t> &pixels, int y0, int y1)
{
    const size_t rowBytes=gweni::test::viewSize().x*4;

    return std::vector<uint8_t>(pixels.begin()+y0*rowBytes, pixels.begin()+y1*rowBytes);
}

std::vector<uint8_t> smallBand(const std::vector<uint8_t> &pixels)
{
    return band(pixels, 0, c_bandSplit);
}

std::vector<uint8_t> largeBand(const std::vector<uint8_t> &pixels)
{
    return band(pixels, c_bandSplit, gweni::test::viewSize().y);
}

//! Pixels that are not the background colour.
size_t drawnPixels(const std::vector<uint8_t> &pixels)
{
    size_t drawn=0;

    for(size_t i=0; i+3 < pixels.size(); i+=4)
    {
        if((pixels[i] != background.r) || (pixels[i+1] != background.g) || (pixels[i+2] != background.b))
            ++drawn;
    }
    return drawn;
}

//! Pixels that differ between two frames or bands of the same size, so a
//! mismatch reports a count rather than both buffers.
size_t differingPixels(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
{
    REQUIRE(a.size() == b.size());

    size_t differing=0;

    for(size_t i=0; i+3 < a.size(); i+=4)
    {
        if(memcmp(&a[i], &b[i], 4) != 0)
            ++differing;
    }
    return differing;
}

//! Renders the draws, in order, on the matching text ids in one frame and reads it back.
std::vector<uint8_t> drawFrame(const std::vector<size_t> &textIds, const std::vector<TextDraw> &draws)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    clearFrame();
    renderer.begin();
    renderer.setDrawColor(white);
    for(size_t i=0; i<draws.size(); ++i)
        renderer.renderText(textIds[i], draws[i].font, draws[i].pos, 1, draws[i].text);
    renderer.end();
    return readFrame();
}

//! The frame `draw` gives alone, on a text id of its own released afterwards.
std::vector<uint8_t> drawAlone(const TextDraw &draw)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    size_t textId=0;

    renderer.generateTextPrimitive(textId);

    const std::vector<uint8_t> pixels=drawFrame({ textId }, { draw });

    renderer.releaseTextPrimitive(textId);
    renderer.begin();
    renderer.end();
    return pixels;
}

//! Forgets the errors the device reported so far, so a later deviceError() is this case's
void clearDeviceErrors()
{
    gweni::test::gpuRenderer().deviceError();
}

//! Whether the loaded large font and the shared OpenSans 11 have atlases of their own.
bool twoAtlases(const Font &large)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    if(renderer.getFontAtlasHandle(large) != renderer.getFontAtlasHandle(gweni::test::openSans()))
        return true;
    // not freed: that would free the shared font's atlas with it
    MESSAGE("skipped: GWENI_SDF_TEXT_MIN draws both sizes from the face's one SDF atlas");
    return false;
}

}//namespace

TEST_CASE("Text atlases: two fonts in one frame each draw as they do alone")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font large=largeFont();

    REQUIRE((renderer.loadFont(large) == Font::Status::Loaded));
    if(!twoAtlases(large))
        return;

    const std::vector<uint8_t> smallAlone=drawAlone(smallDraw());
    const std::vector<uint8_t> largeAlone=drawAlone(largeDraw());

    // each run stays inside its own band, so the bands can be compared one by one
    REQUIRE(drawnPixels(smallBand(smallAlone)) > 50);
    REQUIRE(drawnPixels(largeBand(smallAlone)) == 0);
    REQUIRE(drawnPixels(largeBand(largeAlone)) > 50);
    REQUIRE(drawnPixels(smallBand(largeAlone)) == 0);

    size_t smallId=0;
    size_t largeId=0;

    renderer.generateTextPrimitive(smallId);
    renderer.generateTextPrimitive(largeId);

    std::vector<uint8_t> both;

    // which font was used last must not matter
    SUBCASE("small run first")
    {
        both=drawFrame({ smallId, largeId }, { smallDraw(), largeDraw() });

        if(const char *path=std::getenv("GWENI_TEXT_ATLASES_PNG"))
            gweni::capture::writeRgbaPng(both, gweni::test::viewSize().x, gweni::test::viewSize().y, path);
    }

    SUBCASE("large run first")
    {
        both=drawFrame({ largeId, smallId }, { largeDraw(), smallDraw() });
    }

    CHECK(differingPixels(smallBand(both), smallBand(smallAlone)) == 0);
    CHECK(differingPixels(largeBand(both), largeBand(largeAlone)) == 0);

    // one draw per atlas, both again in the next frame without any new text
    CHECK(renderer.getFrameStats().textDraws == 2);

    clearFrame();
    renderer.begin();
    renderer.end();

    const std::vector<uint8_t> next=readFrame();

    CHECK(differingPixels(smallBand(next), smallBand(smallAlone)) == 0);
    CHECK(differingPixels(largeBand(next), largeBand(largeAlone)) == 0);

    renderer.releaseTextPrimitive(smallId);
    renderer.releaseTextPrimitive(largeId);
    renderer.begin();
    renderer.end();
    renderer.freeFont(large);
}

TEST_CASE("Text atlases: freeing a font with a run still allocated leaves both fonts drawing")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font large=largeFont();

    REQUIRE((renderer.loadFont(large) == Font::Status::Loaded));
    if(!twoAtlases(large))
        return;

    const std::vector<uint8_t> smallAlone=drawAlone(smallDraw());
    const std::vector<uint8_t> largeAlone=drawAlone(largeDraw());

    size_t smallId=0;
    size_t largeId=0;

    renderer.generateTextPrimitive(smallId);
    renderer.generateTextPrimitive(largeId);

    const std::vector<uint8_t> both=drawFrame({ smallId, largeId }, { smallDraw(), largeDraw() });

    CHECK(differingPixels(smallBand(both), smallBand(smallAlone)) == 0);
    CHECK(differingPixels(largeBand(both), largeBand(largeAlone)) == 0);

    // The large run holds its font, coverage or SDF alike: its atlas stays under
    // the same name while the run is laid out in it, and the run draws on.
    const unsigned int largeAtlas=renderer.getFontAtlasHandle(large);

    clearDeviceErrors();
    renderer.freeFont(large);
    CHECK(renderer.getFontAtlasHandle(large) == largeAtlas);
    CHECK(renderer.textureExists(largeAtlas));

    SUBCASE("a frame that renders only the other run")
    {
        const std::vector<uint8_t> after=drawFrame({ smallId }, { smallDraw() });

        CHECK(!renderer.deviceError());
        CHECK(differingPixels(smallBand(after), smallBand(smallAlone)) == 0);
        CHECK(differingPixels(largeBand(after), largeBand(largeAlone)) == 0);
        CHECK(renderer.getFrameStats().textDraws == 2);
    }

    SUBCASE("a frame that renders nothing")
    {
        clearFrame();
        renderer.begin();
        renderer.end();

        const std::vector<uint8_t> after=readFrame();

        CHECK(!renderer.deviceError());
        CHECK(differingPixels(smallBand(after), smallBand(smallAlone)) == 0);
        CHECK(differingPixels(largeBand(after), largeBand(largeAlone)) == 0);
    }

    // the same call on the run uses the font again: the glyphs already there, no
    // slot written and nothing uploaded to an atlas
    const std::vector<uint8_t> again=drawFrame({ smallId, largeId }, { smallDraw(), largeDraw() });

    CHECK(!renderer.deviceError());
    CHECK(differingPixels(smallBand(again), smallBand(smallAlone)) == 0);
    CHECK(differingPixels(largeBand(again), largeBand(largeAlone)) == 0);
    CHECK(renderer.getFrameStats().textSlotsWritten == 0);
    CHECK(renderer.getFrameStats().atlasUploadCalls == 0);

    // a use is no owner: the runs leaving it, the font stays for
    // kIdleFontFlushes flushes and then goes
    renderer.releaseTextPrimitive(smallId);
    renderer.releaseTextPrimitive(largeId);
    CHECK(renderer.getFontAtlasHandle(large) == largeAtlas);
    for(uint64_t flush=0; flush <= SlotRenderer::kIdleFontFlushes; ++flush)
    {
        renderer.begin();
        renderer.end();
    }
    CHECK(renderer.getFontAtlasHandle(large) == 0);
    CHECK(!renderer.textureExists(largeAtlas));
    renderer.freeFont(large);
}

TEST_CASE("Text atlases: a steady second frame with two fonts uploads no text")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font large=largeFont();

    REQUIRE((renderer.loadFont(large) == Font::Status::Loaded));
    if(!twoAtlases(large))
        return;

    size_t smallId=0;
    size_t largeId=0;

    renderer.generateTextPrimitive(smallId);
    renderer.generateTextPrimitive(largeId);

    const std::vector<uint8_t> first=drawFrame({ smallId, largeId }, { smallDraw(), largeDraw() });

    REQUIRE(renderer.getFrameStats().textSlotsWritten > 0);

    const std::vector<uint8_t> second=drawFrame({ smallId, largeId }, { smallDraw(), largeDraw() });

    CHECK(renderer.getFrameStats().textSlotsWritten == 0);
    CHECK(renderer.getFrameStats().uploadBytes == 0);
    CHECK(renderer.getFrameStats().atlasUploadBytes == 0);
    CHECK(renderer.getFrameStats().textDraws == 2);
    CHECK(differingPixels(first, second) == 0);

    renderer.releaseTextPrimitive(smallId);
    renderer.releaseTextPrimitive(largeId);
    renderer.begin();
    renderer.end();
    renderer.freeFont(large);
}
