/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  renderer::Base::measureCarets on the shared renderer gives, for each caret
 *  position of a string (its utf8Next codepoint boundaries, 0 and the end
 *  included), exactly the x measureText gives the prefix ending there, 0 for
 *  the empty prefix: the definition, which the default is. The strings are the
 *  empty one, plain ASCII, kerning pairs, multi-byte UTF-8, glyphs OpenSans
 *  lacks, a '\n' mid-string and a stray continuation byte (whose boundaries
 *  are not freetype-gl's), each in grayscale coverage, in the SDF range, in
 *  LcdRGB (grayscale where the device cannot blend it) and at scale 1.5. A case
 *  that changes the text AA mode or the scale puts it back when it ends.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/utility.h>

#include <vector>

namespace
{

using gweni::Font;
using gweni::String;
using gweni::renderer::SlotRenderer;
using gweni::renderer::TextAA;
using gweni::renderer::TextMode;

const std::vector<String> &strings()
{
    static const std::vector<String> list={
        "",
        "x",
        "Hello, world",
        "AVAWAY To Ty. LT Yo",
        "na\xc3\xafve caf\xc3\xa9 \xe2\x80\x94 \xc3\xbc\xc3\x9f",  // naïve café — üß
        "a\xe4\xb8\xad\xe6\x96\x87" "b\xf0\x9f\x98\x80" "c",       // a中文b😀c, glyphs OpenSans lacks
        "two\nlines AV\nT",
        "a\x80" "b",                                               // a stray continuation byte
    };
    return list;
}

Font openSansAt(float size)
{
    Font font=gweni::test::openSans();

    font.size=size;
    return font;
}

const char *modeName(SlotRenderer &renderer, const Font &font)
{
    if(renderer.getTextMode(font) == TextMode::Sdf)
        return "sdf";
    return (renderer.textAA() == TextAA::Grayscale)?"gray":"lcd";
}

//! measureCarets against measureText of each codepoint prefix, for every string
void checkCarets(float size)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font font=openSansAt(size);

    for(const String &text:strings())
    {
        CAPTURE(text);
        CAPTURE(size);
        CAPTURE(renderer.scale());
        INFO("mode ", modeName(renderer, font));

        std::vector<int> expected={0};

        for(size_t i=gweni::utf8Next(text, 0); !text.empty(); i=gweni::utf8Next(text, i))
        {
            expected.push_back(renderer.measureText(font, text.substr(0, i)).x);

            if(i >= text.size())
                break;
        }

        std::vector<int> xs={-1, -1};  // whatever was in it is replaced

        renderer.measureCarets(font, text, xs);
        CHECK(xs == expected);
    }
}

//! Puts the shared renderer's text AA mode back when a case ends.
struct RestoreTextAA
{
    RestoreTextAA(): aa(gweni::test::gpuRenderer().textAA()) {}
    ~RestoreTextAA() { gweni::test::gpuRenderer().setTextAA(aa); }

    TextAA aa;
};

//! Puts the scale back to 1 when a case ends.
struct ScaleGuard
{
    ~ScaleGuard() { gweni::test::gpuRenderer().setScale(1.0f); }
};

}//namespace

TEST_CASE("measureCarets: each caret is measureText of its prefix, grayscale coverage")
{
    RestoreTextAA restore;
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.setTextAA(TextAA::Grayscale);

    for(float size:{11.f, 12.f, 15.f})
        checkCarets(size);
}

TEST_CASE("measureCarets: each caret is measureText of its prefix, in the SDF range")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    for(float size:{18.f, 24.f, 37.f})
    {
        if(renderer.getTextMode(openSansAt(size)) != TextMode::Sdf)
            MESSAGE("GWENI_SDF_TEXT_MIN puts ", size, " px below the SDF threshold");
        checkCarets(size);
    }
}

TEST_CASE("measureCarets: each caret is measureText of its prefix, LCD")
{
    RestoreTextAA restore;
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.setTextAA(TextAA::LcdRGB);
    if(!renderer.lcdTextAvailable())
        MESSAGE("no LCD text on this device (", renderer.lcdTextUnavailable(), "): grayscale");

    for(float size:{11.f, 12.f, 15.f})
        checkCarets(size);
}

TEST_CASE("measureCarets: each caret is measureText of its prefix, at scale 1.5")
{
    ScaleGuard guard;
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.setScale(1.5f);

    // 11 and 12 px are coverage and SDF at 1.5 (16.5 and 18 device px), 24 SDF
    for(float size:{11.f, 12.f, 24.f})
        checkCarets(size);
}
