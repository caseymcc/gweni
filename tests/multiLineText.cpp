/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  An unwrapped Label holding a string with '\n's ("ab\nlonger line\nc") is
 *  measured as it is drawn: renderText draws every line, so measureText gives
 *  the widest line and every line's height, in a coverage font (11 px) as in
 *  an SDF one (20 px), at scales 1 and 1.5. The run is drawn again by itself,
 *  unclipped, at the Text's place, and every inked pixel of it must be inside
 *  the Text's box (in device px, snapped as the renderer snaps it), the
 *  Label's own frame must show that same ink (nothing of it clipped away by
 *  the Label), and the box must leave less than one line step of empty rows
 *  below the last line's ink. The line step is measureText("x\nx").y less
 *  lineHeight(), which must be more than 0.
 *
 *  At a scale, coverage text is drawn in the font at size*scale, whose line
 *  step (ascender less the descender truncated to whole px) is not the
 *  measured one scaled: at 9 and 10 px times 1.25 and 10 px times 1.5 it is
 *  half a pixel to a pixel longer per line, so six lines of "gH" would end
 *  below the box unless renderText moves each line to the measured one's
 *  place (SlotRenderer::measuredPenShifts' lineShifts). The second case holds
 *  them to the same checks, in grayscale and, where the device blends it, LCD
 *  text, whose filter fringe a pixel left of the first glyph leaves the box
 *  with any number of lines, so there only the rows are checked.
 *
 *  A size measured as SDF (18 px and up) has a line step rounded to a whole
 *  pixel at each px, so drawn at size*scale its lines are off the measured
 *  step scaled too, whether drawn as SDF or, below the threshold, as
 *  coverage. The third case draws 20 lines of "H" at 20 px times 1.5 (41 px a
 *  line against 40.5), at the sizes times 2 whose drawn step is a pixel longer
 *  and a pixel shorter than the measured one doubled (found from measureText,
 *  not named), at 20 px times 4 and at 20 px times 0.75 (drawn as 15 px
 *  coverage, 20 px a line against 20.25). The view is too short for the run,
 *  so it is drawn a page at a time, moved by whole device px, and stitched.
 *  Every line's ink must be inside the measured box, with less than a line
 *  step of empty rows below the last, and each line's ink top within a device
 *  px of line 0's plus L times the measured step scaled; without the line
 *  shifts line 19 is 4.75 to 19 px off.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::String;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 0, 255);
const Color white(255, 255, 255, 255);

//! The box [x0, x1) x [y0, y1) of the pixels that are not the background.
struct Ink
{
    int x0=0, y0=0, x1=0, y1=0;
    size_t pixels=0;
};

Ink inkOf(const std::vector<uint8_t> &frame)
{
    const Point size=gweni::test::viewSize();
    Ink ink;

    ink.x0=size.x;
    ink.y0=size.y;
    for(int y=0; y < size.y; ++y)
    {
        for(int x=0; x < size.x; ++x)
        {
            const uint8_t *pixel=&frame[(y*size.x+x)*4];

            if((pixel[0] == background.r) && (pixel[1] == background.g) && (pixel[2] == background.b))
                continue;
            ++ink.pixels;
            ink.x0=std::min(ink.x0, x);
            ink.y0=std::min(ink.y0, y);
            ink.x1=std::max(ink.x1, x+1);
            ink.y1=std::max(ink.y1, y+1);
        }
    }
    return ink;
}

