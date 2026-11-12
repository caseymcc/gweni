/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A run in a fixed-width face (DejaVu Sans Mono) lays every codepoint in whole
 *  terminal cells, gweni::text::cellCount's: 2 for East Asian Wide/Fullwidth,
 *  by codepoint whichever face draws it (⚡ U+26A1 takes 2 though DejaVu Sans
 *  Mono draws it one cell wide), 0 for combining marks and ignorables, 1 for
 *  the rest. The cell is the face's advance for '0'; a glyph narrower or wider
 *  than its cells is centred in them. These cases pin cellCount's table at
 *  known codepoints and its Unicode version, and lay this table out in
 *  DejaVu Sans Mono at every size from 8 to 24 (coverage below the SDF
 *  threshold, SDF at and above it), at scale 1, 1.5 and 2:
 *
 *      ┌──┬──┐
 *      │ab│cd│
 *      │⚡│✓✗│
 *      └──┴──┘
 *
 *  (the task wrote the third row "│⚡x│✓✗│", which at ⚡'s two cells is a cell
 *  longer than the others, its middle rule a cell right of theirs; the wide
 *  glyph here stands where "ab" does, which is what the table is for):
 *  - each row's carets are the widths of as many '0's as cells before them,
 *    so every caret is a whole number of cells, and every row is 7 cells;
 *  - drawn as one run, three pixel columns, one per rule, are inked in every
 *    row from the middle of the top row to the middle of the bottom one (the
 *    rules join from row to row; the rows overlap where the glyphs reach past
 *    the line step, and the corners' and junctions' stems may sit a fraction of
 *    a pixel aside of │'s, as the hinter places them), and the two │ rows have
 *    the same pixels across each rule (their rules in the same device px);
 *  - the wide glyph is centred in its two cells, and a combining mark after a
 *    letter takes no cell and is drawn over it.
 *  On both backends: the renderer of its own is the case's backend's.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>
#include <gweni/text/cells.h>
#include <gweni/utility.h>

#include <freetype-gl/utf8-utils.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::SlotRenderer;
using gweni::renderer::TextMode;

const Color white(255, 255, 255, 255);

const String rows[4]={
    "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\xAC\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90", // ┌──┬──┐
    "\xE2\x94\x82" "ab" "\xE2\x94\x82" "cd" "\xE2\x94\x82",                                  // │ab│cd│
    "\xE2\x94\x82\xE2\x9A\xA1\xE2\x94\x82\xE2\x9C\x93\xE2\x9C\x97\xE2\x94\x82",              // │⚡│✓✗│
    "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\xB4\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98", // └──┴──┘
};

const float scales[]={ 1.0f, 1.5f, 2.0f };

Font mono(float size)
{
    Font font=gweni::test::openSans();

    font.facename="DejaVuSansMono";
    font.size=size;
    return font;
}

//! A renderer of its own over the shared view, with the skin's default chain
struct Fresh
{
    Fresh():
        paths("resources"),
        owned(gweni::test::newRenderer(paths, gweni::Rect(Point(0, 0), gweni::test::viewSize()))),
        renderer(*owned)
    {
        renderer.init();
        renderer.setGlyphFallback(String(), { "DejaVuSansMono" });
        renderer.generateTextPrimitive(id);
    }

    ~Fresh()
    {
        renderer.releaseTextPrimitive(id);
        renderer.setScale(1.0f);
        renderer.begin();
        renderer.end();
    }

    std::vector<uint8_t> draw(const Font &font, const String &text, Point pos)
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

//! The cells before each codepoint boundary of text, 0 first and the whole last
std::vector<int> cellsBefore(const String &text)
{
    std::vector<int> cells={ 0 };
    int n=0;

    for(size_t i=0; i < text.size(); i=gweni::utf8Next(text, i))
    {
        n+=gweni::text::cellCount(ftgl::utf8_to_utf32(text.c_str()+i));
        cells.push_back(n);
    }
    return cells;
}

uint8_t value(const std::vector<uint8_t> &frame, int x, int y)
{
    const Point size=gweni::test::viewSize();

    return frame[(static_cast<size_t>(y)*size.x+x)*4];
}

struct Ink
{
    int x0, y0, x1, y1;
};

Ink inkOf(const std::vector<uint8_t> &frame)
{
    const Point size=gweni::test::viewSize();
    Ink ink{ size.x, size.y, 0, 0 };

    for(int y=0; y < size.y; ++y)
    {
        for(int x=0; x < size.x; ++x)
        {
            if(value(frame, x, y) == 0)
                continue;
            ink.x0=std::min(ink.x0, x);
            ink.y0=std::min(ink.y0, y);
            ink.x1=std::max(ink.x1, x+1);
            ink.y1=std::max(ink.y1, y+1);
        }
    }
    return ink;
}

//! The runs of x inked in every row of [ya, yb]: [first, last] each
std::vector<std::pair<int, int>> continuousColumns(const std::vector<uint8_t> &frame, int ya, int yb)
{
    std::vector<std::pair<int, int>> runs;
    const Point size=gweni::test::viewSize();

    for(int x=0; x < size.x; ++x)
    {
        bool all=true;

        for(int y=ya; all && y <= yb; ++y)
            all=value(frame, x, y) != 0;
        if(!all)
            continue;
        if(!runs.empty() && runs.back().second == x-1)
            runs.back().second=x;
        else
            runs.emplace_back(x, x);
    }
    return runs;
}

//! The first of the rows in [y0, y1) with the most inked pixels: a horizontal
//! rule's
int widestRow(const std::vector<uint8_t> &frame, int y0, int y1)
{
    const Point size=gweni::test::viewSize();
    int best=y0, most=-1;

    for(int y=y0; y < y1; ++y)
    {
        int n=0;

        for(int x=0; x < size.x; ++x)
            n+=(value(frame, x, y) != 0)?1:0;
        if(n > most)
        {
            most=n;
            best=y;
        }
    }
    return best;
}

}//namespace

TEST_CASE("Cells: cellCount's table at known codepoints")
{
    using gweni::text::cellCount;

    CHECK(String(gweni::text::cellWidthUnicodeVersion()) == "18.0.0");
    for(uint32_t cp:{ 0x61u, 0x30u, 0x20u, 0x7Eu, 0x00E9u, 0x2500u, 0x2502u, 0x250Cu, 0x2713u, 0x2717u,
        0x26A0u, 0x2192u, 0x00A1u /* Ambiguous */, 0x2605u /* Ambiguous */, 0x0430u })
    {
        CAPTURE(cp);
        CHECK(cellCount(cp) == 1);
    }
    for(uint32_t cp:{ 0x26A1u, 0x2614u, 0x5355u, 0x3042u, 0xAC00u, 0xFF21u, 0x1F600u, 0x20000u,
        0x3400u, 0x9FFFu /* unassigned or not, the CJK block defaults to W */, 0x2FFFDu })
    {
        CAPTURE(cp);
        CHECK(cellCount(cp) == 2);
    }
    for(uint32_t cp:{ 0x0300u, 0x0301u, 0x036Fu, 0x20DDu /* Me */, 0x3099u /* Mn, also W */,
        0x200Bu, 0x200Cu, 0x200Du, 0x2060u, 0xFE00u, 0xFE0Fu, 0xFEFFu, 0xE0100u })
    {
        CAPTURE(cp);
        CHECK(cellCount(cp) == 0);
    }
}

TEST_CASE("Cells: a box-drawing table lines up in DejaVu Sans Mono, in coverage and SDF, at scale 1, 1.5 and 2")
{
    Fresh fresh;
    SlotRenderer &renderer=fresh.renderer;
    int coverageRuns=0, sdfRuns=0;

    for(float scale:scales)
    {
        renderer.setScale(scale);

        for(int size=8; size <= 24; ++size)
        {
            const Font font=mono(static_cast<float>(size));

            CAPTURE(scale);
            CAPTURE(size);
            REQUIRE(static_cast<int>(renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));

            const TextMode mode=renderer.getTextMode(font);

            (mode == TextMode::Sdf?sdfRuns:coverageRuns)+=1;
            INFO("mode ", (mode == TextMode::Sdf)?"sdf":"coverage");

            // the carets are whole cells, every row 7 of them
            for(const String &row:rows)
            {
                CAPTURE(row);
                const std::vector<int> cells=cellsBefore(row);
                std::vector<int> carets;

                renderer.measureCarets(font, row, carets);
                REQUIRE(carets.size() == cells.size());
                CHECK(cells.back() == 7);
                for(size_t k=0; k < cells.size(); ++k)
                {
                    CAPTURE(k);
                    CHECK(carets[k] == renderer.measureText(font, String(cells[k], '0')).x);
                }
            }

            // three rules down the drawn table, unbroken and alike in every row
            const String table=rows[0]+"\n"+rows[1]+"\n"+rows[2]+"\n"+rows[3];
            const std::vector<uint8_t> frame=fresh.draw(font, table, Point(10, 10));
            const Ink ink=inkOf(frame);
            const int height=ink.y1-ink.y0;
            const int ya=ink.y0+height*3/20;
            const int yb=ink.y1-1-height*3/20;
            const std::vector<std::pair<int, int>> columns=continuousColumns(frame, ya, yb);

            REQUIRE(height > 0);
            CHECK(columns.size() == 3);
            // the │ of the two middle rows, at the middle of each (a third and two
            // of the way from the top rule to the bottom one): the same pixels
            // across each rule (the corners' and junctions' own stems may sit a
            // fraction of a pixel aside, as the hinter places them)
            const int top=widestRow(frame, ink.y0, ink.y0+height/4);
            const int bottom=widestRow(frame, ink.y1-height/4, ink.y1);
            const int row1=top+static_cast<int>(std::lround((bottom-top)/3.0));
            const int row2=top+static_cast<int>(std::lround((bottom-top)*2/3.0));

            for(const auto &[first, last]:columns)
            {
                for(int x=first-1; x <= last+1; ++x)
                {
                    CAPTURE(x);
                    CHECK(value(frame, x, row1) == value(frame, x, row2));
                }
            }
            if(columns.size() == 3)
            {
                // three cells apart, as the rows' rules are, to within a pixel
                const float c0=(columns[0].first+columns[0].second)/2.0f;
                const float c1=(columns[1].first+columns[1].second)/2.0f;
                const float c2=(columns[2].first+columns[2].second)/2.0f;

                CHECK(std::abs((c1-c0)-(c2-c1)) <= 1.0f);
            }
            renderer.freeFont(font);
        }
    }
    CHECK(coverageRuns > 0);
    CHECK(sdfRuns > 0);
}

TEST_CASE("Cells: a wide glyph is centred in its two cells, a combining mark takes none")
{
    Fresh fresh;
    SlotRenderer &renderer=fresh.renderer;
    const Font font=mono(13.0f);
    const String bolt="\xE2\x9A\xA1";
    const String acute="\xCC\x81";

    REQUIRE(static_cast<int>(renderer.loadFont(font)) == static_cast<int>(Font::Status::Loaded));
    if(renderer.getTextMode(font) != TextMode::Coverage)
    {
        MESSAGE("skipped: GWENI_SDF_TEXT_MIN draws 13 px from the SDF atlas");
        return;
    }

    const int cell=renderer.measureText(font, "0").x;

    REQUIRE(cell > 0);
    CHECK(renderer.measureText(font, bolt).x == 2*cell);
    CHECK(renderer.measureText(font, "a"+acute).x == cell);
    CHECK(renderer.measureText(font, "a"+acute+"b").x == 2*cell);

    // ⚡ drawn at the start of two cells is the same image as one cell along
    const Ink wide=inkOf(fresh.draw(font, bolt, Point(20, 20)));
    const Ink narrow=inkOf(fresh.draw(font, "x\xE2\x9A\xA1", Point(20, 20)));

    CHECK(wide.x0 >= 20+cell/2-1);
    CHECK(wide.x1 <= 20+cell*3/2+1);
    CHECK(narrow.x1-(wide.x1+cell) == 0);

    // the acute is over the a: the ink of "a" + acute starts where "a"'s does and
    // ends within its cell
    const Ink letter=inkOf(fresh.draw(font, "a", Point(20, 20)));
    const Ink marked=inkOf(fresh.draw(font, "a"+acute, Point(20, 20)));

    CHECK(marked.x1 <= 20+cell+1);
    CHECK(marked.y0 < letter.y0);
    renderer.freeFont(font);
}
