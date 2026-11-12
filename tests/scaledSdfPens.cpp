/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A font of 18 px or more is measured as SDF (measureText and measureCarets
 *  at font.size, in canvas units) and, at a scale() that puts font.size*scale
 *  below the SDF threshold, drawn from a hinted grayscale coverage atlas at
 *  that whole px size (20 px at 0.75 is drawn at 15 px). Its glyphs must still
 *  be drawn where they are measured: each at its measured pen times the scale
 *  rounded to a whole device px (SlotRenderer::measuredPenShifts), the rule a
 *  coverage-measured run follows, and not at the drawn font's own hinted pens,
 *  which drift off the measured ones along the line, so a TextBox's caret and
 *  selection would leave the glyphs.
 *
 *  The setups are OpenSans 20 and 24 px, regular and bold (a font of its own,
 *  drawn from the same face), at scales 0.75, 0.8, 0.7 and 0.5 (a ZoomPanel
 *  zoomed out); one whose sizes are not in those modes here is skipped: 24 px
 *  at 0.75 and 0.8 is 18 and 19.2 px, drawn as SDF, and GWENI_SDF_TEXT_MIN or
 *  GWENI_TEXT_AA can move the others. The measured pens are
 *  read from the same run laid out at scale 1, where it is drawn as SDF at the
 *  pens it is measured at. A kerning string ("AVATAR Toyota WAVE fly") is drawn
 *  at the scale, and each glyph's quad must sit at its measured pen times the
 *  scale, rounded. Then from the pixels: the ink of each prefix ending on an
 *  inked glyph, less that glyph's ink drawn alone at the same origin, is where
 *  the glyph was drawn, which must be within 1 device px of its measured pen
 *  scaled; adding the drawn glyph's advance gives its advance edge, which must
 *  be within 1 device px of the unit measureCarets()[k] rounds it up to (the
 *  caret after it, at (caret-1)*scale..caret*scale), and the last one of the
 *  unit measureText().x rounds the run's width up to, from the snapped origin.
 *
 *  Before the fix, drawn at the coverage font's own hinted pens, the worst
 *  glyph quad was this far (device px) off its measured pen scaled, the worst
 *  glyph's ink this far, and the run's last advance edge this far outside
 *  measureText's unit; bold the same as regular (GL; Vulkan the same):
 *
 *      20 px at 0.75: quad 0.86, ink 0.86, end 0.25
 *      20 px at 0.8:  quad 1.38, ink 1.28, end 0.20
 *      20 px at 0.7:  quad 1.07, ink 1.07, end 0.30
 *      20 px at 0.5:  quad 2.70, ink 2.70, end 2.50
 *      24 px at 0.7:  quad 1.37, ink 1.37, end 0.50
 *      24 px at 0.5:  quad 2.86, ink 2.74, end 2.50
 *
 *  With the fix every quad is within 0.5 px (0.47 to 0.50) and every glyph's
 *  ink too, and the ends within 0.5 px of the unit.
 *
 *  The renderer is shared with the other cases; the case puts the scale and the
 *  text AA mode back when it ends.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::SlotRenderer;
using gweni::renderer::TextMode;

const Color background(0, 0, 255, 255);
const Color white(255, 255, 255, 255);

// kerning pairs (AV, VA, AT, To, Yo, WA, AV, fl) and a long enough line for
// the hinted advances to drift
const String text="AVATAR Toyota WAVE fly";
const Point pos(5, 10);

//! Puts the shared renderer's scale and text AA mode back when the case ends.
struct Restore
{
    Restore(): aa(gweni::test::gpuRenderer().textAA()) {}

    ~Restore()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.setTextAA(aa);
    }

    gweni::renderer::TextAA aa;
};

//! A text id for one draw, released (and that flushed) when it goes.
struct TextId
{
    TextId() { gweni::test::gpuRenderer().generateTextPrimitive(id); }

    ~TextId()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
    }

    size_t id=0;
};

//! Renders text alone on textId in one frame at the renderer's scale and reads it back.
std::vector<uint8_t> drawFrame(const TextId &textId, const Font &font, const String &text)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    std::vector<uint8_t> pixels;

    gweni::test::clearView(background);
    renderer.begin();
    renderer.setDrawColor(white);
    renderer.renderText(textId.id, font, pos, 1, text);
    renderer.end();
    gweni::test::readView(pixels);
    return pixels;
}

bool isBackground(const std::vector<uint8_t> &frame, size_t pixel)
{
    const uint8_t *p=&frame[pixel*4];

    return (p[0] == background.r) && (p[1] == background.g) && (p[2] == background.b);
}

//! One past the rightmost inked column, 0 if nothing is inked.
int inkEnd(const std::vector<uint8_t> &frame)
{
    const Point size=gweni::test::viewSize();
    int end=0;

    for(int y=0; y < size.y; ++y)
        for(int x=end; x < size.x; ++x)
            if(!isBackground(frame, size_t(y*size.x+x)))
                end=x+1;
    return end;
}

