/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's SDF rects, drawn offscreen and read back pixel by pixel: the
 *  rounded-rect mask all of styles 4, 6 and 7 share (edge coverage, border,
 *  corners, rects that abut), the bilinear fill (style 6, drawBilinearRect) and
 *  the checker composite (style 7, drawCheckerRect).
 *
 *  Every rect here is 128x128 at (16,16) unless a case says otherwise. A pixel
 *  centre (x+0.5, y+0.5) sits at u=(x-16+0.5)/128 across and v=(y-16+0.5)/128
 *  down, which is what the shader sees through vLocal. The edge anti-aliasing is
 *  a 1 px box filter, so a pixel-aligned straight edge covers its outermost
 *  pixel fully and nothing outside it: only a rounded corner's arc is partly
 *  covered, and the "interior" of a square-cornered rect is the whole rect.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;
using gweni::renderer::OpenGLCore;

const int rectX=16;
const int rectY=16;
const int rectSize=128;
const Rect testRect(rectX, rectY, rectSize, rectSize);

const Color background(0, 0, 255, 255);
const Color white(255, 255, 255, 255);
const Color red(255, 0, 0, 255);
const Color black(0, 0, 0, 255);
const Color green(0, 255, 0, 255);

//! A colour as the shader holds it, 0..1 per channel.
struct Rgb
{
    float r, g, b;
};

Rgb rgb(const Color &c)
{
    return {c.r/255.0f, c.g/255.0f, c.b/255.0f};
}

Rgb mix(const Rgb &a, const Rgb &b, float t)
{
    return {a.r+(b.r-a.r)*t, a.g+(b.g-a.g)*t, a.b+(b.b-a.b)*t};
}

//! The analytic bilinear blend of the four corners at (u,v).
Rgb bilinear(const Color &c00, const Color &c10, const Color &c01, const Color &c11, float u, float v)
{
    return mix(mix(rgb(c00), rgb(c10), u), mix(rgb(c01), rgb(c11), u), v);
}

float pixelU(int x)
{
    return (x-rectX+0.5f)/rectSize;
}

float pixelV(int y)
{
    return (y-rectY+0.5f)/rectSize;
}

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
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const gweni::Point size=gweni::test::viewSize();

    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    renderer.begin();
    draw(renderer);
    renderer.end();

    Frame frame;

    gweni::capture::readFramebufferRgba(size.x, size.y, frame.pixels);
    return frame;
}

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
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        for(size_t id:ids)
            renderer.releasePrimitive(id);
        renderer.begin();
        renderer.end();
    }

    std::vector<size_t> ids;
};

//! True when every channel of `pixel` is within `tolerance` of `expected` (0..1).
bool near(const Color &pixel, const Rgb &expected, float tolerance=2.0f)
{
    return std::fabs(pixel.r-expected.r*255.0f) <= tolerance
        && std::fabs(pixel.g-expected.g*255.0f) <= tolerance
        && std::fabs(pixel.b-expected.b*255.0f) <= tolerance;
}

#define CHECK_PIXEL(frame, x, y, expected)                                          \
    do {                                                                            \
        const Color pixel_=(frame).at((x), (y));                                    \
        const Rgb expected_=(expected);                                             \
        INFO("pixel (", (x), ",", (y), ") is (", int(pixel_.r), ",", int(pixel_.g),   \
            ",", int(pixel_.b), "), expected (", expected_.r*255.0f, ",",           \
            expected_.g*255.0f, ",", expected_.b*255.0f, ")");                      \
        CHECK(near(pixel_, expected_));                                             \
    } while(0)

//! Interior pixels that differ from `expected(x, y)` by more than 2 in a channel.
template<typename Expected>
int interiorMismatches(const Frame &frame, Expected &&expected)
{
    int mismatches=0;

    for(int y=rectY; y<rectY+rectSize; ++y)
        for(int x=rectX; x<rectX+rectSize; ++x)
            if(!near(frame.at(x, y), expected(x, y)))
                ++mismatches;
    return mismatches;
}

