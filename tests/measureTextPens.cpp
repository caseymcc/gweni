/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer::measureText of a coverage font (every size below the SDF
 *  threshold) is exactly the Point measureTextWithTextBuffer gives, the measure
 *  freetype-gl's text_buffer_add_text makes of the run renderText lays out: the
 *  last pen rounded up and one line box. The corpus is every printable ASCII
 *  character alone, the printable ASCII string, kerning words, the empty string,
 *  strings with a '\n' (the last line's pen and one line box, as freetype-gl
 *  measures them), multi-byte UTF-8 with codepoints OpenSans lacks, a stray
 *  continuation byte and a NUL inside the string (where utf8_strlen ends the
 *  run). It is checked at every whole size from 8 px to the threshold and at
 *  two fractional ones, regular and bold, in Grayscale, LcdRGB and LcdBGR (an
 *  LCD mode the device cannot blend is skipped), at scales 1, 1.25, 1.5 and 2:
 *  measureText measures at font.size, so the scale must change nothing. Each
 *  size is checked to be in the coverage range, so the SDF path is not what
 *  is compared. A second case measures a font again after it was freed (its
 *  glyphs, and FontData::glyph's table of them, go with it) and after a text AA
 *  switch and back, which replace it, the ASAN tree catching a glyph kept past
 *  its font. The cases put the text AA mode and the scale back when they end.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <vector>

namespace
{

using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::SlotRenderer;
using gweni::renderer::TextAA;
using gweni::renderer::TextMode;

//! Puts the shared renderer's text AA mode and scale back when a case ends.
struct Restore
{
    Restore(): aa(gweni::test::gpuRenderer().textAA()) {}

    ~Restore()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setTextAA(aa);
        renderer.setScale(1.0f);
    }

    TextAA aa;
};

std::vector<String> corpus()
{
    std::vector<String> list;
    String ascii;

    for(char c=' '; c <= '~'; ++c)
    {
        list.push_back(String(1, c));
        ascii+=c;
    }
    list.push_back(ascii);
    for(const char *word:{"AV", "To", "Wa", "LT", "Yo", "AVAWAY To Ty. LT Yo", "Hello, world"})
        list.push_back(word);
    list.push_back("");
    for(const char *lines:{"\n", "AV\n", "\nAV", "two\nlines AV\nT", "Wa\n\nTo"})
        list.push_back(lines);
    list.push_back("na\xc3\xafve caf\xc3\xa9 \xe2\x80\x94 \xc3\xbc\xc3\x9f");  // naïve café — üß
    list.push_back("a\xe4\xb8\xad\xe6\x96\x87" "b\xf0\x9f\x98\x80" "c");       // a中文b😀c, glyphs OpenSans lacks
    list.push_back("\xe4\xb8\xad" "AV");                                       // 中AV: kerned after a missing glyph
    list.push_back("a\x80" "b");                                               // a stray continuation byte
    list.push_back(String("AV\0To", 5));                                       // utf8_strlen ends the run at the NUL
    return list;
}

Font openSansAt(float size, bool bold)
{
    Font font=gweni::test::openSans();

    font.size=size;
    font.bold=bold;
    return font;
}

}//namespace

TEST_CASE("measureText: a coverage font's pen walk gives the text_buffer_t measure, in every size, weight, mode and scale")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    Restore restore;

    const std::vector<String> strings=corpus();
    const TextAA modes[]={TextAA::Grayscale, TextAA::LcdRGB, TextAA::LcdBGR};
    const float scales[]={1.0f, 1.25f, 1.5f, 2.0f};

    // every whole size from 8 px up to the threshold, and two between them
    std::vector<float> sizes;

    renderer.setScale(1.0f);
    for(float size=8.0f; size < 256.0f; size+=1.0f)
    {
        if(renderer.getTextMode(openSansAt(size, false)) == TextMode::Sdf)
            break;
        sizes.push_back(size);
    }
    for(float size:{10.5f, 13.25f})
    {
        if(renderer.getTextMode(openSansAt(size, false)) != TextMode::Sdf)
            sizes.push_back(size);
    }
    if(sizes.empty())
    {
        MESSAGE("skipped: GWENI_SDF_TEXT_MIN puts 8 px in the SDF range");
        return;
    }

    size_t compared=0;
    size_t mismatches=0;

    for(TextAA aa:modes)
    {
        renderer.setTextAA(aa);
        if(renderer.textAA() != aa)
        {
            MESSAGE("skipped: text AA mode ", (int)aa, " is not available (",
                renderer.lcdTextUnavailable(), ")");
            continue;
        }

        const TextMode coverageMode=(aa == TextAA::LcdRGB)?TextMode::LcdRGB:
            (aa == TextAA::LcdBGR)?TextMode::LcdBGR:TextMode::Coverage;

        for(bool bold:{false, true})
        {
            for(float size:sizes)
            {
                const Font font=openSansAt(size, bold);

                renderer.setScale(1.0f);
                CAPTURE((int)aa);
                CAPTURE(bold);
                CAPTURE(size);
                // measureText measures at font.size, the size of this mode at scale 1
                REQUIRE(renderer.getTextMode(font) == coverageMode);

                for(float scale:scales)
                {
                    renderer.setScale(scale);
                    for(const String &text:strings)
                    {
                        const Point walked=renderer.measureText(font, text);
                        const Point reference=renderer.measureTextWithTextBuffer(font, text);

                        ++compared;
                        if(walked.x == reference.x && walked.y == reference.y)
                            continue;
                        if(++mismatches <= 20)
                        {
                            CAPTURE(scale);
                            CAPTURE(text);
                            CAPTURE(walked.x);
                            CAPTURE(walked.y);
                            CAPTURE(reference.x);
                            CAPTURE(reference.y);
                            FAIL_CHECK("measureText differs from the text_buffer_t measure");
                        }
                    }
                }
            }
        }
    }

    MESSAGE(compared, " measures compared, ", mismatches, " differ");
    CHECK(compared > 0);
    CHECK(mismatches == 0);
}

TEST_CASE("measureText: the glyph table follows a font freed and loaded again, and a text AA switch")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    Restore restore;

    const std::vector<String> strings=corpus();
    const Font font=openSansAt(9.0f, true);
    const auto check=[&]()
    {
        for(const String &text:strings)
        {
            CAPTURE(text);
            const Point walked=renderer.measureText(font, text);
            const Point reference=renderer.measureTextWithTextBuffer(font, text);

            CHECK(walked.x == reference.x);
            CHECK(walked.y == reference.y);
        }
    };

    renderer.setTextAA(TextAA::Grayscale);
    renderer.setScale(1.0f);
    if(renderer.getTextMode(font) == TextMode::Sdf)
    {
        MESSAGE("skipped: GWENI_SDF_TEXT_MIN puts 9 px in the SDF range");
        return;
    }

    // a font loaded and freed: its FontData, glyphs and table go, a use loads a new one
    REQUIRE(renderer.loadFont(font) == Font::Status::Loaded);
    check();

    const size_t loaded=renderer.getLoadedFontCount();

    renderer.freeFont(font);
    REQUIRE(renderer.getLoadedFontCount() == loaded-1);
    check();

    // a switch of mode replaces every coverage font, and switching back again
    renderer.setTextAA(TextAA::LcdRGB);
    check();
    renderer.setTextAA(TextAA::Grayscale);
    check();
}
