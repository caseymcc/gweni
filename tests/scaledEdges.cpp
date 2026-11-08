/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Rects drawn at a fractional scale land on whole device pixels and meet
 *  edge to edge: translate() snaps both edges of a rect (x0=round(x*s),
 *  x1=round((x+w)*s), w=x1-x0) rather than its origin and size apart, which
 *  at 1.25 and 1.5 left adjacent rects overlapping by a pixel. The grid cases
 *  run for plain fills (style 1) and SDF rects (style 4): an SDF rect also
 *  covers its whole snapped rect, odd device sizes included, and its outermost
 *  pixels fully. Also the SDF border, which becomes a whole number of device
 *  pixels at the scale.
 *
 *  The renderer is shared with the other cases, so each case puts the scale and
 *  the render offset back when it ends (ScaleGuard).
 *
 *  The grid is adjacent 7x7 logical rects at scales 1.25, 1.5 and 2, drawn
 *  with drawFilledRect and with drawRoundedRect: in opaque alternating colours
 *  no background shows inside and the run lengths are within 1 px; in 50 %
 *  black there is no darker overlap or lighter gap. A drawLinedRect must stay
 *  closed, and a rounded-rect border must be max(1, round(border*s)) device px
 *  of exactly the border colour.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

const Color background(255, 255, 255, 255);
const Color red(255, 0, 0, 255);
const Color green(0, 255, 0, 255);
const Color halfBlack(0, 0, 0, 128);

//! The grid of adjacent squares every case draws: `columns` x `rows` cells of
//! `cell` logical units, placed by the render offset.
const int columns=16;
const int rows=3;
const int cell=7;
const Point offset(3, 2);

const float scales[]={1.25f, 1.5f, 2.0f};

//! One frame read back from the offscreen framebuffer, top-down RGBA8.
struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }
};

//! Clears to `background`, draws one frame with `draw` and reads it back.
template<typename Draw>
Frame drawFrame(Draw &&draw)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    gweni::test::clearView(background);

    renderer.begin();
    draw(renderer);
    renderer.end();

    Frame frame;

    gweni::test::readView(frame.pixels);
    return frame;
}

//! Puts the scale and the render offset back, so the next case draws at 1x.
struct ScaleGuard
{
    explicit ScaleGuard(float scale)
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setScale(scale);
        renderer.setRenderOffset(offset);
    }

    ~ScaleGuard()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.setRenderOffset(Point(0, 0));
    }
};

//! Primitive ids for one test case, released (and that flushed) when it ends, so the
//! next case's frame starts empty.
struct Primitives
{
    explicit Primitives(size_t count)
        : ids(count)
    {
        for(size_t &id:ids)
            gweni::test::gpuRenderer().generatePrimitive(id);
        // slot order is draw order: ids[0] is drawn first
        std::sort(ids.begin(), ids.end());
    }

    ~Primitives()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        for(size_t id:ids)
            renderer.releasePrimitive(id);
        renderer.begin();
        renderer.end();
    }

    std::vector<size_t> ids;
};

//! Device pixel of a logical coordinate, the rule the renderer must follow.
int device(int logical, float scale)
{
    return SlotRenderer::snap(float(logical), scale);
}

//! How the grid's cells are drawn.
enum class Cells
{
    Filled,     //!< drawFilledRect, style 1
    Rounded     //!< drawRoundedRect at radius 0 and no border, style 4
};

const Cells cellKinds[]={Cells::Filled, Cells::Rounded};

const char *name(Cells cells)
{
    return cells == Cells::Filled?"drawFilledRect":"drawRoundedRect";
}

//! Draws the grid, each cell from `colorOf(column, row)`. Each cell is nearer
//! than the one before it (zIndex grows), so where two overlap the later one is
//! drawn over the earlier rather than depth-rejected, and a blend shows it.
template<typename ColorOf>
Frame drawGrid(const Primitives &primitives, Cells cells, ColorOf &&colorOf)
{
    return drawFrame([&](SlotRenderer &renderer)
    {
        for(int row=0; row<rows; ++row)
        {
            for(int column=0; column<columns; ++column)
            {
                const int index=row*columns+column;
                const Rect rect(column*cell, row*cell, cell, cell);
                const Color color=colorOf(column, row);

                if(cells == Cells::Filled)
                {
                    renderer.setDrawColor(color);
                    renderer.drawFilledRect(primitives.ids[index], rect, index+1);
                }
                else
                {
                    renderer.drawRoundedRect(primitives.ids[index], rect, 0.0f, 0.0f, color, color,
                        color, index+1);
                }
            }
        }
    });
}

bool sameRgb(const Color &a, const Color &b, int tolerance=0)
{
    return std::abs(a.r-b.r) <= tolerance && std::abs(a.g-b.g) <= tolerance
        && std::abs(a.b-b.b) <= tolerance;
}

}//namespace