//! Pixels of the ring just outside `rect` that are not `background`.
int outsideRingMismatches(const Frame &frame, const Rect &rect)
{
    int mismatches=0;

    for(int x=rect.x-1; x<=rect.x+rect.w; ++x)
    {
        mismatches+=!near(frame.at(x, rect.y-1), rgb(background), 0.0f);
        mismatches+=!near(frame.at(x, rect.y+rect.h), rgb(background), 0.0f);
    }
    for(int y=rect.y; y<rect.y+rect.h; ++y)
    {
        mismatches+=!near(frame.at(rect.x-1, y), rgb(background), 0.0f);
        mismatches+=!near(frame.at(rect.x+rect.w, y), rgb(background), 0.0f);
    }
    return mismatches;
}

//! Pixels on the straight parts of `rect`'s four edges (the outermost ring, less
//! `radius` at each end) that differ from `expected(x, y)`.
template<typename Expected>
int edgeMismatches(const Frame &frame, const Rect &rect, int radius, Expected &&expected)
{
    int mismatches=0;

    for(int x=rect.x+radius; x<rect.x+rect.w-radius; ++x)
    {
        for(int y:{rect.y, rect.y+rect.h-1})
        {
            if(!near(frame.at(x, y), expected(x, y)))
            {
                const Color p=frame.at(x, y);
                MESSAGE("edge pixel (", x, ",", y, ") is (", int(p.r), ",", int(p.g), ",", int(p.b), ")");
                ++mismatches;
            }
        }
    }
    for(int y=rect.y+radius; y<rect.y+rect.h-radius; ++y)
    {
        for(int x:{rect.x, rect.x+rect.w-1})
        {
            if(!near(frame.at(x, y), expected(x, y)))
            {
                const Color p=frame.at(x, y);
                MESSAGE("edge pixel (", x, ",", y, ") is (", int(p.r), ",", int(p.g), ",", int(p.b), ")");
                ++mismatches;
            }
        }
    }
    return mismatches;
}

}//namespace

TEST_CASE("Bilinear fill (style 6): four corners blend bilinearly, with no diagonal crease")
{
    Primitives prims(1);

    const Frame frame=drawFrame([&](OpenGLCore &renderer)
        {
            renderer.drawBilinearRect(prims.ids[0], testRect, 1, white, red, black, black,
                0.0f, 0.0f, black);
        });

    auto expected=[](int x, int y)
    {
        return bilinear(white, red, black, black, pixelU(x), pixelV(y));
    };

    // centre: 0.5*mix(white, red, 0.5)
    CHECK_PIXEL(frame, rectX+rectSize/2, rectY+rectSize/2, (Rgb{128/255.0f, 64/255.0f, 64/255.0f}));

    // both diagonals: two triangles lerping per-vertex colour (style 1) miss the
    // u*v term, worst along the diagonal the quad is not split on
    for(int i=0; i<rectSize; ++i)
    {
        CHECK_PIXEL(frame, rectX+i, rectY+i, expected(rectX+i, rectY+i));
        CHECK_PIXEL(frame, rectX+rectSize-1-i, rectY+i, expected(rectX+rectSize-1-i, rectY+i));
    }

    CHECK(interiorMismatches(frame, expected) == 0);
}

