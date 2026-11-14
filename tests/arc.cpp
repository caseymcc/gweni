/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's arc (drawArc: style 5, icon kind 9), drawn offscreen and read
 *  back pixel by pixel on every backend:
 *
 *    coverage   every pixel of the quad and a ring around it is the arc's exact
 *               distance (the ring's inside its sweep, the nearer round cap's
 *               past it) through the 1 px box filter, worked out here on the
 *               CPU, within 2 levels: the stroke's middle fully covered, the
 *               circle's centre and the far side of a part sweep not at all, a
 *               cap's round end partly; at sweeps of 0.25, 0.5, 0.75 and 1 turn
 *               (the whole ring, no cap), each from a start in each quadrant,
 *               in a square and an odd, non-square rect.
 *    half turn  sweeps of 32756..32767/65535, from 16 starts, are the model's
 *               too: just under half a turn the end directions nearly cancel.
 *    start      a quarter-turn arc from a start in each quadrant covers the
 *               middle of its own quadrant's stroke and none of the other
 *               three's: the start is from the top, clockwise.
 *    thickness  the stroke across the ring's right side is max(1, snap(t))
 *               device px of pixels at least half covered, at scales 1 and 2.
 *    depth      a pixel the arc leaves uncovered (its centre, past its caps)
 *               writes no depth: a rect at a lower z drawn after it shows there,
 *               and is rejected where the stroke covers it fully.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <cmath>
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

const double Tau=6.283185307179586;

//! One frame read back, top-down RGBA8.
struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }

    //! The arc's coverage at (x,y), 0..255: red over blue.
    int coverage(int x, int y) const { return at(x, y).r; }
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

//! Primitive ids for one case, released (and that flushed) when it ends.
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

//! Puts the scale back at 1.
struct ScaleGuard
{
    explicit ScaleGuard(float scale) { gweni::test::gpuRenderer().setScale(scale); }
    ~ScaleGuard() { gweni::test::gpuRenderer().setScale(1.0f); }
};

//! The arc as the shader draws it, in device px: its quad, stroke and angles
//! as the slot keeps them.
struct ArcModel
{
    Rect rect;          //!< device px
    double stroke;
    double start;       //!< turns
    double sweep;       //!< turns; 1 the whole ring

    ArcModel(const Rect &logical, float scale, float startTurns, float sweepTurns, float thickness)
    {
        const int x0=SlotRenderer::snap(float(logical.x), scale);
        const int y0=SlotRenderer::snap(float(logical.y), scale);
        const int x1=SlotRenderer::snap(float(logical.x+logical.w), scale);
        const int y1=SlotRenderer::snap(float(logical.y+logical.h), scale);

        rect=Rect(x0, y0, x1-x0, y1-y0);
        stroke=std::max(1, SlotRenderer::snap(thickness, scale));
        start=gweni::renderer::arcStartBits(startTurns)/65536.0;
        const uint16_t sweepBits=gweni::renderer::arcSweepBits(sweepTurns);
        sweep=sweepBits == 0xffff?1.0:sweepBits/65535.0;
    }

    double outer() const { return 0.5*std::min(rect.w, rect.h); }
    double width() const { return std::min(stroke, outer()); }
    double middle() const { return outer()-0.5*width(); }
    double centreX() const { return rect.x+0.5*rect.w; }
    double centreY() const { return rect.y+0.5*rect.h; }

    //! The point at `turns` on the stroke's middle circle, device px.
    void pointAt(double turns, double radius, double &x, double &y) const
    {
        x=centreX()+radius*std::sin(turns*Tau);
        y=centreY()-radius*std::cos(turns*Tau);
    }

    //! The arc's signed distance at the pixel (x,y)'s centre.
    double distance(int x, int y) const
    {
        const double qx=x+0.5-centreX();
        const double qy=y+0.5-centreY();
        const double r=middle();
        double d=std::abs(std::hypot(qx, qy)-r)-0.5*width();

        if(sweep < 1.0)
        {
            double t=0.0;

            if(qx != 0.0 || qy != 0.0)
            {
                t=std::atan2(qx, -qy)/Tau-start;
                t-=std::floor(t);
            }
            if(t > sweep)
            {
                const double a0=start*Tau;
                const double a1=(start+sweep)*Tau;
                const double d0=std::hypot(qx-r*std::sin(a0), qy+r*std::cos(a0));
                const double d1=std::hypot(qx-r*std::sin(a1), qy+r*std::cos(a1));

                d=std::min(d0, d1)-0.5*width();
            }
        }
        return d;
    }

