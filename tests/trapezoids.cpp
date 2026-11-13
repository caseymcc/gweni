/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's trapezoids (style 9, drawTrapezoid), drawn offscreen and read
 *  back pixel by pixel at scales 1, 1.5 and 2, against a model of what
 *  drawTrapezoid places: its sides at the device px translate() snaps an edge to,
 *  its top and bottom edges at y*scale unsnapped, and a pixel's coverage the
 *  exact area of it between the two edges (integrated here column by column on
 *  the CPU, not with the shader's formula):
 *
 *    row        a row of trapezoids sharing their sides, each at a z of its own (so
 *               a column two of them drew would be blended twice, not rejected by
 *               the depth test), slanted, nearly flat and flat edges among them,
 *               draws every pixel at its coverage within a level, opaque and at
 *               50 % alpha: no seam, no column blended twice; and its outer sides
 *               cover whole columns, the column at each side fully covered on every
 *               row both edges are past and the column beside it untouched;
 *    flat       a trapezoid whose edges are flat on whole device px draws byte for
 *               byte what drawFilledRect of the same rect draws, opaque and
 *               translucent, at scale 1 and 2;
 *    crossed    one whose edges cross (an area segment whose line crosses its
 *               base) covers every pixel between them within a level, on both
 *               sides of the crossing, but the column the crossing is in;
 *    depth      a rect at a lower z drawn after it shows on every pixel it leaves
 *               uncovered (it writes no depth there) and not on one it covers,
 *               and a side past +/-32767 device px draws nothing;
 *    retained   drawing it again writes and uploads nothing, a moved edge
 *               rewrites its slot alone, and hiding and showing it keeps style 9.
 *
 *  GWENI_TRAPEZOIDS_DUMP=<dir> writes the row's frames to
 *  <dir>/row_<scale>x_<opaque|half>.png, to look at or to compare between backends.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
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
const Color green(0, 255, 0, 255);

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

//! Puts the scale and the render offset in place for a case; the listener in
//! main.cpp puts them back before the next one.
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
        gweni::test::resetDrawState();
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

//! A trapezoid as drawTrapezoid is given it, in canvas units.
struct Trapezoid
{
    float x0, x1, top0, top1, bottom0, bottom1;
};

//! A trapezoid as drawTrapezoid places it, in device px: its sides' columns
//! [left, right) and its edges' heights at them.
struct Placed
{
    int left, right;
    double top0, top1, bottom0, bottom1;

    //! The exact area of pixel (x,y) between the edges, crossed or not (0 outside
    //! the columns),
    //! integrated over 256 slices of the column: the overlap of a slice with the
    //! pixel's row is linear in x between kinks, so the midpoint rule is within
    //! a small fraction of a level.
    double coverage(int x, int y) const
    {
        if(x < left || x >= right)
            return 0.0;

        const int slices=256;
        const double width=right-left;
        double sum=0.0;

        for(int i=0; i<slices; ++i)
        {
            const double t=(x-left+(i+0.5)/slices)/width;
            const double top=top0+(top1-top0)*t;
            const double bottom=bottom0+(bottom1-bottom0)*t;
            const double lo=std::min(top, bottom);
            const double hi=std::max(top, bottom);

            sum+=std::max(0.0, std::min(hi, y+1.0)-std::max(lo, double(y)));
        }
        return sum/slices;
    }
};

Placed place(const Trapezoid &t, float scale)
{
    auto side=[&](float v, int o) { return int(std::floor((v+float(o))*scale+0.5f)); };
    auto height=[&](float v, int o) { return (double(v)+o)*scale; };

    return Placed{side(t.x0, offset.x), side(t.x1, offset.x),
        height(t.top0, offset.y), height(t.top1, offset.y),
        height(t.bottom0, offset.y), height(t.bottom1, offset.y)};
}

//! A stacked area's segments: sides shared, tops and bottoms continuous, among
//! them a nearly flat top (34.9 to 35.0 over 15 units), a flat one and a flat
//! bottom, sides on and off whole units.
std::vector<Trapezoid> row()
{
    const float xs[]={10.0f, 23.3f, 31.7f, 46.7f, 60.5f, 72.0f, 81.0f};
    const float tops[]={40.0f, 22.4f, 34.9f, 35.0f, 12.0f, 12.0f, 30.0f};
    const float bottoms[]={70.0f, 70.0f, 66.5f, 72.2f, 70.0f, 70.0f, 70.0f};
    std::vector<Trapezoid> trapezoids;

    for(size_t i=0; i+1 < std::size(xs); ++i)
        trapezoids.push_back({xs[i], xs[i+1], tops[i], tops[i+1], bottoms[i], bottoms[i+1]});
    return trapezoids;
}

void dump(const Frame &frame, const std::string &name)
{
    const char *dir=std::getenv("GWENI_TRAPEZOIDS_DUMP");

    if(!dir || !*dir)
        return;

    const Point size=gweni::test::viewSize();

    gweni::capture::writeRgbaPng(frame.pixels, size.x, size.y, std::string(dir)+"/"+name+".png");
}

}//namespace