TEST_CASE("Bilinear fill (style 6): rounded and bordered like style 4")
{
    Primitives prims(1);

    auto fill=[](int x, int y)
    {
        return bilinear(white, red, black, black, pixelU(x), pixelV(y));
    };

    SUBCASE("radius 8, border 1")
    {
        const Frame frame=drawFrame([&](OpenGLCore &renderer)
            {
                renderer.drawBilinearRect(prims.ids[0], testRect, 1, white, red, black, black,
                    8.0f, 1.0f, green);
            });

        // the corner pixel lies outside the rounded mask
        CHECK_PIXEL(frame, rectX, rectY, rgb(background));
        CHECK_PIXEL(frame, rectX+rectSize-1, rectY+rectSize-1, rgb(background));

        // the top edge pixel, on the straight part, is fully covered and all border
        // (coverage and border mix both 1.0), and one row in it is all fill
        const int x=rectX+rectSize/2;

        CHECK_PIXEL(frame, x, rectY, rgb(green));
        CHECK_PIXEL(frame, x, rectY+1, fill(x, rectY+1));
        CHECK_PIXEL(frame, x, rectY+rectSize/2, fill(x, rectY+rectSize/2));
        CHECK(edgeMismatches(frame, testRect, 8, [](int, int) { return rgb(green); }) == 0);
        CHECK(outsideRingMismatches(frame, testRect) == 0);
    }

    SUBCASE("radius 8, border 3: the pixel one row inside the edge is the border colour")
    {
        const Frame frame=drawFrame([&](OpenGLCore &renderer)
            {
                renderer.drawBilinearRect(prims.ids[0], testRect, 1, white, red, black, black,
                    8.0f, 3.0f, green);
            });

        CHECK_PIXEL(frame, rectX, rectY, rgb(background));
        CHECK_PIXEL(frame, rectX+rectSize/2, rectY+1, rgb(green));
        CHECK_PIXEL(frame, rectX+1, rectY+rectSize/2, rgb(green));
        CHECK_PIXEL(frame, rectX+rectSize/2, rectY+rectSize/2, fill(rectX+rectSize/2, rectY+rectSize/2));

        // rows/columns 0..2 in from each straight edge are the border, the 4th the fill
        const int mid=rectSize/2;

        for(int i=0; i<3; ++i)
        {
            CHECK_PIXEL(frame, rectX+mid, rectY+i, rgb(green));
            CHECK_PIXEL(frame, rectX+mid, rectY+rectSize-1-i, rgb(green));
            CHECK_PIXEL(frame, rectX+i, rectY+mid, rgb(green));
            CHECK_PIXEL(frame, rectX+rectSize-1-i, rectY+mid, rgb(green));
        }
        CHECK_PIXEL(frame, rectX+mid, rectY+3, fill(rectX+mid, rectY+3));
        CHECK_PIXEL(frame, rectX+mid, rectY+rectSize-4, fill(rectX+mid, rectY+rectSize-4));
        CHECK_PIXEL(frame, rectX+3, rectY+mid, fill(rectX+3, rectY+mid));
        CHECK_PIXEL(frame, rectX+rectSize-4, rectY+mid, fill(rectX+rectSize-4, rectY+mid));
    }
}

TEST_CASE("Checker composite (style 7): translucent fill over a checker, drawn opaque")
{
    const Color light(204, 204, 204, 255);
    const Color dark(102, 102, 102, 255);

    // drawn first and behind (z 1), so a translucent checker would blend with it
    Primitives prims(2);
    const size_t behindId=prims.ids[0];
    const size_t checkerId=prims.ids[1];

    auto drawWithFill=[&](const Color &fill)
    {
        return drawFrame([&](OpenGLCore &renderer)
            {
                renderer.setDrawColor(green);
                renderer.drawFilledRect(behindId, testRect, 1);
                renderer.drawCheckerRect(checkerId, testRect, 2, fill, light, dark,
                    0.0f, 0.0f, black);
            });
    };

    SUBCASE("fill alpha 128")
    {
        const Frame frame=drawWithFill(Color(255, 0, 0, 128));
        const float alpha=128/255.0f;
        const Rgb overLight=mix(rgb(light), rgb(red), alpha);
        const Rgb overDark=mix(rgb(dark), rgb(red), alpha);

        // 4 px cells from the rect's top-left: (2,2) is in a light cell, (6,2) in a dark one
        const Color a=frame.at(rectX+2, rectY+2);
        const Color b=frame.at(rectX+6, rectY+2);

        CHECK_FALSE(a == b);
        CHECK_PIXEL(frame, rectX+2, rectY+2, overLight);
        CHECK_PIXEL(frame, rectX+6, rectY+2, overDark);
        CHECK_PIXEL(frame, rectX+2, rectY+6, overDark);
        CHECK_PIXEL(frame, rectX+6, rectY+6, overLight);

        // the whole interior: the checker pattern, and no green from behind
        CHECK(interiorMismatches(frame, [&](int x, int y)
            {
                const bool odd=(((x-rectX)/4+(y-rectY)/4)&1) != 0;

                return odd?overDark:overLight;
            }) == 0);
    }

    SUBCASE("fill alpha 255 is the fill alone")
    {
        const Frame frame=drawWithFill(red);

        int notRed=0;

        for(int y=rectY; y<rectY+rectSize; ++y)
            for(int x=rectX; x<rectX+rectSize; ++x)
                if(!(frame.at(x, y) == red))
                    ++notRed;
        CHECK(notRed == 0);
        CHECK(outsideRingMismatches(frame, testRect) == 0);
    }
}

