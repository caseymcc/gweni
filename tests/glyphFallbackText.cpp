/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's glyph fallback chain (setGlyphFallback): a codepoint a run's
 *  face lacks is drawn from the first chain face that has it, in the same run,
 *  atlas and draw. These cases pin, in grayscale coverage, the SDF range and
 *  LcdRGB (where the device can blend it):
 *  - Latin-only text with a chain reads no chain face's file, opens no chain
 *    font, and draws and writes exactly what a renderer without one does;
 *  - the arrow OpenSans lacks is DejaVu Sans Mono's own image (its ink, moved
 *    to the run's baseline, byte for byte), with one font loaded and one text
 *    draw, as many as for Latin text, and the glyph slots of one run;
 *  - the chain's order, and the face's own missing glyph when no chain face has
 *    the codepoint (its pixels as with no chain);
 *  - measureText is the text_buffer_t measure of the drawn run (coverage), a
 *    fallback glyph adds its own face's advance, the ink stays inside the
 *    measure, and measureCarets is measureText of each prefix;
 *  - one read of a face's file serves every font of it, kept while a font holds
 *    it and read again once none does;
 *  - a chain set after a font is loaded loads that font again, so its runs draw
 *    the chain's glyph once laid out again, as a renderer with the chain from
 *    the start draws them.
 *  Each case uses a renderer of its own over the shared view, so the shared one
 *  never gets a chain.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>
#include <gweni/utility.h>

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

// U+2192 RIGHTWARDS ARROW: in Liberation Sans and DejaVu Sans Mono, not OpenSans.
// U+2713 CHECK MARK: in DejaVu Sans Mono alone. U+5355: in none of them.
const String arrow="\xE2\x86\x92";
const String check="\xE2\x9C\x93";
const String cjk="\xE5\x8D\x95";

const String latin=" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ AVATAR To Yo";

Font face(const String &name, float size)
{
    Font font=gweni::test::openSans();

    font.facename=name;
    font.size=size;
    return font;
}

//! The cases' modes: a size in the coverage range (grayscale, then LCD) and one in
//! the SDF range, each skipped where GWENI_SDF_TEXT_MIN or the device rules it out.
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

//! A renderer of its own over the shared view, in one mode, with or without the
//! default chain [DejaVuSansMono].
struct Fresh
{
    Fresh(TextAA aa, std::vector<String> chain):
        paths("resources"),
        owned(gweni::test::newRenderer(paths, gweni::Rect(Point(0, 0), gweni::test::viewSize()))),
        renderer(*owned)
    {
        renderer.init();
        renderer.setTextAA(aa);
        if(!chain.empty())
            renderer.setGlyphFallback(String(), chain);
        renderer.generateTextPrimitive(id);
    }

    ~Fresh()
    {
        renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
    }

    //! Whether the mode is the one asked for here (else the case skips it)
    bool runs(const Mode &mode)
    {
        if(renderer.getTextMode(face("OpenSans", mode.size)) == mode.expected)
            return true;
        MESSAGE("skipped ", mode.name, ": not drawn in that mode here");
        return false;
    }

    //! One frame of text in white on black at pos, read back
    std::vector<uint8_t> draw(const Font &font, const String &text, Point pos=Point(20, 30))
    {
        std::vector<uint8_t> pixels;

        gweni::test::clearView(Color(0, 0, 0, 255));
        renderer.begin();
        renderer.setDrawColor(white);
        renderer.renderText(id, font, pos, 1, text);
        renderer.end();
        gweni::test::readView(pixels);
        return pixels;
    }

    gweni::platform::CurrentPath paths;
    std::unique_ptr<SlotRenderer> owned;
    SlotRenderer &renderer;
    size_t id=0;
};

bool inked(const std::vector<uint8_t> &frame, size_t i)
{
    return (frame[i] != 0) || (frame[i+1] != 0) || (frame[i+2] != 0);
}

//! The box [x0, x1) x [y0, y1) of the pixels that are not black
struct Ink
{
    int x0=0, y0=0, x1=0, y1=0;

