/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's window-button icon glyphs (style 5, drawIcon kinds 2-5), drawn
 *  offscreen and read back pixel by pixel. The chevron (kind 1) points the way
 *  renderer::Base documents its direction, 0 down, 1 up, 2 left, 3 right, at
 *  scales 1, 1.5 and 2: the shader had 2 and 3 swapped, so a horizontal scroll
 *  bar's left arrow pointed right. Kinds 2-5 are measured in device px
 *  and fill the square centred in their rect, stroked max(1, snap(1)) px wide:
 *
 *    2 cross    both diagonals of the square, anti-aliased; mirror-symmetric
 *               and centred in the rect at every scale.
 *    3 dash     the square's width, s rows at (the floor of) its middle.
 *    4 square   the square's outline, s px on its inner side.
 *    5 restore  a front square at the bottom left and the outline of one offset
 *               up and right, drawn only outside the front one.
 *
 *  The strokes of 3-5 are whole rows and columns of fully covered pixels, with
 *  nothing partly covered beside them, at scales 1, 1.5 and 2.
 *
 *  Kind 6, a separator's rule, is s whole rows (columns for direction 1) the
 *  quad's whole length, at the floor of its middle, at scales 1, 1.25, 1.5 and 2
 *  and at every phase of the pixel grid; and the pixels it leaves uncovered write
 *  no depth, so a rect at a lower z drawn after it shows through them.
 *
 *  The renderer is shared with the other cases, so each case puts the scale and
 *  the render offset back when it ends (ScaleGuard).
 *
 *  The cross is checked in odd, even and non-square rects: mirror-symmetric,
 *  centred and inside its rect. The dash, square and restore are checked
 *  against exactly the expected pixels, the restore's back square only outside
 *  the front one. The rule is drawn horizontal and vertical at four
 *  consecutive positions and covers nothing else.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 255, 255);
const Color red(255, 0, 0, 255);

const float scales[]={1.0f, 1.5f, 2.0f};

//! The render offset each case draws at, so a logical position is not a multiple
//! of the scale.
const Point offset(3, 2);

//! One frame read back from the offscreen framebuffer, top-down RGBA8.
struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }

    //! The glyph's coverage at (x,y): red over blue, so red's channel is it.
    double coverage(int x, int y) const
    {
        return at(x, y).r/255.0;
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

//! The device rect a logical rect lands on at `scale`, both edges snapped with
//! the render offset, as translate() does.
Rect deviceRect(const Rect &logical, float scale)
{
    const int x0=SlotRenderer::snap(float(logical.x+offset.x), scale);
    const int y0=SlotRenderer::snap(float(logical.y+offset.y), scale);
    const int x1=SlotRenderer::snap(float(logical.x+logical.w+offset.x), scale);
    const int y1=SlotRenderer::snap(float(logical.y+logical.h+offset.y), scale);

    return Rect(x0, y0, x1-x0, y1-y0);
}

//! The stroke drawIcon gives kinds 2-5 at `scale`, in device px.
int strokeAt(float scale)
{
    return std::max(1, SlotRenderer::snap(1.0f, scale));
}

bool isBackground(const Color &c)
{
    return std::abs(c.r-background.r) <= 2 && std::abs(c.g-background.g) <= 2
        && std::abs(c.b-background.b) <= 2;
}

bool isGlyph(const Color &c)
{
    return std::abs(c.r-red.r) <= 2 && std::abs(c.g-red.g) <= 2 && std::abs(c.b-red.b) <= 2;
}

//! Pixels in a 4 px ring around `rect` that are not the background.
int outsideRingMismatches(const Frame &frame, const Rect &rect)
{
    int mismatches=0;

    for(int y=rect.y-4; y<rect.y+rect.h+4; ++y)
    {
        for(int x=rect.x-4; x<rect.x+rect.w+4; ++x)
        {
            const bool inside=x >= rect.x && x < rect.x+rect.w && y >= rect.y && y < rect.y+rect.h;

            if(!inside && !isBackground(frame.at(x, y)))
                ++mismatches;
        }
    }
    return mismatches;
}

//! A half-open box of device px, [x0,x1) x [y0,y1).
struct Box
{
    int x0, y0, x1, y1;

    bool contains(int x, int y) const
    {
        return x >= x0 && x < x1 && y >= y0 && y < y1;
    }

    Box inset(int s) const
    {
        return {x0+s, y0+s, x1-s, y1-s};
    }
};

bool onOutline(const Box &box, int s, int x, int y)
{
    return box.contains(x, y) && !box.inset(s).contains(x, y);
}

//! The glyph's square in `rect`: min(w, h) on a side, centred on whole px.
Box glyphSquare(const Rect &rect)
{
    const int side=std::min(rect.w, rect.h);
    const int x0=rect.x+(rect.w-side)/2;
    const int y0=rect.y+(rect.h-side)/2;

    return {x0, y0, x0+side, y0+side};
}

//! The restore glyph's front and back squares.
void restoreSquares(const Box &square, int s, Box &front, Box &back)
{
    const int side=square.x1-square.x0;
    const int off=std::max(int(std::floor(side/5.0+0.5)), s+1);

    front={square.x0, square.y0+off, square.x1-off, square.y1};
    back={square.x0+off, square.y0, square.x1, square.y1-off};
}

//! Whether kind 3, 4 or 5 covers device pixel (x,y) of `rect`.
bool expectedCovered(int kind, const Rect &rect, int s, int x, int y)
{
    const Box square=glyphSquare(rect);

    if(kind == 3)
    {
        const int side=square.x1-square.x0;
        const int row=square.y0+(side-s)/2;

        return Box{square.x0, row, square.x1, row+s}.contains(x, y);
    }
    if(kind == 4)
        return onOutline(square, s, x, y);

    Box front, back;

    restoreSquares(square, s, front, back);
    return onOutline(front, s, x, y) || (onOutline(back, s, x, y) && !front.contains(x, y));
}

//! Lengths of the runs of glyph pixels along a row (dx=1) or a column (dy=1).
std::vector<int> runs(const Frame &frame, int x, int y, int dx, int dy, int count)
{
    std::vector<int> lengths;
    int run=0;

    for(int i=0; i<count; ++i)
    {
        if(isGlyph(frame.at(x+i*dx, y+i*dy)))
            ++run;
        else if(run > 0)
        {
            lengths.push_back(run);
            run=0;
        }
    }
    if(run > 0)
        lengths.push_back(run);
    return lengths;
}

}//namespace

