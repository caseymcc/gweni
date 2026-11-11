/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's grid (style 8, drawGrid), drawn offscreen and read back at
 *  scales 1, 1.5 and 2, over origins with every quarter-unit phase, inside the
 *  rect and before it:
 *
 *    columns  each vertical line covers the S = max(1, snap(1)) whole device
 *             columns a vertical drawLine of thickness 1 at that x covers (the
 *             same frame drawn with drawLine is compared column by column), at
 *             origin + k*spacing for every k whose line is in the rect, and so
 *             do the rows of the horizontal lines;
 *    colours  the lines a whole number of majorEvery spacings from the origin
 *             are in the major colour, the others in the minor one, a crossing
 *             in the major one if either line through it is major;
 *    nothing  a pixel on no line is left as it was, depth included: a rect
 *    else     drawn behind the grid after it still shows between the lines,
 *             and nothing is drawn outside the grid's rect.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::PointF;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 255, 255);
const Color minor(255, 0, 0, 255);
const Color major(0, 255, 0, 255);
const Color behind(255, 255, 0, 255);

//! The render offset each case draws at, so a logical position is not a multiple
//! of the scale.
const Point offset(3, 2);

struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }
};

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
        renderer.setClipRegion(Rect(Point(0, 0), gweni::test::viewSize()));
    }
};