TEST_CASE("Scaled rects: adjacent opaque rects leave no background between them, sizes within 1 px")
{
    const Primitives primitives(columns*rows);

    for(Cells cells : cellKinds)
    for(float scale : scales)
    {
        CAPTURE(name(cells));
        CAPTURE(scale);

        const ScaleGuard guard(scale);
        const Frame frame=drawGrid(primitives, cells, [](int column, int row)
        {
            return ((column+row)%2 == 0) ? red : green;
        });

        const int left=device(offset.x, scale);
        const int right=device(offset.x+columns*cell, scale);
        const int top=device(offset.y, scale);
        const int bottom=device(offset.y+rows*cell, scale);

        REQUIRE(right <= gweni::test::viewSize().x);
        REQUIRE(bottom <= gweni::test::viewSize().y);

        // the grid covers exactly its snapped outline: background just outside it
        CHECK(sameRgb(frame.at(left-1, top), background));
        CHECK(sameRgb(frame.at(right, top), background));
        CHECK(sameRgb(frame.at(left, top-1), background));
        CHECK(sameRgb(frame.at(left, bottom), background));

        // no background pixel anywhere inside it, and every run of one colour
        // along a row (a cell's width) or a column (its height) is within 1 px
        // of every other
        int backgroundPixels=0;
        int widest=0;
        int narrowest=1<<30;

        auto measureRun=[&](int run)
        {
            widest=std::max(widest, run);
            narrowest=std::min(narrowest, run);
        };

        for(int y=top; y<bottom; ++y)
        {
            int run=0;
            Color last=frame.at(left, y);

            for(int x=left; x<right; ++x)
            {
                const Color c=frame.at(x, y);

                if(sameRgb(c, background))
                    ++backgroundPixels;
                if(!sameRgb(c, last))
                {
                    measureRun(run);
                    run=0;
                    last=c;
                }
                ++run;
            }
            measureRun(run);
        }

        for(int x=left; x<right; ++x)
        {
            int run=0;
            Color last=frame.at(x, top);

            for(int y=top; y<bottom; ++y)
            {
                const Color c=frame.at(x, y);

                if(!sameRgb(c, last))
                {
                    measureRun(run);
                    run=0;
                    last=c;
                }
                ++run;
            }
            measureRun(run);
        }

        CHECK(backgroundPixels == 0);
        CAPTURE(widest);
        CAPTURE(narrowest);
        CHECK(widest-narrowest <= 1);
    }
}

TEST_CASE("Scaled rects: adjacent 50%-alpha rects neither overlap nor leave a gap")
{
    const Primitives primitives(columns*rows);

    for(Cells cells : cellKinds)
    for(float scale : scales)
    {
        CAPTURE(name(cells));
        CAPTURE(scale);

        const ScaleGuard guard(scale);
        const Frame frame=drawGrid(primitives, cells, [](int, int)
        {
            return halfBlack;
        });

        const int left=device(offset.x, scale);
        const int right=device(offset.x+columns*cell, scale);
        const int top=device(offset.y, scale);
        const int bottom=device(offset.y+rows*cell, scale);

        // one 50% black over white, as the first cell's first pixel has it; a
        // pixel two cells cover is darker, one no cell covers is the background
        const Color single=frame.at(left, top);

        REQUIRE(!sameRgb(single, background, 8));

        int darker=0;
        int lighter=0;

        for(int y=top; y<bottom; ++y)
        {
            for(int x=left; x<right; ++x)
            {
                const Color c=frame.at(x, y);

                if(c.r+1 < single.r)
                {
                    if(darker == 0)
                        MESSAGE("first overlap at (", x, ",", y, ")");
                    ++darker;
                }
                else if(c.r > single.r+1)
                {
                    if(lighter == 0)
                        MESSAGE("first gap at (", x, ",", y, ")");
                    ++lighter;
                }
            }
        }

        CHECK(darker == 0);
        CHECK(lighter == 0);
    }
}

TEST_CASE("Scaled rects: drawLinedRect stays closed, every edge at least 1 device px")
{
    Primitives primitives(4);

    for(float scale : scales)
    {
        CAPTURE(scale);

        const ScaleGuard guard(scale);
        const Rect outline(0, 0, 41, 23);
        const Frame frame=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(red);
            renderer.drawLinedRect(primitives.ids.data(), outline, 1);
        });

        const int left=device(offset.x, scale);
        const int right=device(offset.x+outline.w, scale)-1;
        const int top=device(offset.y, scale);
        const int bottom=device(offset.y+outline.h, scale)-1;
        int missing=0;

        for(int x=left; x<=right; ++x)
            missing+=!sameRgb(frame.at(x, top), red)+!sameRgb(frame.at(x, bottom), red);
        for(int y=top; y<=bottom; ++y)
            missing+=!sameRgb(frame.at(left, y), red)+!sameRgb(frame.at(right, y), red);

        CHECK(missing == 0);
        // just inside the outline is the background again
        CHECK(sameRgb(frame.at((left+right)/2, (top+bottom)/2), background));
    }
}

TEST_CASE("Scaled rounded rect: the border is round(border*scale) device px, at least 1")
{
    const Primitives primitives(1);
    const Color fill(0, 0, 0, 255);
    const Color border(255, 255, 255, 255);

    for(float scale : scales)
    {
        CAPTURE(scale);

        const ScaleGuard guard(scale);
        // radius 0, so the middle row crosses straight edges only
        const Rect rect(10, 10, 60, 20);
        const Frame frame=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.drawRoundedRect(primitives.ids[0], rect, 0.0f, 1.0f, fill, fill, border, 1);
        });

        const int left=device(offset.x+rect.x, scale);
        const int middle=(device(offset.y+rect.y, scale)+device(offset.y+rect.y+rect.h, scale))/2;

        // the columns from the left edge in that read nearer the border colour
        // than the fill: exactly the border colour, the outer one fully covered,
        // and the next one exactly the fill
        int bordered=0;

        while(frame.at(left+bordered, middle).r > 127)
        {
            CHECK(sameRgb(frame.at(left+bordered, middle), border));
            ++bordered;
        }

        CHECK(bordered == std::max(1, device(1, scale)));
        CHECK(sameRgb(frame.at(left+bordered, middle), fill));
        CHECK(sameRgb(frame.at(left-1, middle), background));
    }
}
