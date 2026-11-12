/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The default-ignorable codepoints, U+200B-200D (zero width space, non-joiner,
 *  joiner), U+2060 (word joiner), U+FE00-FE0F (variation selectors) and U+FEFF,
 *  draw nothing: no glyph slot, no advance, in grayscale coverage, LcdRGB (where
 *  the device can blend it) and the SDF range, in a face that has a glyph for
 *  one (OpenSans has U+200B) and one that has none (DejaVu Sans Mono), and they
 *  never send a run down its glyph fallback chain. These cases pin, for each:
 *  - "a<cp>b" draws exactly the pixels of "ab" and writes as many glyph slots;
 *  - it measures as "ab", and its carets are "ab"'s with the one before the
 *    ignorable repeated after it (measureCarets, measureText of each prefix);
 *  - the codepoint alone measures 0 wide, draws nothing and writes no slot;
 *  - "⚠️" (U+26A0 U+FE0F) draws and measures as "⚠" alone.
 *  utf8_is_ignorable, the predicate every walk shares, is checked at the edges
 *  of each range and on a string that ends inside a sequence.
 *
 *  Each case uses a renderer of its own with the default chain [DejaVuSansMono],
 *  as the skin sets it.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>
#include <gweni/utility.h>

#include <freetype-gl/utf8-utils.h>

#include <memory>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::SlotRenderer;
using gweni::renderer::TextAA;
using gweni::renderer::TextMode;

const Color white(255, 255, 255, 255);

//! Every ignorable codepoint, UTF-8 encoded
std::vector<String> ignorables()
{
    std::vector<String> all;
    auto encode=[](uint32_t cp)
    {
        return String{ char(0xE0|(cp>>12)), char(0x80|((cp>>6)&0x3F)), char(0x80|(cp&0x3F)) };
    };

    for(uint32_t cp=0x200B; cp <= 0x200D; ++cp)
        all.push_back(encode(cp));
    all.push_back(encode(0x2060));
    for(uint32_t cp=0xFE00; cp <= 0xFE0F; ++cp)
        all.push_back(encode(cp));
    all.push_back(encode(0xFEFF));
    return all;
}

struct Mode
{
    const char *name;
    float size;
    TextAA aa;
    TextMode expected;
};

const Mode modes[]={
    { "coverage", 13.0f, TextAA::Grayscale, TextMode::Coverage },
    { "lcd", 13.0f, TextAA::LcdRGB, TextMode::LcdRGB },
    { "sdf", 26.0f, TextAA::Grayscale, TextMode::Sdf },
};

const char *const faces[]={ "OpenSans", "DejaVuSansMono" };

Font face(const String &name, float size)
{
    Font font=gweni::test::openSans();

    font.facename=name;
    font.size=size;
    return font;
}

//! A renderer of its own over the shared view, with the default chain
struct Fresh
{
    explicit Fresh(TextAA aa):
        paths("resources"),
        owned(gweni::test::newRenderer(paths, gweni::Rect(Point(0, 0), gweni::test::viewSize()))),
        renderer(*owned)
    {
        renderer.init();
        renderer.setTextAA(aa);
        renderer.setGlyphFallback(String(), { "DejaVuSansMono" });
    }

    bool runs(const Mode &mode)
    {
        if(renderer.getTextMode(face("OpenSans", mode.size)) == mode.expected)
            return true;
        MESSAGE("skipped ", mode.name, ": not drawn in that mode here");
        return false;
    }

    //! One frame of text in a new run, white on black, read back; slots gets the
    //! glyph slots it wrote
    std::vector<uint8_t> draw(const Font &font, const String &text, size_t &slots)
    {
        std::vector<uint8_t> pixels;
        size_t id=0;

        renderer.generateTextPrimitive(id);
        gweni::test::clearView(Color(0, 0, 0, 255));
        renderer.begin();
        renderer.setDrawColor(white);
        renderer.renderText(id, font, Point(20, 30), 1, text);
        renderer.end();
        slots=renderer.getFrameStats().textSlotsWritten;
        gweni::test::readView(pixels);

        renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
        return pixels;
    }

    gweni::platform::CurrentPath paths;
    std::unique_ptr<SlotRenderer> owned;
    SlotRenderer &renderer;
};

