/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  drawRoundedRect's edge mask (renderer::RoundedEdge), drawn offscreen on the
 *  backend under test and read back pixel by pixel; the packing is checked
 *  headless in tests/unit/roundedEdges.cpp.
 *
 *  A square corner is the rect's own corner: its corner pixel is fully covered
 *  and the radius x radius block there is all fill (or border along the bordered
 *  sides), while a rounded corner of the same rect leaves its corner pixel
 *  untouched. A side without a border has no border-coloured pixel along it: its
 *  outer rows, between the borders of the sides next to it, are the fill, and
 *  those sides' borders run straight to its edge; the bordered sides keep their
 *  N px. A mask of 0 draws the unmasked rect's pixels byte for byte, gradient,
 *  radius and border included.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;
using gweni::renderer::SlotRenderer;
namespace edge=gweni::renderer;

const Color background(0, 0, 255, 255);
const Color fill(255, 0, 0, 255);
const Color borderColor(0, 255, 0, 255);

const Rect testRect(16, 16, 96, 64);
const int radius=12;

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

//! Primitive ids for one test case, released (and that flushed) when it ends.
struct Primitives
{
    explicit Primitives(size_t count)
        : ids(count)
    {
        for(size_t &id:ids)
            gweni::test::gpuRenderer().generatePrimitive(id);
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

bool same(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

//! Pixels in [x0,x1) x [y0,y1) that are not `expected`, the first one reported.
int mismatches(const Frame &frame, int x0, int y0, int x1, int y1, const Color &expected)
{
    int count=0;

    for(int y=y0; y<y1; ++y)
        for(int x=x0; x<x1; ++x)
            if(!same(frame.at(x, y), expected))
            {
                if(count == 0)
                {
                    const Color p=frame.at(x, y);

                    MESSAGE("first mismatch at (", x, ",", y, "): (", int(p.r), ",", int(p.g), ",",
                        int(p.b), "), expected (", int(expected.r), ",", int(expected.g), ",",
                        int(expected.b), ")");
                }
                ++count;
            }
    return count;
}

//! Pixels of the ring just outside `rect` that are not the background.
int outsideRingMismatches(const Frame &frame, const Rect &rect)
{
    return mismatches(frame, rect.x-1, rect.y-1, rect.x+rect.w+1, rect.y, background)
        +mismatches(frame, rect.x-1, rect.y+rect.h, rect.x+rect.w+1, rect.y+rect.h+1, background)
        +mismatches(frame, rect.x-1, rect.y, rect.x, rect.y+rect.h, background)
        +mismatches(frame, rect.x+rect.w, rect.y, rect.x+rect.w+1, rect.y+rect.h, background);
}

struct Corner
{
    const char *name;
    uint8_t bit;
    bool right, bottom;
};

const Corner corners[]=
{
    {"top-left", edge::SquareTopLeft, false, false},
    {"top-right", edge::SquareTopRight, true, false},
    {"bottom-left", edge::SquareBottomLeft, false, true},
    {"bottom-right", edge::SquareBottomRight, true, true}
};

//! The corner's radius x radius block of `rect`, as [x0,x1) x [y0,y1).
void cornerBlock(const Rect &rect, const Corner &corner, int out[4])
{
    out[0]=corner.right ? rect.x+rect.w-radius : rect.x;
    out[1]=corner.bottom ? rect.y+rect.h-radius : rect.y;
    out[2]=out[0]+radius;
    out[3]=out[1]+radius;
}

//! The corner pixel of the corner.
Color cornerPixel(const Frame &frame, const Rect &rect, const Corner &corner)
{
    return frame.at(corner.right ? rect.x+rect.w-1 : rect.x, corner.bottom ? rect.y+rect.h-1 : rect.y);
}

struct Side
{
    const char *name;
    uint8_t noBorder;
    uint8_t squareCorners; //!< the two corners on it
};

const Side sides[]=
{
    {"top", edge::NoBorderTop, edge::SquareTopLeft | edge::SquareTopRight},
    {"right", edge::NoBorderRight, edge::SquareTopRight | edge::SquareBottomRight},
    {"bottom", edge::NoBorderBottom, edge::SquareBottomLeft | edge::SquareBottomRight},
    {"left", edge::NoBorderLeft, edge::SquareTopLeft | edge::SquareBottomLeft}
};

}//namespace

TEST_CASE("Rounded rect edge mask: a square corner is fully covered, the rounded ones stay round")
{
    Primitives prims(1);

    for(const Corner &square:corners)
    {
        CAPTURE(square.name);

        const Frame frame=drawFrame([&](SlotRenderer &renderer)
            {
                renderer.drawRoundedRect(prims.ids[0], testRect, float(radius), 0.0f, fill, fill,
                    borderColor, 1, square.bit);
            });

        for(const Corner &corner:corners)
        {
            CAPTURE(corner.name);
            if(&corner == &square)
            {
                int block[4];

                cornerBlock(testRect, corner, block);
                CHECK(same(cornerPixel(frame, testRect, corner), fill));
                CHECK(mismatches(frame, block[0], block[1], block[2], block[3], fill) == 0);
            }
            else
                CHECK(same(cornerPixel(frame, testRect, corner), background));
        }
        // the straight edges between the corners are fully covered, nothing outside
        CHECK(mismatches(frame, testRect.x+radius, testRect.y, testRect.x+testRect.w-radius,
            testRect.y+testRect.h, fill) == 0);
        CHECK(mismatches(frame, testRect.x, testRect.y+radius, testRect.x+testRect.w,
            testRect.y+testRect.h-radius, fill) == 0);
        CHECK(outsideRingMismatches(frame, testRect) == 0);
    }

    SUBCASE("all four square: the whole rect, with its border to the corners")
    {
        const uint8_t all=edge::SquareTopLeft | edge::SquareTopRight | edge::SquareBottomLeft
            | edge::SquareBottomRight;
        const Frame frame=drawFrame([&](SlotRenderer &renderer)
            {
                renderer.drawRoundedRect(prims.ids[0], testRect, float(radius), 1.0f, fill, fill,
                    borderColor, 1, all);
            });

        for(const Corner &corner:corners)
        {
            CAPTURE(corner.name);
            CHECK(same(cornerPixel(frame, testRect, corner), borderColor));
        }
        CHECK(mismatches(frame, testRect.x+1, testRect.y+1, testRect.x+testRect.w-1,
            testRect.y+testRect.h-1, fill) == 0);
        CHECK(outsideRingMismatches(frame, testRect) == 0);
    }
}

TEST_CASE("Rounded rect edge mask: a side without a border has no border-coloured pixel")
{
    Primitives prims(1);
    const int x0=testRect.x;
    const int y0=testRect.y;
    const int x1=testRect.x+testRect.w;
    const int y1=testRect.y+testRect.h;

    for(int border:{1, 2, 3})
    {
        for(const Side &side:sides)
        {
            CAPTURE(border);
            CAPTURE(side.name);

            const Frame frame=drawFrame([&](SlotRenderer &renderer)
                {
                    renderer.drawRoundedRect(prims.ids[0], testRect, float(radius), float(border),
                        fill, fill, borderColor, 1, side.noBorder | side.squareCorners);
                });

            // Each side's outer `border` rows (columns) along its straight part:
            // border colour, or fill on the side without one; the borders of the
            // sides next to that one run to its edge.
            const bool top=side.noBorder == edge::NoBorderTop;
            const bool right=side.noBorder == edge::NoBorderRight;
            const bool bottom=side.noBorder == edge::NoBorderBottom;
            const bool left=side.noBorder == edge::NoBorderLeft;
            const uint8_t square=side.squareCorners;
            // How far a side's straight part starts from the corner at one end:
            // a rounded corner's radius; at a square one, nothing on a bordered
            // side, and the border of the side next to it on the one without.
            auto inset=[&](uint8_t corner, bool borderless)
            {
                return (square & corner) == 0 ? radius : (borderless ? border : 0);
            };

            CHECK(mismatches(frame, x0+inset(edge::SquareTopLeft, top), y0,
                x1-inset(edge::SquareTopRight, top), y0+border, top ? fill : borderColor) == 0);
            CHECK(mismatches(frame, x0+inset(edge::SquareBottomLeft, bottom), y1-border,
                x1-inset(edge::SquareBottomRight, bottom), y1, bottom ? fill : borderColor) == 0);
            CHECK(mismatches(frame, x0, y0+inset(edge::SquareTopLeft, left), x0+border,
                y1-inset(edge::SquareBottomLeft, left), left ? fill : borderColor) == 0);
            CHECK(mismatches(frame, x1-border, y0+inset(edge::SquareTopRight, right), x1,
                y1-inset(edge::SquareBottomRight, right), right ? fill : borderColor) == 0);
            // the inside, from the edge without a border to where the rounded
            // corners on the far side begin
            CHECK(mismatches(frame, left ? x0 : x0+(right ? radius : border), top ? y0 : y0+(bottom ? radius : border),
                right ? x1 : x1-(left ? radius : border), bottom ? y1 : y1-(top ? radius : border),
                fill) == 0);
            // the square corners are the bordered side's border, fully covered
            for(const Corner &corner:corners)
            {
                if((side.squareCorners & corner.bit) == 0)
                    continue;
                CAPTURE(corner.name);
                CHECK(same(cornerPixel(frame, testRect, corner), borderColor));
            }
            CHECK(outsideRingMismatches(frame, testRect) == 0);
        }
    }
}

TEST_CASE("Rounded rect edge mask: a mask of 0 draws the unmasked rect byte for byte")
{
    Primitives prims(3);
    const Color top(200, 120, 40, 255);
    const Color bottom(30, 60, 190, 255);
    const Color border(240, 240, 240, 200);
    const Rect rects[]={Rect(10, 10, 61, 33), Rect(80, 12, 40, 40), Rect(20, 60, 100, 23)};

    for(float scale:{1.0f, 1.5f})
    {
        CAPTURE(scale);

        SlotRenderer &renderer=gweni::test::gpuRenderer();
        const float before=renderer.scale();

        renderer.setScale(scale);

        auto draw=[&](bool masked)
        {
            return drawFrame([&](SlotRenderer &r)
                {
                    for(size_t i=0; i<3; ++i)
                    {
                        const float rad=float(3+5*i);
                        const float bor=float(i);

                        if(masked)
                            r.drawRoundedRect(prims.ids[i], rects[i], rad, bor, top, bottom, border,
                                int(1+i), 0);
                        else
                            r.drawRoundedRect(prims.ids[i], rects[i], rad, bor, top, bottom, border,
                                int(1+i));
                    }
                });
        };

        const Frame unmasked=draw(false);
        const Frame masked=draw(true);

        renderer.setScale(before);
        CHECK(unmasked.pixels == masked.pixels);
        // and the frame drew something
        CHECK(!same(unmasked.at(30, 20), background));
    }
}
