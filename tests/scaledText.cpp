/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Text at a canvas scale() other than 1: SlotRenderer draws it at
 *  font.size*scale() px (coverage or SDF, whichever that size falls in), from
 *  the scaled origin, while measureText stays in canvas units, the same at
 *  every scale, so a layout does not depend on the scale. These cases pin both,
 *  that the drawn run fills the measured box scaled, and that a run drawn again
 *  after a scale change is laid out anew, even in an SDF atlas shared by both
 *  sizes.
 *
 *  The renderer is shared with the other cases, so each case puts the scale back
 *  when it ends (ScaleGuard).
 *
 *  measureText is checked at scales 1, 1.5 and 2; a run at scale 2 is twice
 *  the scale-1 ink, inside its measured box scaled; and the run drawn again
 *  after a scale change matches one laid out fresh at that scale.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <cmath>
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

// no glyph of it reaches past its advance or below the descender
const String text="Sphinx gyp 0123";
const Point pos(4, 10);

Font openSansAt(float size)
{
    Font font=gweni::test::openSans();

    font.size=size;
    return font;
}

//! Sets the scale, and puts 1 back when the case ends.
struct ScaleGuard
{
    ScaleGuard() {}
    ~ScaleGuard() { gweni::test::gpuRenderer().setScale(1.0f); }
};

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

//! Renders text on textId alone in one frame at the renderer's scale and reads it back.
std::vector<uint8_t> drawFrame(size_t textId, const Font &font)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    std::vector<uint8_t> pixels;

    gweni::test::clearView(background);
    renderer.begin();
    renderer.setDrawColor(white);
    renderer.renderText(textId, font, pos, 1, text);
    renderer.end();

    gweni::test::readView(pixels);
    return pixels;
}

//! A text id for one case, released (and that flushed) when it ends.
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

//! The run drawn alone on a new text id at scale.
std::vector<uint8_t> drawnAt(float scale, const Font &font)
{
    gweni::test::gpuRenderer().setScale(scale);

    TextId textId;

    return drawFrame(textId.id, font);
}

}//namespace

TEST_CASE("Scaled text: measureText is in canvas units at any scale")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    ScaleGuard guard;

    // below the SDF threshold, across it at a scale, and above it
    for(float size:{ 9.0f, 11.0f, 24.0f })
    {
        CAPTURE(size);

        const Font font=openSansAt(size);

        renderer.setScale(1.0f);

        const Point unscaled=renderer.measureText(font, text);

        for(float scale:{ 1.5f, 2.0f })
        {
            CAPTURE(scale);
            renderer.setScale(scale);

            const Point measured=renderer.measureText(font, text);

            CHECK(measured.x == unscaled.x);
            CHECK(measured.y == unscaled.y);
        }
    }
}

TEST_CASE("Scaled text: the run is drawn at size*scale in its measured box, scaled")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    ScaleGuard guard;
    const float scale=2.0f;

    for(float size:{ 11.0f, 24.0f })
    {
        CAPTURE(size);

        const Font font=openSansAt(size);

        renderer.setScale(1.0f);

        const Point measured=renderer.measureText(font, text);
        const Ink unscaled=inkOf(drawnAt(1.0f, font));
        const Ink scaled=inkOf(drawnAt(scale, font));

        REQUIRE(unscaled.pixels > 50);
        REQUIRE(scaled.pixels > 50);

        // inside the measured box at its scaled place
        CHECK(scaled.x0 >= SlotRenderer::snap(float(pos.x), scale));
        CHECK(scaled.y0 >= SlotRenderer::snap(float(pos.y), scale));
        CHECK(scaled.x1 <= SlotRenderer::snap(float(pos.x+measured.x), scale));
        CHECK(scaled.y1 <= SlotRenderer::snap(float(pos.y+measured.y), scale));

        // twice the ink of scale 1 in each direction, give or take the rounding of
        // the two sizes' metrics and anti-aliased edges
        CHECK(std::abs((scaled.x1-scaled.x0)-2*(unscaled.x1-unscaled.x0)) <= 4);
        CHECK(std::abs((scaled.y1-scaled.y0)-2*(unscaled.y1-unscaled.y0)) <= 4);
        CHECK(std::abs((scaled.x0-SlotRenderer::snap(float(pos.x), scale))-2*(unscaled.x0-pos.x)) <= 2);
        CHECK(std::abs((scaled.y0-SlotRenderer::snap(float(pos.y), scale))-2*(unscaled.y0-pos.y)) <= 3);
        // the last glyph ends where the measured width puts it, not short of it
        CHECK(scaled.x1 >= SlotRenderer::snap(float(pos.x+measured.x), scale)-int(size*scale/4));
    }
}

TEST_CASE("Scaled text: a run drawn again at a new scale is laid out anew")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    ScaleGuard guard;

    // 11 px moves to another atlas at scale 2 (unless the threshold keeps both
    // in one); 24 px and 48 px share the face's SDF atlas, so only the run's own
    // effective size tells the two apart
    for(float size:{ 11.0f, 24.0f })
    {
        CAPTURE(size);

        const Font font=openSansAt(size);
        const std::vector<uint8_t> fresh=drawnAt(2.0f, font);
        TextId textId;

        renderer.setScale(1.0f);

        const std::vector<uint8_t> atOne=drawFrame(textId.id, font);

        renderer.setScale(2.0f);

        const std::vector<uint8_t> atTwo=drawFrame(textId.id, font);

        // as bools: doctest would print every byte of a failing frame
        const bool moved=(atOne != atTwo);
        const bool asFresh=(atTwo == fresh);

        CHECK(moved);
        CHECK(asFresh);
    }
}
