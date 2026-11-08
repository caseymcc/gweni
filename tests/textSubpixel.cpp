/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's LCD text on the subpixel grid: in an LCD mode a glyph's pen is
 *  the exact sum of the unhinted advances and kerning, quantised to 1/3 device
 *  px. The quad sits on the whole pixel below it, and the phase (0, 1 or 2
 *  thirds past it) picks the image freetype-gl rasterised that far right, which
 *  the glyph vertexes carry as shift = phase/3. The exact pens are worked out
 *  here with FreeType directly, from the face's unhinted metrics.
 *
 *  The LCD contrast curve (setTextContrast) raises white text's coverage, and
 *  leaves black text and grayscale text exactly as they are.
 *
 *  This is the coverage path: a size that GWENI_SDF_TEXT_MIN puts in the SDF
 *  range, which stays grayscale, is skipped.
 *
 *  In rgb mode, for "IIIIIIII"/"nnnnnnnn" at 8-16 px (and at scale 1.5), each
 *  glyph's quad x plus phase/3 is within 1/3 device px of those pens, every
 *  phase is used, and measureText's width is the sum rounded up. Three phases
 *  of 8-16 px ASCII fill a few percent of the atlas. Under the contrast curve
 *  black text and grayscale stay byte for byte.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cmath>
#include <cstring>
#include <set>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::GLTextMode;
using gweni::renderer::OpenGLCore;
using gweni::renderer::TextAA;

const Color white(255, 255, 255, 255);
const Color black(0, 0, 0, 255);

Font sized(float size)
{
    Font font=gweni::test::openSans();

    font.size=size;
    return font;
}

//! Puts the shared renderer's mode, contrast and scale back when a case ends.
struct RestoreRenderer
{
    RestoreRenderer():
        aa(gweni::test::gpuRenderer().textAA()),
        contrast(gweni::test::gpuRenderer().textContrast())
    {}

    ~RestoreRenderer()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.setTextAA(aa);
        renderer.setTextContrast(contrast);
    }

    TextAA aa;
    float contrast;
};

//! A text id for one test case, released (and that flushed) when it ends.
struct TextId
{
    TextId() { gweni::test::gpuRenderer().generateTextPrimitive(id); }

    ~TextId()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
    }

    size_t id=0;
};

void clearFrame(const Color &background)
{
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

std::vector<uint8_t> drawText(const TextId &textId, const Font &font, Point pos, const String &text,
    const Color &color, const Color &background)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Point size=gweni::test::viewSize();
    std::vector<uint8_t> pixels;

    clearFrame(background);
    renderer.begin();
    renderer.setDrawColor(color);
    renderer.renderText(textId.id, font, pos, 1, text);
    renderer.end();
    gweni::capture::readFramebufferRgba(size.x, size.y, pixels);
    return pixels;
}

//! The first vertex of each glyph quad of a text id's run, in glyph order: its
//! top-left corner, and the phase it was drawn at in shift.
std::vector<ftgl::glyph_vertex_t> glyphCorners(const TextId &textId)
{
    const OpenGLCore::MirrorSnapshot snapshot=gweni::test::gpuRenderer().snapshotMirror();
    const std::vector<uint8_t> &bytes=snapshot.texts.at(textId.id);
    const size_t vertexSize=sizeof(ftgl::glyph_vertex_t);
    std::vector<ftgl::glyph_vertex_t> corners;

    // the hidden flag, then 4 vertexes per glyph, (x0,y0) first
    REQUIRE((bytes.size()-1)%(4*vertexSize) == 0);
    for(size_t offset=1; offset < bytes.size(); offset+=4*vertexSize)
    {
        ftgl::glyph_vertex_t v;

        std::memcpy(&v, &bytes[offset], sizeof(v));
        corners.push_back(v);
    }
    return corners;
}

//! The face's own unhinted horizontal metrics at a px size, as FreeType gives them.
struct UnhintedFace
{
    explicit UnhintedFace(float px)
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();
        const String path=renderer.getResourcePaths().getPath(gweni::ResourcePaths::Type::Font,
            gweni::test::openSans().facename);

