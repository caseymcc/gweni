/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  renderText's colour-span overload without a device: renderer::Base's default
 *  sets the draw colour to the first span's and draws the plain run (the draw
 *  colour left alone with no spans), and StubRenderer records each text id's
 *  spans in `textSpans` (a plain renderText records none) while counting the
 *  run's glyph slots as the plain call does.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

using namespace gweni;
using ::test::StubRenderer;
using renderer::ColorSpan;

namespace
{

//! A StubRenderer that records the plain renderText calls and every draw colour
struct PlainCallRecorder: StubRenderer
{
    void setDrawColor(Color color) override
    {
        StubRenderer::setDrawColor(color);
        colors.push_back(color);
    }

    void renderText(size_t textId, const Font &font, Point pos, int zIndex, const String &text) override
    {
        plainCalls.push_back(text);
        StubRenderer::renderText(textId, font, pos, zIndex, text);
    }
    using StubRenderer::renderText;

    std::vector<String> plainCalls;
    std::vector<Color> colors;
};

const Color red(255, 0, 0, 255);
const Color green(0, 255, 0, 255);

bool sameColor(const Color &a, const Color &b)
{
    return (a.r == b.r) && (a.g == b.g) && (a.b == b.b) && (a.a == b.a);
}

}//namespace

TEST_CASE("Colour spans: renderer::Base's default draws the run in the first span's colour")
{
    PlainCallRecorder renderer;
    const std::vector<ColorSpan> spans={ ColorSpan{0, 2, red}, ColorSpan{3, 5, green} };

    renderer.setDrawColor(Color(1, 2, 3, 255));
    renderer.colors.clear();
    // the base class's overload, not the stub's
    renderer.renderer::Base::renderText(4, Font(), Point(0, 0), 0, "ab cd", spans);

    CHECK(renderer.plainCalls == std::vector<String>{ "ab cd" });
    REQUIRE(renderer.colors.size() == 1);
    CHECK(sameColor(renderer.colors.front(), red));
    CHECK(sameColor(renderer.drawColor, red));
    CHECK(renderer.textGlyphs[4] == 5);

    SUBCASE("no spans: the draw colour as it was")
    {
        renderer.colors.clear();
        renderer.renderer::Base::renderText(4, Font(), Point(0, 0), 0, "ab", std::vector<ColorSpan>());
        CHECK(renderer.colors.empty());
        CHECK(renderer.plainCalls.back() == "ab");
    }
}

TEST_CASE("Colour spans: StubRenderer records each text id's spans")
{
    StubRenderer renderer;
    const std::vector<ColorSpan> spans={ ColorSpan{0, 2, red}, ColorSpan{3, 5, green} };
    const size_t before=renderer.spanRenders;

    renderer.renderText(7, Font(), Point(0, 0), 0, "ab cd", spans);
    renderer.end();

    CHECK(renderer.spanRenders == before+1);
    CHECK((renderer.textSpans[7] == spans));
    // counted as the plain call counts the run
    CHECK(renderer.textGlyphs[7] == 5);
    CHECK(renderer.frameStats().textSlotsWritten == 5);

    // another id's spans are its own
    renderer.renderText(8, Font(), Point(0, 0), 0, "x", std::vector<ColorSpan>{ ColorSpan{0, 1, green} });
    CHECK((renderer.textSpans[7] == spans));
    CHECK(renderer.textSpans[8].size() == 1);

    // the plain call records none
    renderer.renderText(7, Font(), Point(0, 0), 0, "ab cd");
    CHECK(renderer.textSpans[7].empty());
}
