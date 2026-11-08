/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer::lineHeight(font) reads the line box from the font's metrics
 *  instead of laying a string out, and is exactly measureText(font, " ").y:
 *  for OpenSans at 8, 11, 14, 17, 18, 24, 36 and 48 px (both sides of the SDF
 *  threshold), regular and bold, at scales 1, 1.5 and 2, in the grayscale and
 *  both LCD text modes. It is the same at every scale, as measureText is. A
 *  missing face gives the built-in fallback's line height, and with the
 *  fallback off it is 0, as measureText is (0,0).
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

namespace
{

using gweni::Font;
using gweni::String;
using gweni::renderer::TextAA;

//! Puts the shared renderer's text AA mode, scale and fallback font back when a
//! case ends.
struct Restore
{
    Restore(): aa(gweni::test::gpuRenderer().textAA()) {}

    ~Restore()
    {
        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setTextAA(aa);
        renderer.setScale(1.0f);
        renderer.setFallbackFont(true);
    }

    TextAA aa;
};

Font makeFont(const String &facename, float size, bool bold=false)
{
    Font font;

    font.facename=facename;
    font.size=size;
    font.bold=bold;
    return font;
}

}//namespace

TEST_CASE("lineHeight: measureText's y for \" \" in every size, weight, scale and text mode")
{
    gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();
    Restore restore;

    const TextAA modes[]={TextAA::Grayscale, TextAA::LcdRGB, TextAA::LcdBGR};
    const float sizes[]={8, 11, 14, 17, 18, 24, 36, 48};
    const float scales[]={1.0f, 1.5f, 2.0f};

    for(TextAA aa:modes)
    {
        renderer.setTextAA(aa);
        if(renderer.textAA() != aa)
        {
            MESSAGE("skipped: text AA mode ", (int)aa, " is not available");
            continue;
        }

        for(bool bold:{false, true})
        {
            for(float size:sizes)
            {
                const Font font=makeFont("OpenSans", size, bold);
                int atScale1=0;

                for(float scale:scales)
                {
                    CAPTURE((int)aa);
                    CAPTURE(bold);
                    CAPTURE(size);
                    CAPTURE(scale);
                    renderer.setScale(scale);

                    const int height=renderer.lineHeight(font);

                    CHECK(height > 0);
                    CHECK(height == renderer.measureText(font, " ").y);
                    // a one-line string's height is the line box, whatever the string
                    CHECK(height == renderer.measureText(font, "Hello, gweni!").y);
                    CHECK(height == renderer.measureText(font, "").y);

                    if(scale == 1.0f)
                        atScale1=height;
                    CHECK(height == atScale1);
                }
            }
        }
    }
}

TEST_CASE("lineHeight: a missing face has the fallback's line height, and 0 with the fallback off")
{
    gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();
    Restore restore;

    for(float size:{11.0f, 24.0f})
    {
        CAPTURE(size);

        const Font missing=makeFont("NoSuchLineHeightFace", size);
        const Font openSans=makeFont("OpenSans", size);

        CHECK(renderer.lineHeight(missing) == renderer.measureText(missing, " ").y);
        CHECK(renderer.lineHeight(missing) == renderer.lineHeight(openSans));
    }

    renderer.setFallbackFont(false);
    for(float size:{11.0f, 24.0f})
    {
        CAPTURE(size);

        const Font missing=makeFont("NoSuchLineHeightFaceNoFallback", size);

        CHECK(renderer.measureText(missing, " ").y == 0);
        CHECK(renderer.lineHeight(missing) == 0);
    }
}