//! One past the rightmost column where two frames differ, 0 if they do not.
int changedEnd(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
{
    const Point size=gweni::test::viewSize();
    int end=0;

    for(int y=0; y < size.y; ++y)
        for(int x=end; x < size.x; ++x)
            if(std::memcmp(&a[size_t(y*size.x+x)*4], &b[size_t(y*size.x+x)*4], 4) != 0)
                end=x+1;
    return end;
}

//! The first vertex of each glyph quad of a text id's run, in glyph order.
std::vector<ftgl::glyph_vertex_t> glyphCorners(const TextId &textId)
{
    const SlotRenderer::MirrorSnapshot snapshot=gweni::test::gpuRenderer().snapshotMirror();
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

//! The glyph of an ASCII character in the font the renderer draws font in now.
const ftgl::texture_glyph_t *drawnGlyph(const Font &font, char c)
{
    ftgl::texture_font_t *textureFont=gweni::test::gpuRenderer().getTextureFont(font);
    const char codepoint[2]={ c, '\0' };

    REQUIRE(textureFont != nullptr);
    return ftgl::texture_font_get_glyph(textureFont, codepoint);
}

//! The pen of each glyph of text in canvas units, as the run is measured: laid
//! out at scale 1, where a size measured as SDF is drawn as SDF at the pens it
//! is measured at, each quad at the pen plus its glyph's offset scaled.
std::vector<float> measuredPens(const Font &font)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.setScale(1.0f);
    REQUIRE(renderer.getTextMode(font) == TextMode::Sdf);

    TextId textId;

    drawFrame(textId, font, text);

    ftgl::texture_font_t *textureFont=renderer.getTextureFont(font);

    REQUIRE(textureFont != nullptr);
    REQUIRE(textureFont->size > 0.0f);

    const float s=font.size/textureFont->size;
    const std::vector<ftgl::glyph_vertex_t> corners=glyphCorners(textId);
    std::vector<float> pens;

    REQUIRE(corners.size() == text.size());
    for(size_t i=0; i < corners.size(); ++i)
        pens.push_back(corners[i].x-float(pos.x)-drawnGlyph(font, text[i])->offset_x*s);
    return pens;
}

}//namespace

TEST_CASE("Scaled text: an SDF-measured run drawn as coverage is at its measured pens")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    Restore restore;
    const float tolerance=1.0f;

    for(float size:{ 20.0f, 24.0f })
    for(bool bold:{ false, true })
    for(float scale:{ 0.75f, 0.8f, 0.7f, 0.5f })
    {
        CAPTURE(size);
        CAPTURE(bold);
        CAPTURE(scale);

        Font font=gweni::test::openSans();

        font.size=size;
        font.bold=bold;

        // measured as SDF at font.size, drawn as grayscale coverage at size*scale
        renderer.setScale(1.0f);

        const bool measuredSdf=(renderer.getTextMode(font) == TextMode::Sdf);

        renderer.setScale(scale);
        if(!measuredSdf || (renderer.getTextMode(font) != TextMode::Coverage))
        {
            MESSAGE("skipped: ", size, " px at scale ", scale, " is not measured as SDF and drawn as coverage here");
            continue;
        }

        const std::vector<float> pens=measuredPens(font);

        renderer.setScale(scale);

        const int origin=SlotRenderer::snap(float(pos.x), scale);
        const Point measured=renderer.measureText(font, text);
        std::vector<int> carets;

        renderer.measureCarets(font, text, carets);
        REQUIRE(carets.size() == text.size()+1);

        // each quad at its measured pen scaled, rounded to a whole device px
        {
            TextId run;

            drawFrame(run, font, text);

            const std::vector<ftgl::glyph_vertex_t> corners=glyphCorners(run);

            REQUIRE(corners.size() == text.size());
            for(size_t i=0; i < corners.size(); ++i)
            {
                CAPTURE(i);

                const float quadPen=corners[i].x-float(origin)-drawnGlyph(font, text[i])->offset_x;

                CAPTURE(quadPen);
                CAPTURE(pens[i]*scale);
                CHECK(quadPen == std::floor(quadPen));
                CHECK(std::fabs(quadPen-pens[i]*scale) <= 0.5f+1e-3f);
            }
        }

        // from the pixels: where each inked glyph was drawn, from its prefix's ink
        // less the glyph's own ink drawn alone at the origin
        int inked=0;
        std::vector<uint8_t> before;

        {
            TextId empty;

            before=drawFrame(empty, font, String());
        }

        for(size_t k=1; k <= text.size(); ++k)
        {
            CAPTURE(k);

            const char c=text[k-1];
            std::vector<uint8_t> prefix, alone;

            {
                TextId prefixId;

                prefix=drawFrame(prefixId, font, text.substr(0, k));
            }
            {
                TextId aloneId;

                alone=drawFrame(aloneId, font, String(1, c));
            }

            const int aloneEnd=inkEnd(alone);

            if(aloneEnd > 0)
            {
                ++inked;
                // the glyph's own ink is the rightmost change it made to the prefix before it
                const float drawnPen=float(changedEnd(before, prefix)-aloneEnd);
                const float penError=drawnPen-pens[k-1]*scale;
                const float edge=float(origin)+drawnPen+drawnGlyph(font, c)->advance_x;

                CAPTURE(drawnPen);
                CAPTURE(pens[k-1]*scale);
                CHECK(std::fabs(penError) <= tolerance);

                // the caret after it, which measureCarets rounds up to a whole unit
                CAPTURE(edge);
                CAPTURE(carets[k]);
                CHECK(edge >= float(origin)+(carets[k]-1)*scale-tolerance);
                CHECK(edge <= float(origin)+carets[k]*scale+tolerance);

                if(k == text.size())
                {
                    // measureText's width, likewise rounded up
                    const float low=float(origin)+(measured.x-1)*scale;
                    const float high=float(origin)+measured.x*scale;

                    CHECK(edge >= low-tolerance);
                    CHECK(edge <= high+tolerance);
                }
            }
            before.swap(prefix);
        }

        CHECK(inked > 15);
    }
}