        REQUIRE(FT_Init_FreeType(&library) == 0);
        REQUIRE(FT_New_Face(library, path.c_str(), 0, &face) == 0);
        // the char size freetype-gl sets: 26.6 px at 72 dpi
        REQUIRE(FT_Set_Char_Size(face, (FT_F26Dot6)(px*64), 0, 72, 72) == 0);
    }

    ~UnhintedFace()
    {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
    }

    double advance(char c)
    {
        REQUIRE(FT_Load_Glyph(face, FT_Get_Char_Index(face, (FT_ULong)c), FT_LOAD_NO_HINTING) == 0);
        return face->glyph->linearHoriAdvance/65536.0;
    }

    double kerning(char left, char right)
    {
        FT_Vector k={0, 0};

        FT_Get_Kerning(face, FT_Get_Char_Index(face, (FT_ULong)left), FT_Get_Char_Index(face, (FT_ULong)right),
            FT_KERNING_UNFITTED, &k);
        return k.x/64.0;
    }

    //! The exact pen of each glyph of a run, from 0
    std::vector<double> pens(const String &text)
    {
        std::vector<double> result;
        double pen=0.0;

        for(size_t i=0; i < text.size(); ++i)
        {
            if(i > 0)
                pen+=kerning(text[i-1], text[i]);
            result.push_back(pen);
            pen+=advance(text[i]);
        }
        result.push_back(pen); // where the run ends
        return result;
    }

    FT_Library library=nullptr;
    FT_Face face=nullptr;
};

bool isWhole(float v)
{
    return v == std::floor(v);
}

//! Checks each glyph of a run drawn at origin (device px) against the exact pens
//! of the face at px, and adds the phases it used to `phases`.
void checkPens(const TextId &textId, const Font &font, float originX, float px, const String &text,
    std::set<int> &phases)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    ftgl::texture_font_t *textureFont=renderer.getTextureFont(font);

    REQUIRE(textureFont != nullptr);
    CHECK(textureFont->subpixel_positions == 3);

    UnhintedFace face(px);
    const std::vector<double> exact=face.pens(text);
    const std::vector<ftgl::glyph_vertex_t> corners=glyphCorners(textId);

    REQUIRE(corners.size() == text.size());
    for(size_t i=0; i < corners.size(); ++i)
    {
        CAPTURE(i);

        const int phase=(int)std::lround(corners[i].shift*3.0f);
        const char codepoint[2]={text[i], '\0'};
        const ftgl::texture_glyph_t *image=ftgl::texture_font_get_glyph_phase(textureFont, codepoint, phase);

        CHECK(std::fabs(corners[i].shift*3.0f-phase) < 1e-4f);
        REQUIRE((phase >= 0 && phase < 3));
        REQUIRE(image != nullptr);
        CHECK(image->phase == phase);

        // the quad's whole pixel, where the pen's image is drawn from
        const float quadX=corners[i].x-originX-image->offset_x;

        CHECK(isWhole(quadX));

        const double position=quadX+phase/3.0;

        CAPTURE(position);
        CAPTURE(exact[i]);
        // rounded to the nearest third: within 1/6 px, and so within the 1/3 asked
        CHECK(std::fabs(position-exact[i]) <= 1.0/6.0+1e-3);
        phases.insert(phase);
    }
}

bool lcdRange(const Font &font)
{
    return gweni::test::gpuRenderer().getTextMode(font) == GLTextMode::LcdRGB;
}

}//namespace

TEST_CASE("LCD text: each glyph is drawn within 1/3 px of its unhinted pen, and the width is the advances'")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    RestoreRenderer restore;
    std::set<int> phases;

    renderer.setTextAA(TextAA::LcdRGB);

    for(float size=8; size <= 16; ++size)
        for(const String text:{"IIIIIIII", "nnnnnnnn"})
        {
            CAPTURE(size);
            CAPTURE(text);

            const Font font=sized(size);

            if(!lcdRange(font))
                continue;

            TextId textId;

            drawText(textId, font, Point(10, 40), text, white, black);
            checkPens(textId, font, 10.0f, size, text, phases);

            // the pen ends at the sum of the unhinted advances (and kerning), which
            // measureText rounds up to whole canvas units
            UnhintedFace face(size);
            const double sum=face.pens(text).back();
            const int width=renderer.measureText(font, text).x;

            CAPTURE(sum);
            CHECK(width >= sum-1e-3);
            CHECK(width < sum+1.0);
        }

    // the premise: the runs put glyphs at every third of a pixel
    CHECK(phases == std::set<int>{0, 1, 2});
}