bool blank(const std::vector<uint8_t> &frame)
{
    for(size_t i=0; i < frame.size(); i+=4)
    {
        if(frame[i] != 0 || frame[i+1] != 0 || frame[i+2] != 0)
            return false;
    }
    return true;
}

//! measureText of each codepoint prefix, 0 for the empty one
std::vector<int> prefixWidths(SlotRenderer &renderer, const Font &font, const String &text)
{
    std::vector<int> xs={ 0 };

    for(size_t i=gweni::utf8Next(text, 0); i <= text.size(); i=gweni::utf8Next(text, i))
    {
        xs.push_back(renderer.measureText(font, text.substr(0, i)).x);
        if(i >= text.size())
            break;
    }
    return xs;
}

}//namespace

TEST_CASE("Ignorables: utf8_is_ignorable holds for each range and nothing beside it")
{
    for(const String &cp:ignorables())
        CHECK(ftgl::utf8_is_ignorable(cp.c_str()) == 1);

    // the neighbours of each range, and other 3-byte characters
    const char *const others[]={
        "\xE2\x80\x8A", "\xE2\x80\x8E", "\xE2\x81\x9F", "\xE2\x81\xA1",
        "\xEF\xB7\xBF", "\xEF\xB9\x80", "\xEF\xBB\xBE", "\xEF\xBC\x80",
        "\xE2\x86\x92", "\xE2\x9C\x93", "a", " ", "", "\xE2", "\xE2\x80", "\xEF\xBB"
    };

    for(const char *other:others)
    {
        CAPTURE(String(other));
        CHECK(ftgl::utf8_is_ignorable(other) == 0);
    }
    CHECK(ftgl::utf8_is_ignorable(nullptr) == 0);
}

TEST_CASE("Ignorables: draw nothing, take no advance and no glyph slot, in every mode and face")
{
    for(const Mode &mode:modes)
    {
        Fresh fresh(mode.aa);

        if(!fresh.runs(mode))
            continue;

        for(const char *name:faces)
        {
            const Font font=face(name, mode.size);

            REQUIRE(static_cast<int>(fresh.renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));

            size_t plainSlots=0;
            const std::vector<uint8_t> plain=fresh.draw(font, "ab", plainSlots);
            const int plainWidth=fresh.renderer.measureText(font, "ab").x;
            const int aWidth=fresh.renderer.measureText(font, "a").x;

            REQUIRE(plainSlots == 2);

            for(const String &cp:ignorables())
            {
                CAPTURE(mode.name);
                CAPTURE(name);
                CAPTURE(ftgl::utf8_to_utf32(cp.c_str()));

                const String text="a"+cp+"b";
                size_t slots=0;

                CHECK(fresh.draw(font, text, slots) == plain);
                CHECK(slots == plainSlots);
                CHECK(fresh.renderer.measureText(font, text).x == plainWidth);

                std::vector<int> carets;

                fresh.renderer.measureCarets(font, text, carets);
                CHECK(carets == std::vector<int>{ 0, aWidth, aWidth, plainWidth });
                CHECK(carets == prefixWidths(fresh.renderer, font, text));

                // alone: nothing at all
                size_t aloneSlots=1;

                CHECK(blank(fresh.draw(font, cp, aloneSlots)));
                CHECK(aloneSlots == 0);
                CHECK(fresh.renderer.measureText(font, cp).x == 0);
            }
            fresh.renderer.freeFont(font);
        }
        CHECK(fresh.renderer.glyphFallbackFilesRead() == 0);
    }
}

TEST_CASE("Ignorables: a warning sign with its emoji variation selector is the sign alone")
{
    const String sign="\xE2\x9A\xA0";
    const String selector="\xEF\xB8\x8F";

    for(const Mode &mode:modes)
    {
        CAPTURE(mode.name);
        Fresh fresh(mode.aa);

        if(!fresh.runs(mode))
            continue;

        const Font font=face("OpenSans", mode.size);
        size_t signSlots=0, withSlots=0;

        REQUIRE(static_cast<int>(fresh.renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));

        const std::vector<uint8_t> alone=fresh.draw(font, sign+" x", signSlots);

        CHECK(!blank(alone));
        CHECK(fresh.draw(font, sign+selector+" x", withSlots) == alone);
        CHECK(withSlots == signSlots);
        CHECK(fresh.renderer.measureText(font, sign+selector+" x").x == fresh.renderer.measureText(font, sign+" x").x);
        fresh.renderer.freeFont(font);
    }
}