    //! The pixel's coverage, 0..255, the box filter of distance().
    double coverage(int x, int y) const
    {
        return std::clamp(0.5-distance(x, y), 0.0, 1.0)*255.0;
    }
};

//! The pixels of the model's quad and 3 px round it whose coverage is more than
//! 2 levels off the model's.
int coverageMismatches(const Frame &frame, const ArcModel &arc)
{
    int wrong=0;

    for(int y=arc.rect.y-3; y<arc.rect.y+arc.rect.h+3; ++y)
    {
        for(int x=arc.rect.x-3; x<arc.rect.x+arc.rect.w+3; ++x)
        {
            const double expected=arc.coverage(x, y);

            if(std::abs(frame.coverage(x, y)-expected) > 2.0)
            {
                if(wrong < 5)
                    MESSAGE("pixel (", x, ",", y, ") coverage ", frame.coverage(x, y), ", expected ", expected);
                ++wrong;
            }
        }
    }
    return wrong;
}

}//namespace

TEST_CASE("Arcs: every pixel is the arc's distance through the box filter, at each sweep from each quadrant")
{
    Primitives prims(1);
    const Rect rects[]={Rect(30, 20, 60, 60), Rect(150, 25, 47, 53)};
    const float sweeps[]={0.25f, 0.5f, 0.75f, 1.0f};
    const float starts[]={0.1f, 0.35f, 0.6f, 0.85f};

    for(const Rect &logical:rects)
    {
        for(float sweep:sweeps)
        {
            for(float start:starts)
            {
                const ArcModel arc(logical, 1.0f, start, sweep, 6.0f);

                CAPTURE(logical.x);
                CAPTURE(sweep);
                CAPTURE(start);

                const Frame frame=drawFrame([&](SlotRenderer &renderer)
                    {
                        renderer.drawArc(prims.ids[0], logical, start, sweep, 6.0f, red, 1);
                    });

                CHECK(coverageMismatches(frame, arc) == 0);

                // the stroke's middle, halfway along the sweep, fully covered
                double x, y;

                arc.pointAt(arc.start+0.5*arc.sweep, arc.middle(), x, y);
                CHECK(frame.coverage(int(std::floor(x)), int(std::floor(y))) == 255);
                // the circle's centre, inside the ring, not at all
                CHECK(frame.coverage(int(std::floor(arc.centreX())), int(std::floor(arc.centreY()))) == 0);
                if(sweep < 1.0f)
                {
                    // halfway round the part it does not sweep, nothing
                    arc.pointAt(arc.start+arc.sweep+0.5*(1.0-arc.sweep), arc.middle(), x, y);
                    CHECK(frame.coverage(int(std::floor(x)), int(std::floor(y))) == 0);

                    // 1.5 px past the start cap's round end, back along the
                    // tangent from the start point, nothing
                    double sx, sy;

                    arc.pointAt(arc.start, arc.middle(), sx, sy);
                    const double tx=std::cos(arc.start*Tau);
                    const double ty=std::sin(arc.start*Tau);
                    const double back=0.5*arc.width();

                    CHECK(frame.coverage(int(std::floor(sx-tx*(back+1.5))), int(std::floor(sy-ty*(back+1.5)))) == 0);
                }
            }
        }
    }
}

TEST_CASE("Arcs: a sweep just under half a turn draws its whole sweep and both caps, from any start")
{
    // Just under half a turn the two end directions are nearly opposite and
    // int16-rounded, so their sum is a few units long and points anywhere: a
    // test on it cut a gap into the start cap (sweep bits 32766 and 32767, up
    // to 54 pixels 255 levels off).
    Primitives prims(1);
    const Rect logical(40, 30, 48, 48);

    for(int bits=32756; bits<=32767; ++bits)
    {
        for(int i=0; i<16; ++i)
        {
            const float start=(i+0.17f)/16.0f;
            const float sweep=float(bits/65535.0);
            const ArcModel arc(logical, 1.0f, start, sweep, 5.0f);

            REQUIRE(gweni::renderer::arcSweepBits(sweep) == bits);
            CAPTURE(bits);
            CAPTURE(start);

            const Frame frame=drawFrame([&](SlotRenderer &renderer)
                {
                    renderer.drawArc(prims.ids[0], logical, start, sweep, 5.0f, red, 1);
                });

            CHECK(coverageMismatches(frame, arc) == 0);
        }
    }
}