TEST_CASE("LCD text: at a canvas scale the glyphs sit within 1/3 device px of the unhinted pens at size*scale")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    RestoreRenderer restore;
    std::set<int> phases;

    renderer.setTextAA(TextAA::LcdRGB);
    renderer.setScale(1.5f);

    for(float size:{8.0f, 9.0f, 10.0f, 11.0f})
        for(const String text:{"IIIIIIII", "nnnnnnnn"})
        {
            CAPTURE(size);
            CAPTURE(text);

            const Font font=sized(size);

            if(!lcdRange(font))
                continue;

            TextId textId;

            // canvas x 10 is device px 15 at 1.5
            drawText(textId, font, Point(10, 40), text, white, black);
            checkPens(textId, font, 15.0f, size*1.5f, text, phases);
        }
    CHECK(phases.size() > 1);
}

TEST_CASE("LCD text: three phases of 8-16 px ASCII fit their atlases with room to spare")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    RestoreRenderer restore;

    renderer.setTextAA(TextAA::LcdRGB);

    for(float size=8; size <= 16; ++size)
    {
        CAPTURE(size);

        const Font font=sized(size);

        if(!lcdRange(font))
            continue;
        REQUIRE(renderer.ensureFont(font));

        ftgl::texture_font_t *textureFont=renderer.getTextureFont(font);

        REQUIRE(textureFont != nullptr);
        for(char c=' '; c <= '~'; ++c)
        {
            const char codepoint[2]={c, '\0'};

            for(int phase=0; phase < 3; ++phase)
                CHECK(ftgl::texture_font_get_glyph_phase(textureFont, codepoint, phase) != nullptr);
        }

        const texture_atlas_t *atlas=textureFont->atlas;
        const double fill=(double)atlas->used/(double)(atlas->width*atlas->height);

        MESSAGE(size, " px: ", textureFont->glyphs->size, " glyph images, atlas ", atlas->width, "x", atlas->height,
            " depth ", atlas->depth, ", ", fill*100.0, " % used");
        CHECK(atlas->depth == 3);
        CHECK(fill < 0.25);
        renderer.freeFont(font);
    }
}

TEST_CASE("LCD text: the contrast curve raises white text's coverage, and leaves black text and grayscale as they are")
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    RestoreRenderer restore;
    const Font font=sized(11);
    const Color grey(128, 128, 128, 255);

    auto inkSum=[](const std::vector<uint8_t> &frame, const Color &background)
    {
        const int rgb[3]={background.r, background.g, background.b};
        double sum=0.0;

        for(size_t i=0; i < frame.size(); i+=4)
            for(size_t c=0; c < 3; ++c)
                sum+=std::abs((int)frame[i+c]-rgb[c]);
        return sum;
    };

    for(TextAA aa:{TextAA::Grayscale, TextAA::LcdRGB})
    {
        renderer.setTextAA(aa);
        if(renderer.getTextMode(font) == GLTextMode::Sdf)
            return;

        const bool lcd=(aa == TextAA::LcdRGB);

        CAPTURE(lcd);

        TextId textId;
        std::vector<uint8_t> whiteOnBlack[2], blackOnWhite[2], whiteOnGrey[2];

        for(int i=0; i < 2; ++i)
        {
            renderer.setTextContrast(i?2.0f:0.0f);
            whiteOnBlack[i]=drawText(textId, font, Point(10, 40), "Illumination 0123", white, black);
            blackOnWhite[i]=drawText(textId, font, Point(10, 40), "Illumination 0123", black, white);
            whiteOnGrey[i]=drawText(textId, font, Point(10, 40), "Illumination 0123", white, grey);
        }

        // black has no luminance to boost by
        CHECK(blackOnWhite[0] == blackOnWhite[1]);
        if(!lcd)
        {
            CHECK(whiteOnBlack[0] == whiteOnBlack[1]);
            CHECK(whiteOnGrey[0] == whiteOnGrey[1]);
            continue;
        }

        const double ink0=inkSum(whiteOnBlack[0], black);
        const double ink1=inkSum(whiteOnBlack[1], black);

        CHECK(ink0 > 1000.0); // the premise: something was drawn
        CHECK(ink1 > ink0*1.1);
        CHECK(inkSum(whiteOnGrey[1], grey) > inkSum(whiteOnGrey[0], grey));
    }
}