TEST_CASE("Icon kind 1 (chevron) points the way its direction says: 0 down, 1 up, 2 left, 3 right")
{
    Primitives prims(1);
    const Rect logical(21, 33, 15, 15);
    const char *names[]={"down", "up", "left", "right"};
    // the way each direction points, in device px
    const int pointX[]={0, 0, -1, 1};
    const int pointY[]={1, -1, 0, 0};

    for(float scale:scales)
    {
        ScaleGuard guard(scale);
        const Rect rect=deviceRect(logical, scale);
        const double centreX=rect.x+rect.w/2.0;
        const double centreY=rect.y+rect.h/2.0;

        for(int direction=0; direction < 4; ++direction)
        {
            const std::string name=names[direction];

            CAPTURE(scale);
            CAPTURE(name);

            const Frame frame=drawFrame([&](SlotRenderer &renderer)
                {
                    renderer.drawIcon(prims.ids[0], logical, 1, direction, red, 1);
                });

            CHECK(outsideRingMismatches(frame, rect) == 0);

            // The apex is on the centre line across the pointing axis, the arms'
            // ends off it: the ink on that line lies further along the pointing
            // axis than the glyph's ink as a whole.
            double ink=0.0;
            double along=0.0;
            double lineInk=0.0;
            double lineAlong=0.0;

            for(int y=rect.y; y<rect.y+rect.h; ++y)
            {
                for(int x=rect.x; x<rect.x+rect.w; ++x)
                {
                    const double a=frame.coverage(x, y);
                    const double t=(x+0.5-centreX)*pointX[direction]+(y+0.5-centreY)*pointY[direction];
                    const double across=(x+0.5-centreX)*pointY[direction]-(y+0.5-centreY)*pointX[direction];

                    ink+=a;
                    along+=a*t;
                    if(std::fabs(across) <= scale)
                    {
                        lineInk+=a;
                        lineAlong+=a*t;
                    }
                }
            }
            REQUIRE(ink > 1.0);
            REQUIRE(lineInk > 0.5);

            const double glyph=along/ink;
            const double apex=lineAlong/lineInk;

            CAPTURE(glyph);
            CAPTURE(apex);
            CHECK(apex > glyph+scale);
            CHECK(apex > 0.0);
        }
    }
}