TEST_CASE("Arcs: the start is from the top, clockwise: a quarter turn from each quadrant covers that quadrant alone")
{
    Primitives prims(1);
    const Rect logical(40, 20, 100, 100);

    for(int quadrant=0; quadrant<4; ++quadrant)
    {
        CAPTURE(quadrant);

        const ArcModel arc(logical, 1.0f, quadrant*0.25f, 0.25f, 8.0f);
        const Frame frame=drawFrame([&](SlotRenderer &renderer)
            {
                renderer.drawArc(prims.ids[0], logical, quadrant*0.25f, 0.25f, 8.0f, red, 1);
            });

        for(int other=0; other<4; ++other)
        {
            double x, y;

            // the middle of quadrant `other`'s stroke: 1/8 turn into it
            arc.pointAt(other*0.25+0.125, arc.middle(), x, y);
            CAPTURE(other);
            CHECK(frame.coverage(int(std::floor(x)), int(std::floor(y))) == (other == quadrant?255:0));
        }
    }
}

TEST_CASE("Arcs: the stroke is max(1, snap(thickness)) device px across the ring, at scales 1 and 2")
{
    Primitives prims(1);
    const Rect logical(20, 10, 60, 60);
    const float thicknesses[]={0.4f, 1.0f, 3.0f, 5.0f};

    for(float scale:{1.0f, 2.0f})
    {
        ScaleGuard guard(scale);

        for(float thickness:thicknesses)
        {
            CAPTURE(scale);
            CAPTURE(thickness);

            const ArcModel arc(logical, scale, 0.0f, 1.0f, thickness);
            const Frame frame=drawFrame([&](SlotRenderer &renderer)
                {
                    renderer.drawArc(prims.ids[0], logical, 0.0f, 1.0f, thickness, red, 1);
                });

            // the row through the centre (a whole px from it in a square of even
            // side, the row below the middle), from the centre to the right edge
            const int row=int(std::floor(arc.centreY()));
            int covered=0;

            for(int x=int(std::floor(arc.centreX())); x<arc.rect.x+arc.rect.w+2; ++x)
            {
                if(frame.coverage(x, row) >= 128)
                    ++covered;
            }
            CHECK(covered == std::max(1, SlotRenderer::snap(thickness, scale)));
            CHECK(coverageMismatches(frame, arc) == 0);
        }
    }
}

TEST_CASE("Arcs: a pixel the arc leaves uncovered writes no depth: a rect behind it drawn after it shows there")
{
    // the arc's slot comes first, so it is drawn before the rect at a lower z
    Primitives prims(2);
    const Rect logical(30, 20, 80, 80);
    const ArcModel arc(logical, 1.0f, 0.0f, 0.5f, 10.0f);

    const Frame frame=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.drawArc(prims.ids[0], logical, 0.0f, 0.5f, 10.0f, red, 2);
            renderer.drawRoundedRect(prims.ids[1], logical, 0.0f, 0.0f, green, green, green, 1);
        });

    int wrong=0;

    for(int y=arc.rect.y; y<arc.rect.y+arc.rect.h; ++y)
    {
        for(int x=arc.rect.x; x<arc.rect.x+arc.rect.w; ++x)
        {
            const double expected=arc.coverage(x, y);
            const Color pixel=frame.at(x, y);

            // uncovered: the rect's green; covered: the arc's red, the rect rejected
            if(expected == 0.0 && (pixel.g < 253 || pixel.r > 2))
                ++wrong;
            else if(expected == 255.0 && (pixel.r < 253 || pixel.g > 2))
                ++wrong;
        }
    }
    CHECK(wrong == 0);
}