//! A textured-skin canvas over the whole view at \p scale holding one
//! unwrapped Label in \p font.
struct Scene
{
    Scene(float scale, const Font &font, const String &text)
    {
        const Point viewSize=gweni::test::viewSize();

        skin=std::make_unique<gweni::skin::textured::TexturedSkin>(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=std::make_unique<gweni::controls::Canvas>(skin.get());
        canvas->setScale(scale);
        canvas->setSize(static_cast<int>(std::lround(viewSize.x/scale)),
            static_cast<int>(std::lround(viewSize.y/scale)));

        label=canvas->newChild<gweni::controls::Label>();
        label->setPos(6, 3);
        label->setFont(font);
        label->setTextColor(white);
        label->setText(text);
    }

    ~Scene()
    {
        canvas.reset();
        skin.reset();

        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.begin();
        renderer.end();
    }

    std::vector<uint8_t> frame()
    {
        std::vector<uint8_t> pixels;

        gweni::test::clearView(background);
        canvas->updateCanvas();
        canvas->renderCanvas();
        gweni::test::readView(pixels);
        return pixels;
    }

    //! The Label's Text box in canvas units.
    gweni::Rect textBox()
    {
        const Point at=label->localPosToCanvas(Point(label->textX(), label->textY()));

        return gweni::Rect(at.x, at.y, label->textWidth(), label->textHeight());
    }

    std::unique_ptr<gweni::skin::textured::TexturedSkin> skin;
    std::unique_ptr<gweni::controls::Canvas> canvas;
    gweni::controls::Label *label=nullptr;
};

//! \p s drawn by itself on a new text id at \p at, unclipped, at the renderer's
//! current scale.
std::vector<uint8_t> runAlone(const Font &font, const Point &at, const String &s)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    size_t id=0;
    std::vector<uint8_t> pixels;

    renderer.generateTextPrimitive(id);
    gweni::test::clearView(background);
    renderer.begin();
    renderer.setDrawColor(white);
    renderer.renderText(id, font, at, 1, s);
    renderer.end();
    gweni::test::readView(pixels);

    renderer.releaseTextPrimitive(id);
    renderer.begin();
    renderer.end();
    return pixels;
}

//! Puts the shared renderer's text AA mode back when a case ends.
struct RestoreTextAA
{
    RestoreTextAA(): aa(gweni::test::gpuRenderer().textAA()) {}
    ~RestoreTextAA() { gweni::test::gpuRenderer().setTextAA(aa); }

    gweni::renderer::TextAA aa;
};

Font openSansAt(float size)
{
    Font font=gweni::test::openSans();

    font.size=size;
    return font;
}

//! An unwrapped Label holding \p text in \p font at \p scale: every inked pixel of
//! its run inside its Text's box, less than a line step of empty rows below the
//! last line's ink, and all of it shown. Without \p columns only the rows are
//! checked.
void checkLabel(float size, float scale, const String &text, bool columns=true)
{
    CAPTURE(size);
    CAPTURE(scale);
    CAPTURE(text);

    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font font=openSansAt(size);
    Scene scene(scale, font, text);

    scene.frame();
    scene.frame();

    const Ink label=inkOf(scene.frame());
    const gweni::Rect box=scene.textBox();

    // the line step in canvas units, as measureText gives it
    const int step=renderer.measureText(font, "x\nx").y-renderer.lineHeight(font);

    CHECK(step > 0);

    // the run alone, unclipped, where the Label draws it, the Label hidden
    scene.label->hide();
    scene.frame();
    renderer.setScale(scale);

    const Ink run=inkOf(runAlone(font, Point(box.x, box.y), text));
    const int left=SlotRenderer::snap(float(box.x), scale);
    const int top=SlotRenderer::snap(float(box.y), scale);
    const int right=SlotRenderer::snap(float(box.x+box.w), scale);
    const int bottom=SlotRenderer::snap(float(box.y+box.h), scale);

    REQUIRE(run.pixels > 50);

    // every line's ink inside the box
    CHECK(run.y0 >= top);
    CHECK(run.y1 <= bottom);
    if(columns)
    {
        CHECK(run.x0 >= left);
        CHECK(run.x1 <= right);
    }

    // and less than a line step of empty rows below the last line
    CHECK(bottom-run.y1 < step*scale);

    // the Label shows all of it: nothing is clipped away
    CHECK(label.y0 == run.y0);
    CHECK(label.y1 == run.y1);
    if(columns)
    {
        CHECK(label.x0 == run.x0);
        CHECK(label.x1 == run.x1);
        CHECK(label.pixels == run.pixels);
    }
}

//! Every row of a run drawn by itself, from the top of its box (row 0, its
//! snapped origin) down: whether it holds ink, and the columns it holds ink in.
struct RunRows
{
    int first=0; //!< the box-relative row rows[0] is
    std::vector<int> x0, x1; //!< x0 > x1 where a row has no ink

    bool inked(size_t row) const { return x0[row] < x1[row]; }
};

