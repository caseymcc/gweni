/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's grayscale text: glyphs come from a one-channel coverage atlas,
 *  are placed on whole pixels with whole-pixel advances, and their coverage is
 *  applied to the destination once. Each case draws offscreen and reads the
 *  framebuffer back, or reads the glyph quads out of the text mirror.
 *
 *  A glyph quad is as wide and tall as its atlas region and sits on whole
 *  pixels, so each pixel centre samples exactly one texel centre: a pixel's
 *  coverage is that texel's value.
 *
 *  This is the coverage path: a size that GWENI_SDF_TEXT_MIN puts in the SDF
 *  range is skipped.
 *
 *  The atlas is depth 1 / GL_R8. White text over mid-grey reads back with
 *  R==G==B, white on black reads back as the atlas coverage texel for texel
 *  (applied once, not squared), and "IIIIIIII"/"nnnnnnnn" at 8-16 px step by
 *  one whole-pixel advance whose sum is measureText's width.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::OpenGLCore;

const Color white(255, 255, 255, 255);

//! One glyph quad of a run as the mirror holds it, in view pixels.
struct GlyphQuad
{
    float x0, y0, x1, y1; //!< top-left and bottom-right
    float s0, t0;         //!< atlas coordinates of the top-left corner
};

Font sized(float size)
{
    Font font=gweni::test::openSans();

    font.size=size;
    return font;
}

void clearFrame(const Color &background)
{
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

//! A text id for one test case, released (and that flushed) when it ends, so the
//! next case's frame starts empty.
struct TextId
{
    TextId()
    {
        gweni::test::gpuRenderer().generateTextPrimitive(id);
    }

    ~TextId()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
    }

    size_t id=0;
};

//! Draws `text` in `font` and white at `pos` over `background`, in one frame.
void drawText(const TextId &textId, const Font &font, Point pos, const String &text, const Color &background)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    clearFrame(background);
    renderer.begin();
    renderer.setDrawColor(white);
    renderer.renderText(textId.id, font, pos, 1, text);
    renderer.end();
}

//! The glyph quads of a text id's run, in glyph order.
std::vector<GlyphQuad> glyphQuads(const TextId &textId)
{
    const OpenGLCore::MirrorSnapshot snapshot=gweni::test::gpuRenderer().snapshotMirror();
    const std::vector<uint8_t> &bytes=snapshot.texts.at(textId.id);
    const size_t vertexSize=sizeof(ftgl::glyph_vertex_t);
    std::vector<GlyphQuad> quads;

    // the hidden flag, then 4 vertexes per glyph: (x0,y0) (x0,y1) (x1,y1) (x1,y0)
    REQUIRE((bytes.size()-1)%(4*vertexSize) == 0);
    for(size_t offset=1; offset < bytes.size(); offset+=4*vertexSize)
    {
        ftgl::glyph_vertex_t v[4];

        std::memcpy(v, &bytes[offset], sizeof(v));
        quads.push_back(GlyphQuad{v[0].x, v[0].y, v[2].x, v[2].y, v[0].u, v[0].v});
    }
    return quads;
}

//! A font's atlas as it is on the GPU: the coverage the text shader samples, one
//! byte per texel.
std::vector<uint8_t> atlasCoverage(const Font &font, int &width)
{
    const unsigned int name=gweni::test::gpuRenderer().getFontAtlasName(font);
    int height=0;

    REQUIRE(name != 0);
    glBindTexture(GL_TEXTURE_2D, name);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

    std::vector<uint8_t> coverage(static_cast<size_t>(width)*height);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, coverage.data());
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    // the renderer rebinds what it needs; leave nothing bound behind it
    glBindTexture(GL_TEXTURE_2D, 0);
    return coverage;
}

std::vector<uint8_t> readFrame()
{
    const Point size=gweni::test::viewSize();
    std::vector<uint8_t> pixels;

    gweni::capture::readFramebufferRgba(size.x, size.y, pixels);
    return pixels;
}

const uint8_t *pixelAt(const std::vector<uint8_t> &frame, int x, int y)
{
    return &frame[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];
}

bool isWhole(float v)
{
    return v == std::floor(v);
}

//! Whether font is drawn from a coverage atlas; says so when it is skipped.
bool coverage(const Font &font)
{
    if(gweni::test::gpuRenderer().getTextMode(font) == gweni::renderer::GLTextMode::Coverage)
        return true;
    MESSAGE("skipped: GWENI_SDF_TEXT_MIN draws ", font.size, " px from the SDF atlas");
    return false;
}

}//namespace