TEST_CASE("Trapezoid (style 9): a row of them has no seam and blends no column twice, and its sides cover whole columns")
{
    const std::vector<Trapezoid> trapezoids=row();
    Primitives prims(trapezoids.size());

    for(float scale : scales)
    {
        ScaleGuard guard(scale);
        std::vector<Placed> placed;

        for(const Trapezoid &t : trapezoids)
            placed.push_back(place(t, scale));

        for(int alpha : {255, 128})
        {
            CAPTURE(scale);
            CAPTURE(alpha);

            const Color color(255, 0, 0, (unsigned char)alpha);
            const Frame frame=drawFrame([&](SlotRenderer &renderer)
                {
                    // each at a z of its own, so the depth test hides no double blend
                    for(size_t i=0; i<trapezoids.size(); ++i)
                    {
                        const Trapezoid &t=trapezoids[i];

                        renderer.drawTrapezoid(prims.ids[i], t.x0, t.x1, t.top0, t.top1,
                            t.bottom0, t.bottom1, color, int(10+i));
                    }
                });

            char name[64];

            std::snprintf(name, sizeof(name), "row_%gx_%s", scale, alpha == 255 ? "opaque" : "half");
            dump(frame, name);

            // every pixel at its coverage: a seam is a column drawn too lightly, a
            // column two slots drew one blended twice (at 50 %, 191 for 128)
            const int x0=placed.front().left-3;
            const int x1=placed.back().right+3;
            const int y1=std::min(gweni::test::viewSize().y, int((80.0f+offset.y)*scale)+2);
            int wrong=0;
            int partial=0;

            for(int y=0; y<y1; ++y)
            {
                for(int x=x0; x<x1; ++x)
                {
                    double c=0.0;

                    for(const Placed &p : placed)
                        c+=p.coverage(x, y);

                    const Color pixel=frame.at(x, y);
                    const double expected=255.0*c*alpha/255.0;

                    partial+=(c > 0.02 && c < 0.98);
                    if(std::abs(pixel.r-expected) > 1.5 || pixel.g != 0
                        || std::abs(pixel.b-(255.0-expected)) > 1.5)
                    {
                        if(wrong < 8)
                            MESSAGE("pixel (", x, ",", y, ") is ", int(pixel.r), ",", int(pixel.g), ",",
                                int(pixel.b), " for coverage ", c, " (red ", expected, ")");
                        ++wrong;
                    }
                }
            }
            CHECK(wrong == 0);
            // the edges are anti-aliased: the model's partly covered pixels are there
            CHECK(partial > 50);

            // the outer sides: the first and last columns fully covered on every row
            // both edges are past, the columns beside them untouched on every row
            const Placed &first=placed.front();
            const Placed &last=placed.back();
            int sides=0;
            int beside=0;

            for(int y=0; y<y1; ++y)
            {
                const bool firstInside=y >= std::max(first.top0, first.top1)
                    && y+1 <= std::min(first.bottom0, first.bottom1);
                const bool lastInside=y >= std::max(last.top0, last.top1)
                    && y+1 <= std::min(last.bottom0, last.bottom1);

                if(firstInside)
                    sides+=frame.at(first.left, y).r != alpha;
                if(lastInside)
                    sides+=frame.at(last.right-1, y).r != alpha;
                beside+=!(frame.at(first.left-1, y) == background);
                beside+=!(frame.at(last.right, y) == background);
            }
            CHECK(sides == 0);
            CHECK(beside == 0);
        }
    }
}