    bool empty() const { return x1 <= x0; }
};

Ink inkOf(const std::vector<uint8_t> &frame)
{
    const Point size=gweni::test::viewSize();
    Ink ink{ size.x, size.y, 0, 0 };

    for(int y=0; y < size.y; ++y)
    {
        for(int x=0; x < size.x; ++x)
        {
            if(!inked(frame, (static_cast<size_t>(y)*size.x+x)*4))
                continue;
            ink.x0=std::min(ink.x0, x);
            ink.y0=std::min(ink.y0, y);
            ink.x1=std::max(ink.x1, x+1);
            ink.y1=std::max(ink.y1, y+1);
        }
    }
    return ink;
}

//! The ink's box of pixels, row by row
std::vector<uint8_t> inkPixels(const std::vector<uint8_t> &frame, const Ink &ink)
{
    const Point size=gweni::test::viewSize();
    std::vector<uint8_t> crop;

    for(int y=ink.y0; y < ink.y1; ++y)
    {
        const size_t row=(static_cast<size_t>(y)*size.x+ink.x0)*4;

        crop.insert(crop.end(), frame.begin()+row, frame.begin()+row+(ink.x1-ink.x0)*4);
    }
    return crop;
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

TEST_CASE("Glyph fallback: Latin-only text reads and opens no chain face, and draws as with no chain")
{
    for(const Mode &mode:modes)
    {
        CAPTURE(mode.name);
        Fresh plain(mode.aa, {});
        Fresh chained(mode.aa, { "DejaVuSansMono" });

        if(!chained.runs(mode))
            continue;

        const Font font=face("OpenSans", mode.size);

        REQUIRE(static_cast<int>(chained.renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));
        REQUIRE(static_cast<int>(plain.renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));

        const std::vector<uint8_t> expected=plain.draw(font, latin);
        const auto plainStats=plain.renderer.getFrameStats();
        const std::vector<uint8_t> drawn=chained.draw(font, latin);
        const auto chainedStats=chained.renderer.getFrameStats();

        CHECK(drawn == expected);
        CHECK(chainedStats.textSlotsWritten == plainStats.textSlotsWritten);
        CHECK(chainedStats.atlasUploadBytes == plainStats.atlasUploadBytes);
        CHECK(chainedStats.textDraws == plainStats.textDraws);
        CHECK(chained.renderer.measureText(font, latin).x == plain.renderer.measureText(font, latin).x);

        std::vector<int> carets, plainCarets;

        chained.renderer.measureCarets(font, latin, carets);
        plain.renderer.measureCarets(font, latin, plainCarets);
        CHECK(carets == plainCarets);

        CHECK(chained.renderer.glyphFallbackFilesRead() == 0);
        CHECK(chained.renderer.glyphFallbackFontsOpened() == 0);

        chained.renderer.freeFont(font);
        plain.renderer.freeFont(font);
    }
}

TEST_CASE("Glyph fallback: a codepoint the face lacks is the chain face's own glyph, in one run, atlas and draw")
{
    for(const Mode &mode:modes)
    {
        CAPTURE(mode.name);
        Fresh chained(mode.aa, { "DejaVuSansMono" });
        Fresh mono(mode.aa, {});

        if(!chained.runs(mode))
            continue;

        const Font font=face("OpenSans", mode.size);
        const Font monoFont=face("DejaVuSansMono", mode.size);

        REQUIRE(static_cast<int>(chained.renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));
        REQUIRE(static_cast<int>(mono.renderer.loadFont(monoFont)) == static_cast<int>(Font::Status::Loaded));

        chained.draw(font, "ab");
        const auto latinStats=chained.renderer.getFrameStats();
        const size_t fonts=chained.renderer.getLoadedFontCount();

        // the arrow alone: DejaVu Sans Mono's image, on the run's baseline
        const std::vector<uint8_t> fromChain=chained.draw(font, arrow);
        const auto arrowStats=chained.renderer.getFrameStats();
        const std::vector<uint8_t> fromMono=mono.draw(monoFont, arrow);
        const Ink chainInk=inkOf(fromChain);
        const Ink monoInk=inkOf(fromMono);

        REQUIRE(!chainInk.empty());
        CHECK(chainInk.x0 == monoInk.x0);
        CHECK(chainInk.x1-chainInk.x0 == monoInk.x1-monoInk.x0);
        CHECK(chainInk.y1-chainInk.y0 == monoInk.y1-monoInk.y0);
        CHECK(inkPixels(fromChain, chainInk) == inkPixels(fromMono, monoInk));

        CHECK(chained.renderer.glyphFallbackFilesRead() == 1);
        CHECK(chained.renderer.glyphFallbackFontsOpened() == 1);
        // no atlas or font of its own, one draw as for Latin text, one run
        CHECK(chained.renderer.getLoadedFontCount() == fonts);
        CHECK(arrowStats.textDraws == latinStats.textDraws);

        chained.draw(font, "a"+arrow+"b"+check);
        CHECK(chained.renderer.getFrameStats().textDraws == latinStats.textDraws);
        CHECK(chained.renderer.getLoadedFontCount() == fonts);
        CHECK(chained.renderer.glyphFallbackFontsOpened() == 1);

        chained.renderer.freeFont(font);
        mono.renderer.freeFont(monoFont);
    }
}

TEST_CASE("Glyph fallback: the first chain face with the codepoint draws it; with none, the face's own missing glyph")
{
    Fresh chained(TextAA::Grayscale, { "LiberationSans-Regular", "DejaVuSansMono" });
    Fresh plain(TextAA::Grayscale, {});
    Fresh liberation(TextAA::Grayscale, {});

    if(!chained.runs(modes[0]))
        return;

    const Font font=face("OpenSans", 13.0f);
    const Font liberationFont=face("LiberationSans-Regular", 13.0f);

    REQUIRE(static_cast<int>(chained.renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));
    REQUIRE(static_cast<int>(plain.renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));
    REQUIRE(static_cast<int>(liberation.renderer.loadFont(liberationFont)) == static_cast<int>(Font::Status::Loaded));

    const std::vector<uint8_t> fromChain=chained.draw(font, arrow);
    const std::vector<uint8_t> fromLiberation=liberation.draw(liberationFont, arrow);

    CHECK(inkPixels(fromChain, inkOf(fromChain)) == inkPixels(fromLiberation, inkOf(fromLiberation)));
    // Liberation has it: DejaVu Sans Mono is not opened
    CHECK(chained.renderer.glyphFallbackFontsOpened() == 1);

    chained.draw(font, check);
    CHECK(chained.renderer.glyphFallbackFontsOpened() == 2);
    CHECK(chained.renderer.glyphFallbackFilesRead() == 2);

    // in none of them: OpenSans's own missing glyph, as with no chain
    CHECK(chained.draw(font, cjk) == plain.draw(font, cjk));
    CHECK(chained.renderer.measureText(font, cjk).x == plain.renderer.measureText(font, cjk).x);

    chained.renderer.freeFont(font);
    plain.renderer.freeFont(font);
    liberation.renderer.freeFont(liberationFont);
}

TEST_CASE("Glyph fallback: measureText and measureCarets measure the drawn run, fallback glyphs at their own face's advance")
{
    const String mixed="A"+arrow+"V"+check+"x "+arrow+arrow+"To";

    for(const Mode &mode:modes)
    {
        CAPTURE(mode.name);
        Fresh chained(mode.aa, { "DejaVuSansMono" });

        if(!chained.runs(mode))
            continue;

        SlotRenderer &renderer=chained.renderer;
        const Font font=face("OpenSans", mode.size);
        const Font monoFont=face("DejaVuSansMono", mode.size);

        REQUIRE(static_cast<int>(renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));
        REQUIRE(static_cast<int>(renderer.loadFont(monoFont)) == static_cast<int>(Font::Status::Loaded));

        const Point measured=renderer.measureText(font, mixed);

        if(mode.expected != TextMode::Sdf)
        {
            // the run as text_buffer_add_text lays it out to be drawn
            const Point reference=renderer.measureTextWithTextBuffer(font, mixed);

            CHECK(measured.x == reference.x);
            CHECK(measured.y == reference.y);

            // hinted, whole-pixel advances: the arrow adds exactly its own face's
            if(mode.expected == TextMode::Coverage)
            {
                CHECK(renderer.measureText(font, "a"+arrow).x-renderer.measureText(font, "a").x
                    == renderer.measureText(monoFont, arrow).x);
            }
        }
        else
        {
            const int step=renderer.measureText(font, "a"+arrow).x-renderer.measureText(font, "a").x;

            CHECK(std::abs(step-renderer.measureText(monoFont, arrow).x) <= 1);
        }

        std::vector<int> carets;

        renderer.measureCarets(font, mixed, carets);
        CHECK(carets == prefixWidths(renderer, font, mixed));
        CHECK(carets.back() == measured.x);

        // the ink is inside the measure
        const Point pos(20, 30);
        const Ink ink=inkOf(chained.draw(font, mixed, pos));

        CHECK(ink.x0 >= pos.x-1);
        CHECK(ink.x1 <= pos.x+measured.x+1);
        CHECK(ink.x1 > pos.x+measured.x-renderer.measureText(font, "o").x);

        renderer.freeFont(font);
        renderer.freeFont(monoFont);
    }
}

TEST_CASE("Glyph fallback: a face's file is read once for every font of it, while one holds it")
{
    Fresh chained(TextAA::Grayscale, { "DejaVuSansMono" });

    if(!chained.runs(modes[0]))
        return;

    SlotRenderer &renderer=chained.renderer;
    const Font small=face("OpenSans", 12.0f);
    const Font large=face("OpenSans", 14.0f);

    REQUIRE(static_cast<int>(renderer.loadFont(small)) == static_cast<int>(Font::Status::Loaded));
    REQUIRE(static_cast<int>(renderer.loadFont(large)) == static_cast<int>(Font::Status::Loaded));
    renderer.measureText(small, arrow);
    renderer.measureText(large, arrow);
    CHECK(renderer.glyphFallbackFontsOpened() == 2);
    CHECK(renderer.glyphFallbackFilesRead() == 1);

    // with both fonts gone, nothing holds the file: the next miss reads it again
    renderer.freeFont(small);
    renderer.freeFont(large);
    CHECK(renderer.getTextureFont(small) == nullptr);
    CHECK(renderer.getTextureFont(large) == nullptr);

    REQUIRE(static_cast<int>(renderer.loadFont(small)) == static_cast<int>(Font::Status::Loaded));
    renderer.measureText(small, arrow);
    CHECK(renderer.glyphFallbackFontsOpened() == 3);
    CHECK(renderer.glyphFallbackFilesRead() == 2);
    renderer.freeFont(small);
}

TEST_CASE("Glyph fallback: a chain set after a font is loaded loads it again")
{
    Fresh late(TextAA::Grayscale, {});
    Fresh early(TextAA::Grayscale, { "DejaVuSansMono" });

    if(!late.runs(modes[0]))
        return;

    const Font font=face("OpenSans", 13.0f);
    const String text="a"+arrow+"b";

    REQUIRE(static_cast<int>(late.renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));
    REQUIRE(static_cast<int>(early.renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));

    const std::vector<uint8_t> box=late.draw(font, text);
    const unsigned serial=late.renderer.textLayoutSerial();

    late.renderer.setGlyphFallback(String(), { "DejaVuSansMono" });
    CHECK(late.renderer.textLayoutSerial() != serial);
    CHECK(late.renderer.getTextureFont(font) != nullptr);
    CHECK(late.renderer.glyphFallback("OpenSans") == std::vector<String>{ "DejaVuSansMono" });
    // a face is never its own fallback
    CHECK(late.renderer.glyphFallback("DejaVuSansMono").empty());

    const std::vector<uint8_t> drawn=late.draw(font, text);

    CHECK(drawn != box);
    CHECK(drawn == early.draw(font, text));

    late.renderer.freeFont(font);
    early.renderer.freeFont(font);
}