struct Primitives
{
    explicit Primitives(size_t count)
        : ids(count)
    {
        for(size_t &id:ids)
            gweni::test::gpuRenderer().generatePrimitive(id);
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

//! The device columns (rows) a frame's pixels in \p color cover along a line
//! across the view at \p at (a row for columns, a column for rows), from
//! \p first to \p last.
std::set<int> covered(const Frame &frame, bool columns, int at, int first, int last,
    const std::vector<Color> &colors)
{
    std::set<int> result;

    for(int i=first; i < last; ++i)
    {
        const Color pixel=columns?frame.at(i, at):frame.at(at, i);

        for(const Color &color : colors)
            if(pixel == color)
                result.insert(i);
    }
    return result;
}

}//namespace

TEST_CASE("Grid (style 8): its lines cover the columns and rows drawLine's strokes do, majors every N")
{
    const float scales[]={1.0f, 1.5f, 2.0f};
    const float phases[]={0.0f, 0.25f, 0.5f, 0.75f};
    // the grid's rect in canvas units, at the render offset
    const Rect rect(10, 6, 150, 66);
    const float spacing=13.25f;
    const int majorEvery=4;
    Primitives prims(64);

    for(float scale : scales)
    {
        ScaleGuard guard(scale);
        // the rect's device px, as translate() snaps its edges
        auto snap=[&](float v) { return int(std::floor(v*scale+0.5f)); };
        const int left=snap(float(rect.x+offset.x));
        const int top=snap(float(rect.y+offset.y));
        const int right=snap(float(rect.x+rect.w+offset.x));
        const int bottom=snap(float(rect.y+rect.h+offset.y));

        for(float phase : phases)
        {
            // an origin before the rect, so the first lines are at k > 0
            for(float start : {-31.0f, 22.0f})
            {
                const PointF origin(rect.x+start+phase, rect.y+start*0.5f+phase);

                CAPTURE(scale);
                CAPTURE(phase);
                CAPTURE(start);

                const Frame grid=drawFrame([&](SlotRenderer &renderer)
                    {
                        renderer.drawGrid(prims.ids[0], rect, origin, spacing, majorEvery, minor, major, 2);
                        for(size_t slot=1; slot < prims.ids.size(); ++slot)
                            renderer.hidePrimitive(prims.ids[slot]);
                    });

                // the same lines drawn one by one: a vertical (horizontal)
                // drawLine across the rect at each line's coordinate, minor or major
                std::vector<float> xs, ys;
                std::vector<bool> xMajor, yMajor;

                for(int k=-20; k < 40; ++k)
                {
                    const float x=origin.x+k*spacing;
                    const float y=origin.y+k*spacing;
                    const bool isMajor=((k%majorEvery)+majorEvery)%majorEvery == 0;

                    if((x > rect.x) && (x < rect.x+rect.w-1))
                    {
                        xs.push_back(x);
                        xMajor.push_back(isMajor);
                    }
                    if((y > rect.y) && (y < rect.y+rect.h-1))
                    {
                        ys.push_back(y);
                        yMajor.push_back(isMajor);
                    }
                }
                REQUIRE(xs.size()+ys.size()+1 <= prims.ids.size());

                const Frame lines=drawFrame([&](SlotRenderer &renderer)
                    {
                        size_t slot=0;

                        for(size_t i=0; i < xs.size(); ++i)
                        {
                            renderer.setDrawColor(xMajor[i]?major:minor);
                            renderer.drawLine(prims.ids[slot++], PointF(xs[i], float(rect.y)),
                                PointF(xs[i], float(rect.y+rect.h)), 1.0f, 2);
                        }
                        for(size_t i=0; i < ys.size(); ++i)
                        {
                            renderer.setDrawColor(yMajor[i]?major:minor);
                            renderer.drawLine(prims.ids[slot++], PointF(float(rect.x), ys[i]),
                                PointF(float(rect.x+rect.w), ys[i]), 1.0f, 2);
                        }
                        for(; slot < prims.ids.size(); ++slot)
                            renderer.hidePrimitive(prims.ids[slot]);
                    });

                const std::vector<Color> both{minor, major};
                // a row and a column halfway between two lines, where only the
                // lines across them show
                REQUIRE(xs.size() >= 2);
                REQUIRE(ys.size() >= 2);
                const int row=snap(0.5f*(ys[0]+ys[1])+offset.y);
                const int column=snap(0.5f*(xs[0]+xs[1])+offset.x);

                CHECK(covered(grid, true, row, left, right, both)
                    == covered(lines, true, row, left, right, both));
                CHECK(covered(grid, false, column, top, bottom, both)
                    == covered(lines, false, column, top, bottom, both));
                CHECK(covered(grid, true, row, left, right, {major})
                    == covered(lines, true, row, left, right, {major}));
                CHECK(covered(grid, false, column, top, bottom, {major})
                    == covered(lines, false, column, top, bottom, {major}));

                // S whole columns a line, the stroke drawLine gives
                const int stroke=std::max(1, int(std::floor(scale+0.5f)));
                const std::set<int> columns=covered(grid, true, row, left, right, both);
                std::set<int> starts;

                for(int c : columns)
                    if(columns.count(c-1) == 0)
                        starts.insert(c);
                for(int c : starts)
                    for(int i=0; i < stroke; ++i)
                        CHECK(columns.count(c+i) == 1);

                // nothing outside the rect
                int outside=0;

                for(int y=0; y < bottom+4; ++y)
                    for(int x=0; x < right+4; ++x)
                        if(((x < left) || (x >= right) || (y < top) || (y >= bottom))
                            && !(grid.at(x, y) == background))
                            ++outside;
                CHECK(outside == 0);
            }
        }
    }
}

TEST_CASE("Grid (style 8): a pixel on no line writes no depth")
{
    Primitives prims(2);
    ScaleGuard guard(1.0f);
    const Rect rect(10, 10, 100, 80);

    // the grid at z 5 first, then a rect at z 2 behind it: it shows between
    // the lines and not on them
    const Frame frame=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.drawGrid(prims.ids[0], rect, PointF(10.0f, 10.0f), 20.0f, 4, minor, major, 5);
            renderer.drawRoundedRect(prims.ids[1], rect, 0.0f, 0.0f, behind, behind, behind, 2);
        });

    const int x0=rect.x+offset.x;
    const int y0=rect.y+offset.y;

    // a line on the rect's first column and row, majors there, minors 20 on
    CHECK((frame.at(x0, y0+5) == major));
    CHECK((frame.at(x0+20, y0+5) == minor));
    CHECK((frame.at(x0+80, y0+5) == major));
    CHECK((frame.at(x0+5, y0+20) == minor));
    // between the lines, the rect drawn after it, behind it
    CHECK((frame.at(x0+5, y0+5) == behind));
    CHECK((frame.at(x0+30, y0+45) == behind));
}