//! \p s drawn by itself at canvas x \p x, clipped to the view alone, at \p scale, for every row
//! from \p margin device px above its box's top to \p margin below \p bottom
//! (box-relative device px). The view is shorter than a long run, so the run is
//! drawn once per page, moved up a page at a time by whole device px each: a
//! moved run is a fresh layout, so the pages stitch into the one tall run.
RunRows runRows(const Font &font, int x, float scale, const String &s, int bottom, int margin)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Point viewSize=gweni::test::viewSize();
    // canvas units that are whole device px at every scale the cases use
    const int unit=4;
    const int pageDevice=viewSize.y-24;
    const int page=unit*std::max(1, static_cast<int>(std::floor(pageDevice/(unit*scale))));
    RunRows rows;

    rows.first=-margin;
    rows.x0.assign(bottom+2*margin, viewSize.x);
    rows.x1.assign(bottom+2*margin, 0);

    const gweni::Rect clip=renderer.clipRegion();

    // the whole view, whatever the last canvas or case left
    renderer.setScale(scale);
    renderer.setClipRegion(gweni::Rect(0, 0, static_cast<int>(std::ceil(viewSize.x/scale)),
        static_cast<int>(std::ceil(viewSize.y/scale))));
    for(int y=unit*static_cast<int>(std::ceil(margin/(unit*scale))); ; y-=page)
    {
        const int origin=SlotRenderer::snap(float(y), scale);
        const std::vector<uint8_t> frame=runAlone(font, Point(x, y), s);

        for(int row=0; row < viewSize.y; ++row)
        {
            const int at=row-origin-rows.first;

            if((at < 0) || (at >= static_cast<int>(rows.x0.size())))
                continue;
            for(int column=0; column < viewSize.x; ++column)
            {
                const uint8_t *pixel=&frame[(row*viewSize.x+column)*4];

                if((pixel[0] == background.r) && (pixel[1] == background.g) && (pixel[2] == background.b))
                    continue;
                rows.x0[at]=std::min(rows.x0[at], column);
                rows.x1[at]=std::max(rows.x1[at], column+1);
            }
        }
        if(viewSize.y-origin >= bottom+margin)
            break;
    }
    renderer.setClipRegion(clip);
    renderer.setScale(1.0f);
    renderer.begin();
    renderer.end();
    return rows;
}

//! The line step of \p font in canvas units, as measureText gives it.
int measuredLineStep(const Font &font)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    return renderer.measureText(font, "x\nx").y-renderer.lineHeight(font);
}

//! \p lines lines of "H" in \p size px, measured as SDF, drawn at \p scale: every
//! line's ink inside the run's measured box, less than a line step of empty rows
//! below the last, and each line's ink top within a device px of line 0's plus
//! L times the measured step scaled.
void checkSdfLines(float size, float scale, int lines)
{
    CAPTURE(size);
    CAPTURE(scale);
    CAPTURE(lines);

    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Font font=openSansAt(size);
    String text;

    for(int i=0; i < lines; ++i)
        text+=(i == 0)?"H":"\nH";

    const int step=measuredLineStep(font);
    const Point measured=renderer.measureText(font, text);
    const int x=6;

    REQUIRE(step > 0);
    REQUIRE(measured.y == renderer.lineHeight(font)+(lines-1)*step);

    // the box in device px, relative to its snapped top (the run's origin is a
    // whole device px at every page of runRows)
    const int left=SlotRenderer::snap(float(x), scale);
    const int right=SlotRenderer::snap(float(x+measured.x), scale);
    const int bottom=SlotRenderer::snap(float(measured.y), scale);
    const int margin=static_cast<int>(std::ceil(4*step*scale));
    const RunRows rows=runRows(font, x, scale, text, bottom, margin);

    // the ink's bands of rows, one per line
    std::vector<int> tops, ends;
    int inkLeft=std::numeric_limits<int>::max(), inkRight=0;

    for(size_t row=0; row < rows.x0.size(); ++row)
    {
        if(!rows.inked(row))
            continue;
        inkLeft=std::min(inkLeft, rows.x0[row]);
        inkRight=std::max(inkRight, rows.x1[row]);
        if((row == 0) || !rows.inked(row-1))
            tops.push_back(static_cast<int>(row)+rows.first);
        if((row+1 == rows.x0.size()) || !rows.inked(row+1))
            ends.push_back(static_cast<int>(row)+1+rows.first);
    }

    REQUIRE(tops.size() == size_t(lines));

    // every line's ink inside the box
    CHECK(tops.front() >= 0);
    CHECK(ends.back() <= bottom);
    CHECK(inkLeft >= left);
    CHECK(inkRight <= right);

    // and less than a line step of empty rows below the last line
    CHECK(bottom-ends.back() < step*scale);

    // each line where it is measured, from line 0: the line furthest off
    int worstLine=0;
    float worst=0.0f;

    for(int line=1; line < lines; ++line)
    {
        const float error=std::fabs(tops[line]-tops.front()-line*step*scale);

        if(error > worst)
        {
            worst=error;
            worstLine=line;
        }
    }
    CAPTURE(worstLine);
    CAPTURE(tops[worstLine]-tops.front());
    CAPTURE(worstLine*step*scale);
    CHECK(worst <= 1.0f);
}

}//namespace

