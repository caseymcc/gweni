/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Text at or above OpenGLCore's SDF threshold (GWENI_SDF_TEXT_MIN, 18 px by
 *  default) is drawn from one signed distance field atlas per face, shared by
 *  every size, with its metrics scaled from the atlas's base size. These cases
 *  pin that sharing, that measureText scales with the size and fits the drawn
 *  run, and that changing a run's size uploads nothing to the atlas. Each skips
 *  itself when GWENI_SDF_TEXT_MIN puts its sizes out of the SDF range.
 *
 *  measureText contains the drawn ink, and a run's size change uploads its
 *  vertex slots only.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::GLTextMode;
using gweni::renderer::OpenGLCore;

const Color background(0, 0, 255, 255);
const Color white(255, 255, 255, 255);

Font openSansAt(float size)
{
    Font font=gweni::test::openSans();

    font.size=size;
    return font;
}

//! Whether every size is drawn from the SDF atlas in this run.
bool allSdf(const std::vector<float> &sizes)
{
    for(float size:sizes)
    {
        if(gweni::test::gpuRenderer().getTextMode(openSansAt(size)) != GLTextMode::Sdf)
        {
            MESSAGE("skipped: GWENI_SDF_TEXT_MIN puts ", size, " px below the SDF threshold");
            return false;
        }
    }
    return true;
}

//! The box [x0, x1) x [y0, y1) of the pixels that are not the background.
struct Ink
{
    int x0=0, y0=0, x1=0, y1=0;
    size_t pixels=0;
};

Ink inkOf(const std::vector<uint8_t> &frame)
{
    const Point size=gweni::test::viewSize();
    Ink ink;

    ink.x0=size.x;
    ink.y0=size.y;
    for(int y=0; y<size.y; ++y)
    {
        for(int x=0; x<size.x; ++x)
        {
            const uint8_t *pixel=&frame[(y*size.x+x)*4];

            if((pixel[0] == background.r) && (pixel[1] == background.g) && (pixel[2] == background.b))
                continue;
            ++ink.pixels;
            ink.x0=std::min(ink.x0, x);
            ink.y0=std::min(ink.y0, y);
            ink.x1=std::max(ink.x1, x+1);
            ink.y1=std::max(ink.y1, y+1);
        }
    }
    return ink;
}

//! Renders text on textId alone in one frame and reads it back.
std::vector<uint8_t> drawFrame(size_t textId, const Font &font, Point pos, const String &text)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Point size=gweni::test::viewSize();
    std::vector<uint8_t> pixels;

    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    renderer.begin();
    renderer.setDrawColor(white);
    renderer.renderText(textId, font, pos, 1, text);
    renderer.end();

    gweni::capture::readFramebufferRgba(size.x, size.y, pixels);
    return pixels;
}

}//namespace

TEST_CASE("SDF text: every size of a face shares one atlas, apart from the coverage sizes")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    if(!allSdf({ 24.0f, 36.0f, 72.0f }))
        return;

    const Font at24=openSansAt(24.0f);
    const Font at36=openSansAt(36.0f);
    const Font at72=openSansAt(72.0f);

    REQUIRE((renderer.loadFont(at24) == Font::Status::Loaded));

    const size_t fonts=renderer.getLoadedFontCount();

    REQUIRE((renderer.loadFont(at36) == Font::Status::Loaded));
    REQUIRE((renderer.loadFont(at72) == Font::Status::Loaded));

    CHECK(renderer.getLoadedFontCount() == fonts);
    CHECK(renderer.getFontAtlasName(at24) != 0);
    CHECK(renderer.getFontAtlasName(at36) == renderer.getFontAtlasName(at24));
    CHECK(renderer.getFontAtlasName(at72) == renderer.getFontAtlasName(at24));
    // the shared 11 px UI font keeps its own coverage atlas
    if(renderer.getTextMode(gweni::test::openSans()) == GLTextMode::Coverage)
        CHECK(renderer.getFontAtlasName(gweni::test::openSans()) != renderer.getFontAtlasName(at24));

    // each size's load holds the face's atlas: freeing one leaves it to the others
    const unsigned int atlas=renderer.getFontAtlasName(at24);

    renderer.freeFont(at36);
    CHECK(renderer.getFontAtlasName(at24) == atlas);
    CHECK(glIsTexture(atlas) == GL_TRUE);
    CHECK(renderer.getLoadedFontCount() == fonts);
    renderer.freeFont(at72);
    CHECK(renderer.getFontAtlasName(at24) == atlas);

    // the last owner's free, with no run laid out in it, frees it for every size;
    // unless GWENI_SDF_TEXT_MIN puts the shared fixture in it too, whose load holds it
    const bool fixtureShares=(renderer.getFontAtlasName(gweni::test::openSans()) == atlas);

    renderer.freeFont(at24);
    if(fixtureShares)
    {
        CHECK(renderer.getFontAtlasName(at24) == atlas);
        CHECK(renderer.getLoadedFontCount() == fonts);
        return;
    }
    CHECK(renderer.getFontAtlasName(at24) == 0);
    CHECK(renderer.getFontAtlasName(at36) == 0);
    CHECK(glIsTexture(atlas) == GL_FALSE);
    CHECK(renderer.getLoadedFontCount() == fonts-1);
}