TEST_CASE("Icon kind 2 (cross): symmetric and centred in odd and even rects, at scales 1, 1.5 and 2")
{
    Primitives prims(1);
    const Rect logicalRects[]={Rect(21, 33, 11, 11), Rect(51, 27, 10, 10), Rect(81, 41, 12, 9)};

    for(float scale:scales)
    {
        ScaleGuard guard(scale);

        for(const Rect &logical:logicalRects)
        {
            const Rect rect=deviceRect(logical, scale);

            CAPTURE(scale);
            CAPTURE(rect.x);
            CAPTURE(rect.y);
            CAPTURE(rect.w);
            CAPTURE(rect.h);

            const Frame frame=drawFrame([&](SlotRenderer &renderer)
                {
                    renderer.drawIcon(prims.ids[0], logical, 2, 0, red, 1);
                });

            CHECK(outsideRingMismatches(frame, rect) == 0);

            double ink=0.0;
            double sumX=0.0;
            double sumY=0.0;
            int asymmetric=0;

            for(int y=rect.y; y<rect.y+rect.h; ++y)
            {
                for(int x=rect.x; x<rect.x+rect.w; ++x)
                {
                    const double a=frame.coverage(x, y);
                    const int mirrorX=2*rect.x+rect.w-1-x;
                    const int mirrorY=2*rect.y+rect.h-1-y;

                    if(std::fabs(a-frame.coverage(mirrorX, y)) > 2.5/255.0
                        || std::fabs(a-frame.coverage(x, mirrorY)) > 2.5/255.0)
                        ++asymmetric;
                    ink+=a;
                    sumX+=a*(x+0.5);
                    sumY+=a*(y+0.5);
                }
            }
            REQUIRE(ink > 1.0);
            CHECK(asymmetric == 0);

            const double offX=sumX/ink-(rect.x+rect.w/2.0);
            const double offY=sumY/ink-(rect.y+rect.h/2.0);

            CAPTURE(offX);
            CAPTURE(offY);
            CHECK(std::fabs(offX) < 0.5);
            CHECK(std::fabs(offY) < 0.5);

            // both diagonals reach the square's corners: its four corner pixels have ink
            const Box square=glyphSquare(rect);

            CHECK(frame.coverage(square.x0, square.y0) > 0.1);
            CHECK(frame.coverage(square.x1-1, square.y0) > 0.1);
            CHECK(frame.coverage(square.x0, square.y1-1) > 0.1);
            CHECK(frame.coverage(square.x1-1, square.y1-1) > 0.1);
        }
    }
}

TEST_CASE("Icon kinds 3-5 (dash, square, restore): whole fully covered rows and columns at scales 1, 1.5 and 2")
{
    Primitives prims(1);
    const Rect logicalRects[]={Rect(21, 33, 10, 10), Rect(51, 27, 11, 11), Rect(81, 41, 12, 9)};

    for(float scale:scales)
    {
        ScaleGuard guard(scale);
        const int s=strokeAt(scale);

        for(int kind=3; kind <= 5; ++kind)
        {
            for(const Rect &logical:logicalRects)
            {
                const Rect rect=deviceRect(logical, scale);

                CAPTURE(scale);
                CAPTURE(kind);
                CAPTURE(rect.x);
                CAPTURE(rect.y);
                CAPTURE(rect.w);
                CAPTURE(rect.h);

                const Frame frame=drawFrame([&](SlotRenderer &renderer)
                    {
                        renderer.drawIcon(prims.ids[0], logical, kind, 0, red, 1);
                    });

                CHECK(outsideRingMismatches(frame, rect) == 0);

                // every pixel is the glyph colour or the background, never between,
                // and exactly where the glyph's strokes are
                int partial=0;
                int wrong=0;

                for(int y=rect.y; y<rect.y+rect.h; ++y)
                {
                    for(int x=rect.x; x<rect.x+rect.w; ++x)
                    {
                        const Color pixel=frame.at(x, y);
                        const bool glyph=isGlyph(pixel);

                        if(!glyph && !isBackground(pixel))
                            ++partial;
                        else if(glyph != expectedCovered(kind, rect, s, x, y))
                            ++wrong;
                    }
                }
                CHECK(partial == 0);
                CHECK(wrong == 0);

                // the stroke widths, read off the pixels on their own
                const Box square=glyphSquare(rect);
                const int side=square.x1-square.x0;
                const int midX=square.x0+side/2;
                const int midY=square.y0+side/2;
                const std::vector<int> column=runs(frame, midX, square.y0, 0, 1, side);
                const std::vector<int> row=runs(frame, square.x0, midY, 1, 0, side);

                if(kind == 3)
                {
                    CHECK(column == std::vector<int>{s});
                    CHECK(runs(frame, square.x0, square.y0+(side-s)/2, 1, 0, side)
                        == std::vector<int>{side});
                }
                else if(kind == 4)
                {
                    CHECK(column == std::vector<int>{s, s});
                    CHECK(row == std::vector<int>{s, s});
                    CHECK(isGlyph(frame.at(square.x0, square.y0)));
                    CHECK(isGlyph(frame.at(square.x1-1, square.y1-1)));
                }
                else
                {
                    Box front, back;

                    restoreSquares(square, s, front, back);

                    // nothing of the back square inside the front one
                    int insideFront=0;

                    for(int y=front.y0+s; y<front.y1-s; ++y)
                        for(int x=front.x0+s; x<front.x1-s; ++x)
                            if(!isBackground(frame.at(x, y)))
                                ++insideFront;
                    CHECK(insideFront == 0);
                    // the back square shows at the top right, apart from the front one
                    CHECK(isGlyph(frame.at(square.x1-1, square.y0)));
                    CHECK(isBackground(frame.at(front.x1-1-s, front.y0-1)));
                    // a column through both squares: back top, front top, front bottom
                    CHECK(runs(frame, front.x1-1-s, square.y0, 0, 1, side)
                        == std::vector<int>{s, s, s});
                }
            }
        }
    }
}