TEST_CASE("SDF rects (styles 4, 6, 7): the straight edges are fully covered, nothing outside")
{
    Primitives prims(1);
    const int radii[]={0, 8};

    SUBCASE("style 4, flat")
    {
        for(int radius:radii)
        {
            CAPTURE(radius);

            const Frame frame=drawFrame([&](OpenGLCore &renderer)
                {
                    renderer.drawRoundedRect(prims.ids[0], testRect, float(radius), 0.0f, red, red,
                        black, 1);
                });

            CHECK(edgeMismatches(frame, testRect, radius, [](int, int) { return rgb(red); }) == 0);
            CHECK(outsideRingMismatches(frame, testRect) == 0);
            if(radius == 0)
                CHECK(interiorMismatches(frame, [](int, int) { return rgb(red); }) == 0);
        }
    }

    SUBCASE("style 4, gradient: the top colour on the first row, the bottom on the last")
    {
        for(int radius:radii)
        {
            CAPTURE(radius);

            const Frame frame=drawFrame([&](OpenGLCore &renderer)
                {
                    renderer.drawRoundedRect(prims.ids[0], testRect, float(radius), 0.0f, white,
                        black, black, 1);
                });

            // t at a pixel centre is pixelV(y), within half a pixel of either end
            auto expected=[](int, int y) { return mix(rgb(white), rgb(black), pixelV(y)); };

            CHECK(edgeMismatches(frame, testRect, radius, expected) == 0);
            CHECK_PIXEL(frame, rectX+rectSize/2, rectY, rgb(white));
            CHECK_PIXEL(frame, rectX+rectSize/2, rectY+rectSize-1, rgb(black));
            CHECK(outsideRingMismatches(frame, testRect) == 0);
        }
    }

    SUBCASE("style 6")
    {
        for(int radius:radii)
        {
            CAPTURE(radius);

            const Frame frame=drawFrame([&](OpenGLCore &renderer)
                {
                    renderer.drawBilinearRect(prims.ids[0], testRect, 1, white, red, black, black,
                        float(radius), 0.0f, black);
                });

            auto expected=[](int x, int y)
            {
                return bilinear(white, red, black, black, pixelU(x), pixelV(y));
            };

            CHECK(edgeMismatches(frame, testRect, radius, expected) == 0);
            CHECK(outsideRingMismatches(frame, testRect) == 0);
        }
    }

    SUBCASE("style 7")
    {
        const Color light(204, 204, 204, 255);
        const Color dark(102, 102, 102, 255);

        for(int radius:radii)
        {
            CAPTURE(radius);

            const Frame frame=drawFrame([&](OpenGLCore &renderer)
                {
                    renderer.drawCheckerRect(prims.ids[0], testRect, 1, Color(255, 0, 0, 128),
                        light, dark, float(radius), 0.0f, black);
                });

            auto expected=[&](int x, int y)
            {
                const bool odd=(((x-rectX)/4+(y-rectY)/4)&1) != 0;

                return mix(rgb(odd?dark:light), rgb(red), 128/255.0f);
            };

            CHECK(edgeMismatches(frame, testRect, radius, expected) == 0);
            CHECK(outsideRingMismatches(frame, testRect) == 0);
        }
    }
}