TEST_CASE("Trapezoid (style 9): a flat one on whole device px draws what drawFilledRect of the same rect draws")
{
    Primitives prims(1);
    const Rect rects[]={Rect(12, 9, 37, 21), Rect(60, 4, 1, 1), Rect(70, 30, 55, 3)};

    for(float scale : {1.0f, 2.0f})
    {
        ScaleGuard guard(scale);

        for(const Rect &rect : rects)
        {
            for(const Color &color : {red, Color(40, 200, 90, 100)})
            {
                CAPTURE(scale);
                CAPTURE(rect.x);
                CAPTURE(int(color.a));

                const Frame filled=drawFrame([&](SlotRenderer &renderer)
                    {
                        renderer.setDrawColor(color);
                        renderer.drawFilledRect(prims.ids[0], rect, 5);
                    });
                const Frame trapezoid=drawFrame([&](SlotRenderer &renderer)
                    {
                        const float top=float(rect.y);
                        const float bottom=float(rect.y+rect.h);

                        renderer.drawTrapezoid(prims.ids[0], float(rect.x), float(rect.x+rect.w),
                            top, top, bottom, bottom, color, 5);
                    });
                int differ=0;

                for(size_t i=0; i<filled.pixels.size(); ++i)
                    differ+=filled.pixels[i] != trapezoid.pixels[i];
                CHECK(differ == 0);
            }
        }
    }
}

TEST_CASE("Trapezoid (style 9): one whose edges cross covers what lies between them on both sides")
{
    Primitives prims(1);
    // the top from above the bottom to below it: they cross at x = 30 + 40*10/22
    const Trapezoid t{30.0f, 70.0f, 20.0f, 52.0f, 30.0f, 40.0f};

    for(float scale : scales)
    {
        ScaleGuard guard(scale);
        const Placed placed=place(t, scale);
        // where the edges cross, in device px, and the column it is in
        const double cross=placed.left+(placed.right-placed.left)
            *(placed.bottom0-placed.top0)/((placed.bottom0-placed.top0)-(placed.bottom1-placed.top1));
        const int crossColumn=int(std::floor(cross));

        CAPTURE(scale);

        const Frame frame=drawFrame([&](SlotRenderer &renderer)
            {
                renderer.drawTrapezoid(prims.ids[0], t.x0, t.x1, t.top0, t.top1, t.bottom0, t.bottom1, red, 1);
            });
        const int y1=gweni::test::viewSize().y;
        int wrong=0;
        int left=0;
        int right=0;

        for(int y=0; y<y1; ++y)
        {
            for(int x=placed.left-2; x<placed.right+2; ++x)
            {
                if(x == crossColumn)
                    continue;

                const double c=placed.coverage(x, y);
                const Color pixel=frame.at(x, y);

                if(c == 1.0)
                    (x < crossColumn ? left : right)+=1;
                if(std::abs(pixel.r-255.0*c) > 1.5)
                {
                    if(wrong < 8)
                        MESSAGE("pixel (", x, ",", y, ") is ", int(pixel.r), " for coverage ", c);
                    ++wrong;
                }
            }
        }
        CHECK(wrong == 0);
        // both sides of the crossing have whole pixels between the edges
        CHECK(left > 20);
        CHECK(right > 20);
    }
}