TEST_CASE("SDF text: measureText scales with the size")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    if(!allSdf({ 18.0f, 36.0f, 72.0f }))
        return;

    const String text="Sphinx gyp 0123456789 &?!";
    const Point at18=renderer.measureText(openSansAt(18.0f), text);
    const Point at36=renderer.measureText(openSansAt(36.0f), text);
    const Point at72=renderer.measureText(openSansAt(72.0f), text);

    // the same unhinted layout scaled, so only the rounding up to whole pixels differs
    CHECK(std::abs(at72.x-2*at36.x) <= 2);
    CHECK(std::abs(at36.x-2*at18.x) <= 2);
    CHECK(std::abs(at72.y-2*at36.y) <= 2);
    CHECK(std::abs(at36.y-2*at18.y) <= 2);

    renderer.freeFont(openSansAt(36.0f));
}

TEST_CASE("SDF text: the drawn run fits the box measureText gives")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    if(!allSdf({ 24.0f, 36.0f, 48.0f }))
        return;

    // no glyph of these reaches past its advance or below the descender
    const String text="Sphinx gyp 0123";

    for(float size:{ 24.0f, 36.0f, 48.0f })
    {
        CAPTURE(size);

        const Font font=openSansAt(size);
        const Point pos(8, 20);
        const Point measured=renderer.measureText(font, text);
        size_t textId=0;

        renderer.generateTextPrimitive(textId);

        const Ink ink=inkOf(drawFrame(textId, font, pos, text));

        REQUIRE(ink.pixels > 100);
        CHECK(ink.x0 >= pos.x);
        CHECK(ink.y0 >= pos.y);
        CHECK(ink.x1 <= pos.x+measured.x);
        CHECK(ink.y1 <= pos.y+measured.y);
        // and it is no box much larger than the ink either
        CHECK(ink.x1 >= pos.x+measured.x-size/4);

        renderer.releaseTextPrimitive(textId);
        renderer.begin();
        renderer.end();
    }

    renderer.freeFont(openSansAt(24.0f));
}

TEST_CASE("SDF text: changing a run's size rewrites its vertexes and uploads nothing to the atlas")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    if(!allSdf({ 24.0f, 36.0f }))
        return;

    const String text="Sphinx gyp";
    size_t textId=0;

    renderer.generateTextPrimitive(textId);

    const std::vector<uint8_t> at24=drawFrame(textId, openSansAt(24.0f), Point(8, 20), text);
    const std::vector<uint8_t> at36=drawFrame(textId, openSansAt(36.0f), Point(8, 20), text);
    const OpenGLCore::FrameStats stats=renderer.getFrameStats();

    CHECK(at24 != at36);
    CHECK(stats.atlasUploadCalls == 0);
    // one slot per codepoint, the space included: the run's own, and only them
    CHECK(stats.textSlotsWritten == text.size());
    // the glyph indexes are static, and the run kept its slots: one vertex range
    CHECK(stats.uploadCalls == 1);
    CHECK(stats.uploadBytes == text.size()*4*sizeof(ftgl::glyph_vertex_t));

    renderer.releaseTextPrimitive(textId);
    renderer.begin();
    renderer.end();
    renderer.freeFont(openSansAt(24.0f));
}