TEST_CASE("Multi-line text: an unwrapped Label's box holds every line it draws")
{
    // coverage at both scales (16.5 px is below the threshold), and SDF
    for(float size:{ 11.0f, 20.0f })
    {
        for(float scale:{ 1.0f, 1.5f })
            checkLabel(size, scale, "ab\nlonger line\nc");
    }
}

TEST_CASE("Multi-line text: at a scale, each coverage line is drawn where it is measured")
{
    // Sizes where the font drawn at size*scale steps lines further apart than the
    // measured step times the scale (its descender is truncated to whole px), by
    // half a pixel or a whole one per line: over six lines the last would be
    // drawn below the box.
    const String lines="gH\ngH\ngH\ngH\ngH\ngH";

    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;

    // in grayscale, and in LCD text where the device can blend it: its filter's
    // fringe reaches a pixel left of the first glyph's, out of the box whether the
    // run has one line or six, so only its rows are checked
    for(gweni::renderer::TextAA aa:{ gweni::renderer::TextAA::Grayscale, gweni::renderer::TextAA::LcdRGB })
    {
        renderer.setTextAA(aa);
        if(renderer.textAA() != aa)
            continue;

        const bool columns=(aa == gweni::renderer::TextAA::Grayscale);

        CAPTURE(static_cast<int>(aa));
        checkLabel(9.0f, 1.25f, lines, columns);
        checkLabel(10.0f, 1.25f, lines, columns);
        checkLabel(10.0f, 1.5f, lines, columns);
    }
}

TEST_CASE("Multi-line text: at a scale, each line of an SDF-measured run is drawn where it is measured")
{
    // An SDF font's line step is round((ascender-descender)*px/48), which is not
    // linear in px: drawn at size*scale a line is a fraction of a pixel or more
    // off the measured step scaled, and over 20 lines the error adds up.
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;

    renderer.setTextAA(gweni::renderer::TextAA::Grayscale);

    // the sizes at scale 2 whose drawn step (the step measured at twice the size,
    // SDF too) is a whole pixel longer, and a whole pixel shorter, than the
    // measured one doubled
    float longer=0.0f, shorter=0.0f;

    for(float size=18.0f; size <= 40.0f; size+=1.0f)
    {
        const int error=measuredLineStep(openSansAt(2*size))-2*measuredLineStep(openSansAt(size));

        if((error == 1) && (longer == 0.0f))
            longer=size;
        if((error == -1) && (shorter == 0.0f))
            shorter=size;
    }
    REQUIRE(longer > 0.0f);
    REQUIRE(shorter > 0.0f);

    // drawn as SDF: half a pixel a line at 1.5, a pixel a line either way at 2,
    // and at 4
    checkSdfLines(20.0f, 1.5f, 20);
    checkSdfLines(longer, 2.0f, 20);
    checkSdfLines(shorter, 2.0f, 20);
    checkSdfLines(20.0f, 4.0f, 20);
    // drawn as coverage, 15 px being below the SDF threshold: its step is 20
    // against 27 times 0.75
    checkSdfLines(20.0f, 0.75f, 20);
}
