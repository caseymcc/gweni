/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer::fontMetrics, on whichever backend gweni_unittests runs (GL or
 *  Vulkan), for OpenSans regular and bold at coverage sizes (11, 13 px) and SDF
 *  sizes (24, 26 px):
 *  - the ascender, descender and line box are the same in canvas units at
 *    scales 1, 1.5 and 2;
 *  - the box is lineHeight() and measureText's y for one line, and splits into
 *    ascender plus descender;
 *  - at scale 1 the ascender is where renderText puts the baseline: the bottom
 *    row of an "H"'s ink (its stems end on the baseline) is the row just above
 *    pos.y + ascender, which is what lets RichLayout put runs in several fonts
 *    on one baseline;
 *  - a font that does not load has all three 0.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>

#include <memory>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::renderer::FontMetrics;
using gweni::renderer::SlotRenderer;

//! The lowest row holding a pixel at least half inked, -1 for none (an SDF
//! edge on a pixel boundary leaves a trace of coverage in the row past it)
int lowestInkRow(const std::vector<uint8_t> &frame)
{
    const Point size=gweni::test::viewSize();

    for(int y=size.y-1; y >= 0; --y)
    {
        for(int x=0; x < size.x; ++x)
        {
            const size_t i=(static_cast<size_t>(y)*size.x+x)*4;

            if((frame[i] >= 128) || (frame[i+1] >= 128) || (frame[i+2] >= 128))
                return y;
        }
    }
    return -1;
}

}//namespace

TEST_CASE("Font metrics: the same in canvas units at scales 1, 1.5 and 2, and the baseline renderText draws")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    size_t id=0;

    renderer.generateTextPrimitive(id);

    for(float size:{ 11.0f, 13.0f, 24.0f, 26.0f })
    for(bool bold:{ false, true })
    {
        CAPTURE(size);
        CAPTURE(bold);

        Font font=gweni::test::openSans();

        font.size=size;
        font.bold=bold;
        renderer.setScale(1.0f);

        const FontMetrics atOne=renderer.fontMetrics(font);

        CHECK(atOne.lineHeight == renderer.lineHeight(font));
        CHECK(atOne.lineHeight == renderer.measureText(font, "Hxg").y);
        CHECK(atOne.ascender > atOne.descender);
        CHECK(atOne.descender > 0);
        CHECK(atOne.ascender+atOne.descender == atOne.lineHeight);

        // the baseline as drawn at scale 1
        const Point pos(20, 30);
        std::vector<uint8_t> frame;

        gweni::test::clearView(Color(0, 0, 0, 255));
        renderer.begin();
        renderer.setDrawColor(Color(255, 255, 255, 255));
        renderer.renderText(id, font, pos, 1, "HHH");
        renderer.end();
        gweni::test::readView(frame);
        CHECK(lowestInkRow(frame) == pos.y+atOne.ascender-1);

        for(float scale:{ 1.5f, 2.0f })
        {
            CAPTURE(scale);
            renderer.setScale(scale);
            CHECK(renderer.fontMetrics(font) == atOne);
        }
    }

    renderer.setScale(1.0f);

    Font missing=gweni::test::openSans();

    missing.facename="NoSuchFace";
    renderer.setFallbackFont(false);
    CHECK(renderer.fontMetrics(missing) == FontMetrics());
    renderer.setFallbackFont(true);

    renderer.releaseTextPrimitive(id);
    renderer.begin();
    renderer.end();
}