TEST_CASE("Grayscale text: the glyph atlas holds one coverage byte per texel")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Font &font=gweni::test::openSans();

    if(!coverage(font))
        return;
    CHECK(renderer.getFontAtlasDepth(font) == 1);

    GLint format=0;
    GLint greenBits=-1;

    glBindTexture(GL_TEXTURE_2D, renderer.getFontAtlasName(font));
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_GREEN_SIZE, &greenBits);
    glBindTexture(GL_TEXTURE_2D, 0);

    CHECK(format == GL_R8);
    CHECK(greenBits == 0);
}

TEST_CASE("Grayscale text: white text over mid-grey is neutral grey, with no colour fringe")
{
    const Color grey(128, 128, 128, 255);
    TextId textId;

    if(!coverage(gweni::test::openSans()))
        return;

    drawText(textId, gweni::test::openSans(), Point(8, 30), "Illumination Tester 0123 minimum", grey);

    const std::vector<uint8_t> frame=readFrame();
    const Point size=gweni::test::viewSize();
    int inked=0;
    int coloured=0;

    for(int y=0; y<size.y; ++y)
        for(int x=0; x<size.x; ++x)
        {
            const uint8_t *p=pixelAt(frame, x, y);

            if((p[0] != p[1]) || (p[1] != p[2]))
                ++coloured;
            if(p[0] != grey.r)
                ++inked;
        }

    CHECK(inked > 50); // the premise: something was drawn
    CHECK(coloured == 0);
}

TEST_CASE("Grayscale text: white on black is the atlas coverage, applied once")
{
    // round letters at the UI sizes, so most of their ink is part-covered pixels
    for(float size:{9.0f, 11.0f, 16.0f})
    {
        CAPTURE(size);

        const Font font=sized(size);
        TextId textId;

        if(!coverage(font))
            continue;
        drawText(textId, font, Point(20, 40), "o", Color(0, 0, 0, 255));

        const std::vector<GlyphQuad> quads=glyphQuads(textId);
        REQUIRE(quads.size() == 1);

        const GlyphQuad &quad=quads[0];
        // the premise of reading one texel per pixel
        REQUIRE(isWhole(quad.x0));
        REQUIRE(isWhole(quad.y0));
        REQUIRE(isWhole(quad.x1));
        REQUIRE(isWhole(quad.y1));

        int atlasWidth=0;
        const std::vector<uint8_t> coverage=atlasCoverage(font, atlasWidth);
        const std::vector<uint8_t> frame=readFrame();
        const int atlasHeight=static_cast<int>(coverage.size())/atlasWidth;
        const int texX=static_cast<int>(std::lround(quad.s0*atlasWidth));
        const int texY=static_cast<int>(std::lround(quad.t0*atlasHeight));
        int partial=0;
        int mismatches=0;

        for(int y=static_cast<int>(quad.y0); y<static_cast<int>(quad.y1); ++y)
            for(int x=static_cast<int>(quad.x0); x<static_cast<int>(quad.x1); ++x)
            {
                const int a=coverage[static_cast<size_t>(texY+y-(int)quad.y0)*atlasWidth+texX+x-(int)quad.x0];
                const int pixel=pixelAt(frame, x, y)[0];

                if((a > 0) && (a < 255))
                    ++partial;
                if(std::abs(pixel-a) > 1)
                {
                    if(mismatches < 4)
                        MESSAGE("pixel (", x, ",", y, ") is ", pixel, ", its coverage ", a);
                    ++mismatches;
                }
            }

        CHECK(partial > 4); // the premise: the glyph has anti-aliased edges
        CHECK(mismatches == 0);
    }
}

TEST_CASE("Grayscale text: identical glyphs sit at identical whole-pixel steps, and measureText is their sum")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    for(float size:{8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 14.0f, 16.0f})
        for(const String text:{"IIIIIIII", "nnnnnnnn"})
        {
            CAPTURE(size);
            CAPTURE(text);

            const Font font=sized(size);
            TextId textId;

            if(!coverage(font))
                continue;
            drawText(textId, font, Point(10, 40), text, Color(0, 0, 0, 255));

            const std::vector<GlyphQuad> quads=glyphQuads(textId);
            REQUIRE(quads.size() == 8);

            std::set<float> steps;

            for(size_t i=0; i<quads.size(); ++i)
            {
                CHECK(isWhole(quads[i].x0));
                CHECK(quads[i].x1-quads[i].x0 == quads[0].x1-quads[0].x0);
                if(i > 0)
                    steps.insert(quads[i].x0-quads[i-1].x0);
            }

            REQUIRE(steps.size() == 1);

            const float step=*steps.begin();

            CHECK(step > 0.0f);
            // no kerning between two identical glyphs here, so the pen ends at
            // eight advances of one step each
            CHECK(renderer.measureText(font, text).x == static_cast<int>(8*step));
        }
}