TEST_CASE("Trapezoid (style 9): pixels it leaves uncovered write no depth, and a side past 32 K draws nothing")
{
    Primitives prims(2);
    const Trapezoid t{14.0f, 70.5f, 30.0f, 6.3f, 44.0f, 58.7f};

    for(float scale : scales)
    {
        ScaleGuard guard(scale);
        const Placed placed=place(t, scale);

        CAPTURE(scale);

        SUBCASE("a rect at a lower z drawn after it shows wherever it leaves a pixel uncovered")
        {
            const Frame frame=drawFrame([&](SlotRenderer &renderer)
                {
                    renderer.drawTrapezoid(prims.ids[0], t.x0, t.x1, t.top0, t.top1,
                        t.bottom0, t.bottom1, red, 2);
                    renderer.setDrawColor(green);
                    renderer.drawFilledRect(prims.ids[1], Rect(10, 2, 70, 62), 1);
                });
            int wrong=0;
            int uncovered=0;

            for(int y=int(placed.top1)-1; y<int(placed.bottom1)+2; ++y)
            {
                for(int x=placed.left; x<placed.right; ++x)
                {
                    const double c=placed.coverage(x, y);
                    const Color pixel=frame.at(x, y);

                    if(c == 0.0)
                    {
                        ++uncovered;
                        wrong+=!(pixel == green);
                    }
                    else if(c == 1.0)
                        wrong+=!(pixel == red);
                }
            }
            CHECK(wrong == 0);
            CHECK(uncovered > 100);
        }

        SUBCASE("a side past +/-32767 device px draws nothing")
        {
            const Frame frame=drawFrame([&](SlotRenderer &renderer)
                {
                    renderer.drawTrapezoid(prims.ids[0], t.x0, 40000.0f, t.top0, t.top1,
                        t.bottom0, t.bottom1, red, 2);
                });
            int shown=0;

            for(size_t i=0; i<frame.pixels.size(); i+=4)
                shown+=frame.pixels[i] != 0;
            CHECK(shown == 0);
        }
    }
}

TEST_CASE("Trapezoid (style 9): drawn again it writes nothing, a moved edge its slot only, and hide/show keeps style 9")
{
    Primitives prims(3);
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    ScaleGuard guard(1.5f);
    std::vector<Trapezoid> trapezoids=row();

    trapezoids.resize(3);

    auto draw=[&]()
    {
        return drawFrame([&](SlotRenderer &renderer)
            {
                for(size_t i=0; i<trapezoids.size(); ++i)
                {
                    const Trapezoid &t=trapezoids[i];

                    renderer.drawTrapezoid(prims.ids[i], t.x0, t.x1, t.top0, t.top1,
                        t.bottom0, t.bottom1, red, 1);
                }
            });
    };
    auto styleOf=[&](size_t id)
    {
        const SlotRenderer::MirrorSnapshot mirror=renderer.snapshotMirror();
        const std::vector<uint8_t> &bytes=mirror.primitives.at(id);
        uint16_t style=0;

        std::copy(bytes.begin(), bytes.begin()+sizeof(style), reinterpret_cast<uint8_t *>(&style));
        return style;
    };

    const Frame first=draw();

    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 3);
    CHECK(styleOf(prims.ids[0]) == 9);

    const Frame again=draw();

    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 0);
    CHECK(renderer.getFrameStats().uploadBytes == 0);
    CHECK(again.pixels == first.pixels);

    // the last trapezoid's right top is its own
    trapezoids[2].top1=20.0f;

    const Frame moved=draw();

    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 1);
    CHECK(moved.pixels != first.pixels);

    trapezoids[2].top1=row()[2].top1;
    draw();

    renderer.hidePrimitive(prims.ids[1]);
    CHECK(styleOf(prims.ids[1]) == 0);

    const Frame hidden=drawFrame([](SlotRenderer &) {});

    CHECK(hidden.pixels != first.pixels);

    renderer.showPrimitive(9, prims.ids[1]);
    CHECK(styleOf(prims.ids[1]) == 9);

    const Frame shown=drawFrame([](SlotRenderer &) {});

    CHECK(shown.pixels == first.pixels);
}