TEST_CASE("SDF text: freeing one size while other sizes' runs use the atlas leaves them drawing")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    if(!allSdf({ 24.0f, 48.0f }))
        return;

    // what Label does: one Label's font is freed when it changes size or is
    // deleted, while another Label still draws a different size of the face
    const Font at24=openSansAt(24.0f);
    const Font at48=openSansAt(48.0f);
    const String text="Sphinx gyp";
    const Point pos24(8, 10);
    const Point pos48(8, 60);
    size_t id24=0;
    size_t id48=0;

    renderer.generateTextPrimitive(id24);
    renderer.generateTextPrimitive(id48);

    // 48 px alone, for the pixels it must keep
    const std::vector<uint8_t> alone48=drawFrame(id48, at48, pos48, text);

    REQUIRE(inkOf(alone48).pixels > 100);

    const unsigned int atlas=renderer.getFontAtlasName(at48);
    const size_t fonts=renderer.getLoadedFontCount();

    drawFrame(id24, at24, pos24, text);

    renderer.freeFont(at24);

    // the shared atlas stays, under the same GL name, and is not uploaded again
    CHECK(renderer.getFontAtlasName(at48) == atlas);
    CHECK(glIsTexture(atlas) == GL_TRUE);
    CHECK(renderer.getLoadedFontCount() == fonts);

    // the 24 px run goes; the 48 px run, untouched since, draws as it did alone
    renderer.releaseTextPrimitive(id24);

    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    renderer.begin();
    renderer.end();

    std::vector<uint8_t> after;
    const Point size=gweni::test::viewSize();

    gweni::capture::readFramebufferRgba(size.x, size.y, after);
    CHECK(after == alone48);
    CHECK(renderer.getFrameStats().atlasUploadCalls == 0);
    CHECK(renderer.getFontAtlasName(at48) == atlas);

    // freed, and its last run gone: the atlas goes with it
    renderer.freeFont(at48);
    CHECK(renderer.getFontAtlasName(at48) == atlas);
    renderer.releaseTextPrimitive(id48);
    CHECK(renderer.getFontAtlasName(at48) == 0);
    CHECK(glIsTexture(atlas) == GL_FALSE);
    CHECK(renderer.getLoadedFontCount() == fonts-1);

    // loading it anew uploads the whole atlas in the next frame, and that is counted
    REQUIRE((renderer.loadFont(at48) == Font::Status::Loaded));
    renderer.begin();
    renderer.end();
    CHECK(renderer.getFrameStats().atlasUploadCalls == 1);
    CHECK(renderer.getFrameStats().atlasUploadBytes >= 1024*1024);
    renderer.freeFont(at48);
    CHECK(renderer.getFontAtlasName(at48) == 0);
}

TEST_CASE("SDF text: a freed atlas used again before its last run leaves is kept")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    if(!allSdf({ 24.0f, 36.0f }))
        return;

    const Font at24=openSansAt(24.0f);
    const Font at36=openSansAt(36.0f);
    size_t textId=0;

    renderer.generateTextPrimitive(textId);
    drawFrame(textId, at24, Point(8, 20), "Sphinx");

    const unsigned int atlas=renderer.getFontAtlasName(at24);

    // Label::setFont(face, 36, bold): the 24 px font is freed first, then the run
    // is laid out at 36 px, which moves it within the same atlas
    renderer.freeFont(at24);
    drawFrame(textId, at36, Point(8, 20), "Sphinx");
    CHECK(renderer.getFrameStats().atlasUploadCalls == 0);

    // the run leaving it now does not free it: the 36 px use took it back
    renderer.releaseTextPrimitive(textId);
    CHECK(renderer.getFontAtlasName(at36) == atlas);
    CHECK(glIsTexture(atlas) == GL_TRUE);

    renderer.begin();
    renderer.end();
    renderer.freeFont(at36);
    CHECK(renderer.getFontAtlasName(at36) == 0);
}
