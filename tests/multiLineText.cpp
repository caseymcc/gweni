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
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
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