TEST_CASE("Rounded rect (style 4): a corner arc stays anti-aliased")
{
    Primitives prims(1);

    const Frame frame=drawFrame([&](OpenGLCore &renderer)
        {
            renderer.drawRoundedRect(prims.ids[0], testRect, 8.0f, 0.0f, red, red, black, 1);
        });

    // red over blue: the red channel is the coverage. The corner pixel is outside the
    // arc, the pixel on the diagonal whose centre is 7.8 px from the arc's centre
    // (8,8) is cut by it (d=-0.2, about 0.7), and the one inside that is covered.
    for(int corner=0; corner<4; ++corner)
    {
        const bool right=(corner&1) != 0;
        const bool bottom=(corner&2) != 0;
        auto at=[&](int i)
        {
            return frame.at(right?rectX+rectSize-1-i:rectX+i, bottom?rectY+rectSize-1-i:rectY+i);
        };

        CAPTURE(corner);
        CHECK(at(0) == background);
        CHECK(at(2).r > 10);
        CHECK(at(2).r < 245);
        CHECK(at(3).r == 255);
        CHECK(at(3).b == 0);
    }
}

TEST_CASE("Rounded rects (style 4) that abut meet without a seam")
{
    Primitives prims(2);
    const int half=rectSize/2;

    SUBCASE("side by side")
    {
        const Frame frame=drawFrame([&](OpenGLCore &renderer)
            {
                renderer.drawRoundedRect(prims.ids[0], Rect(rectX, rectY, half, rectSize), 0.0f, 0.0f,
                    red, red, black, 1);
                renderer.drawRoundedRect(prims.ids[1], Rect(rectX+half, rectY, half, rectSize), 0.0f,
                    0.0f, red, red, black, 1);
            });

        CHECK(interiorMismatches(frame, [](int, int) { return rgb(red); }) == 0);
        CHECK(outsideRingMismatches(frame, testRect) == 0);
        for(int y=rectY; y<rectY+rectSize; y+=17)
        {
            CHECK_PIXEL(frame, rectX+half-1, y, rgb(red));
            CHECK_PIXEL(frame, rectX+half, y, rgb(red));
        }
    }

    SUBCASE("one above the other")
    {
        const Frame frame=drawFrame([&](OpenGLCore &renderer)
            {
                renderer.drawRoundedRect(prims.ids[0], Rect(rectX, rectY, rectSize, half), 0.0f, 0.0f,
                    red, red, black, 1);
                renderer.drawRoundedRect(prims.ids[1], Rect(rectX, rectY+half, rectSize, half), 0.0f,
                    0.0f, red, red, black, 1);
            });

        CHECK(interiorMismatches(frame, [](int, int) { return rgb(red); }) == 0);
        CHECK(outsideRingMismatches(frame, testRect) == 0);
        for(int x=rectX; x<rectX+rectSize; x+=17)
        {
            CHECK_PIXEL(frame, x, rectY+half-1, rgb(red));
            CHECK_PIXEL(frame, x, rectY+half, rgb(red));
        }
    }
}

TEST_CASE("Rounded rect (style 4) border: N px border is the outer N rows, then the fill")
{
    Primitives prims(1);

    for(int border:{1, 2, 4})
    {
        CAPTURE(border);

        const Frame frame=drawFrame([&](OpenGLCore &renderer)
            {
                renderer.drawRoundedRect(prims.ids[0], testRect, 6.0f, float(border), red, red, green, 1);
            });

        const int mid=rectSize/2;

        for(int i=0; i<border; ++i)
        {
            CHECK_PIXEL(frame, rectX+mid, rectY+i, rgb(green));
            CHECK_PIXEL(frame, rectX+mid, rectY+rectSize-1-i, rgb(green));
            CHECK_PIXEL(frame, rectX+i, rectY+mid, rgb(green));
            CHECK_PIXEL(frame, rectX+rectSize-1-i, rectY+mid, rgb(green));
        }
        CHECK_PIXEL(frame, rectX+mid, rectY+border, rgb(red));
        CHECK_PIXEL(frame, rectX+mid, rectY+rectSize-1-border, rgb(red));
        CHECK_PIXEL(frame, rectX+border, rectY+mid, rgb(red));
        CHECK_PIXEL(frame, rectX+rectSize-1-border, rectY+mid, rgb(red));
        CHECK(outsideRingMismatches(frame, testRect) == 0);
    }
}
