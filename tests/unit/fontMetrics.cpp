/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  renderer::Base::fontMetrics on the stub fixture (StubRenderer, which counts
 *  the calls and gives Base's estimate): the ascender, descender and line box
 *  are the same in canvas units at scales 1, 1.5 and 2, the box is lineHeight()
 *  and splits into ascender plus descender, and a font twice the size has a
 *  box twice the height. The GPU renderers' metrics are tests/fontMetrics.cpp's.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

using gweni::renderer::FontMetrics;

TEST_CASE("Font metrics: StubRenderer's are the same in canvas units at scales 1, 1.5 and 2")
{
    test::StubRenderer renderer;

    for(float size:{ 8.0f, 11.0f, 24.0f })
    {
        CAPTURE(size);

        gweni::Font font;

        font.facename="OpenSans";
        font.size=size;

        renderer.setScale(1.0f);

        const size_t calls=renderer.fontMetricsCalls;
        const FontMetrics atOne=renderer.fontMetrics(font);

        CHECK(renderer.fontMetricsCalls == calls+1);
        CHECK(atOne.lineHeight == renderer.lineHeight(font));
        CHECK(atOne.ascender > 0);
        CHECK(atOne.descender > 0);
        CHECK(atOne.ascender+atOne.descender == atOne.lineHeight);

        for(float scale:{ 1.5f, 2.0f })
        {
            CAPTURE(scale);
            renderer.setScale(scale);

            const FontMetrics scaled=renderer.fontMetrics(font);

            CHECK(scaled.ascender == atOne.ascender);
            CHECK(scaled.descender == atOne.descender);
            CHECK(scaled.lineHeight == atOne.lineHeight);
        }
    }
    renderer.setScale(1.0f);

    gweni::Font small, large;

    small.size=10.0f;
    large.size=20.0f;
    CHECK(renderer.fontMetrics(large).lineHeight == 2*renderer.fontMetrics(small).lineHeight);
}
