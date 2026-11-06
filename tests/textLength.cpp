/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore must hand freetype-gl's text_buffer_add_text a code-point count,
 *  not the string's byte count. With bytes, a multi-byte UTF-8 string makes
 *  freetype-gl read past the terminating NUL and measure or draw whatever
 *  follows it as extra glyphs (fixed in cca344a; it showed as ListBoxDemo
 *  output that changed with ASLR). These tests put known bytes after the NUL,
 *  so that over-read changes the result every time instead of by luck.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <freetype-gl/utf8-utils.h>

namespace
{

using gweni::String;

//! "€8.95" in a string whose bytes past the terminator are all `fill`:
//! assign() keeps the 64-byte capacity and only rewrites what it copies.
String withTrailingBytes(char fill)
{
    String s(64, fill);

    s.assign(u8"€8.95");
    return s;
}

//! Draws `text` alone in one frame and returns how many glyph quads that wrote.
//
//! The text id is new, so every quad of its run is a slot written this frame,
//! and FrameStats counts exactly those.
size_t renderedGlyphs(const String &text)
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    size_t textId=0;

    renderer.generateTextPrimitive(textId);
    renderer.begin();
    renderer.setDrawColor(gweni::Color(255, 255, 255, 255));
    renderer.renderText(textId, gweni::test::openSans(), gweni::Point(4, 20), 1, text);
    renderer.end();

    const size_t glyphs=renderer.getFrameStats().textSlotsWritten;

    // leave nothing behind for the next test's frame
    renderer.releaseTextPrimitive(textId);
    renderer.begin();
    renderer.end();
    return glyphs;
}

}//namespace

TEST_CASE("UTF-8 text: the bytes after the terminator do not change measureText or renderText")
{
    const String trailingW=withTrailingBytes('W');
    const String trailingNul=withTrailingBytes('\0');

    // the premise: same string, but different bytes past its end
    REQUIRE(trailingW == trailingNul);
    REQUIRE(trailingW.size() == 7);
    REQUIRE(trailingW.c_str()[8] == 'W');
    REQUIRE(trailingNul.c_str()[8] == '\0');

    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();

    SUBCASE("measureText")
    {
        const int widthW=renderer.measureText(gweni::test::openSans(), trailingW).x;
        const int widthNul=renderer.measureText(gweni::test::openSans(), trailingNul).x;

        CHECK(widthW > 0);
        CHECK(widthW == widthNul);
    }

    SUBCASE("renderText draws one glyph per code point")
    {
        CHECK(ftgl::utf8_strlen(trailingW.c_str()) == 5);
        CHECK(renderedGlyphs(trailingW) == 5);
        CHECK(renderedGlyphs(trailingNul) == 5);
    }
}

TEST_CASE("UTF-8 text: a heap string of exact size is measured and drawn to its end")
{
    // Longer than the 15-byte small-string buffer and built from a literal, so it
    // lives in a heap allocation that ends at its NUL: reading past it is a heap
    // over-read, which an ASAN build reports. Without ASAN it passes either way.
    const String text(u8"Курица €8.95 £3.50");

    REQUIRE(text.size() > 15);

    const size_t codePoints=ftgl::utf8_strlen(text.c_str());

    CHECK(codePoints == 18);
    CHECK(gweni::test::gpuRenderer().measureText(gweni::test::openSans(), text).x > 0);
    CHECK(renderedGlyphs(text) == codePoints);
}