TEST_CASE("Icon kind 6 (rule): s whole rows or columns the quad's length, wherever it lands, at scales 1, 1.25, 1.5 and 2")
{
    Primitives prims(1);
    const float ruleScales[]={1.0f, 1.25f, 1.5f, 2.0f};

    for(float scale:ruleScales)
    {
        ScaleGuard guard(scale);
        const int s=strokeAt(scale);

        // a separator's quad: one logical unit tall plus one on each side, at four
        // consecutive positions, so every phase of the grid at 1.25 and 1.5 is met
        for(int direction=0; direction <= 1; ++direction)
        {
            for(int step=0; step < 4; ++step)
            {
                const Rect logical=direction == 0?Rect(17, 30+step, 41, 3):Rect(30+step, 17, 3, 41);
                const Rect rect=deviceRect(logical, scale);

                CAPTURE(scale);
                CAPTURE(direction);
                CAPTURE(rect.x);
                CAPTURE(rect.y);
                CAPTURE(rect.w);
                CAPTURE(rect.h);

                const Frame frame=drawFrame([&](SlotRenderer &renderer)
                    {
                        renderer.drawIcon(prims.ids[0], logical, 6, direction, red, 1);
                    });

                CHECK(outsideRingMismatches(frame, rect) == 0);

                // the stroke's first row (column), at the floor of the quad's middle
                const int across=direction == 0?rect.h:rect.w;
                const int first=(direction == 0?rect.y:rect.x)+(across-s)/2;
                int partial=0;
                int wrong=0;

                for(int y=rect.y; y<rect.y+rect.h; ++y)
                {
                    for(int x=rect.x; x<rect.x+rect.w; ++x)
                    {
                        const Color pixel=frame.at(x, y);
                        const bool glyph=isGlyph(pixel);
                        const int at=direction == 0?y:x;

                        if(!glyph && !isBackground(pixel))
                            ++partial;
                        else if(glyph != (at >= first && at < first+s))
                            ++wrong;
                    }
                }
                CHECK(partial == 0);
                CHECK(wrong == 0);

                // read off the pixels: s thick across the middle, the quad's whole length
                if(direction == 0)
                {
                    CHECK(runs(frame, rect.x+rect.w/2, rect.y, 0, 1, rect.h) == std::vector<int>{s});
                    CHECK(runs(frame, rect.x, first, 1, 0, rect.w) == std::vector<int>{rect.w});
                }
                else
                {
                    CHECK(runs(frame, rect.x, rect.y+rect.h/2, 1, 0, rect.w) == std::vector<int>{s});
                    CHECK(runs(frame, first, rect.y, 0, 1, rect.h) == std::vector<int>{rect.h});
                }
            }
        }
    }
}

TEST_CASE("Icon pixels a glyph leaves uncovered write no depth: a rect behind a rule's quad shows around the stroke")
{
    // the rule's slot comes first, so it is drawn before the rect at a lower z
    Primitives prims(2);
    const Color green(0, 255, 0, 255);
    const Rect rule(17, 30, 41, 5);
    const Rect behind(10, 25, 60, 15);

    for(float scale:scales)
    {
        ScaleGuard guard(scale);
        const Rect rect=deviceRect(rule, scale);
        const int s=strokeAt(scale);
        const int first=rect.y+(rect.h-s)/2;

        CAPTURE(scale);

        const Frame frame=drawFrame([&](SlotRenderer &renderer)
            {
                renderer.drawIcon(prims.ids[0], rule, 6, 0, red, 2);
                renderer.drawRoundedRect(prims.ids[1], behind, 0.0f, 0.0f, green, green, green, 1);
            });

        int wrong=0;

        for(int y=rect.y; y<rect.y+rect.h; ++y)
        {
            for(int x=rect.x; x<rect.x+rect.w; ++x)
            {
                const Color pixel=frame.at(x, y);
                const bool stroke=y >= first && y < first+s;

                if(stroke?!isGlyph(pixel):(pixel.g < 250 || pixel.r > 5))
                    ++wrong;
            }
        }
        CHECK(wrong == 0);
    }
}
