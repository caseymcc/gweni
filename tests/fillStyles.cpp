/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's bilinear fill (style 6, drawBilinearRect) and checker composite
 *  (style 7, drawCheckerRect), drawn offscreen and read back pixel by pixel.
 *
 *  Every rect here is 128x128 at (16,16). A pixel centre (x+0.5, y+0.5) sits at
 *  u=(x-16+0.5)/128 across and v=(y-16+0.5)/128 down, which is what the shader
 *  sees through vLocal. The SDF edge anti-aliasing part-covers the outermost
 *  pixel ring of any of these rects, and at the corners, where fwidth(d) is 2
 *  across the diagonal, the pixel inside it too, so "interior" below starts
 *  `inset` pixels in.
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
const int inset=2;

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

    for(int y=rectY+inset; y<rectY+rectSize-inset; ++y)
        for(int x=rectX+inset; x<rectX+rectSize-inset; ++x)
            if(!near(frame.at(x, y), expected(x, y)))
                ++mismatches;
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
    for(int i=inset; i<rectSize-inset; ++i)
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

        // the top edge pixel, on the straight part: d=-0.5 and fwidth(d)=1 there, so
        // the shader's coverage and border mix are both 1-smoothstep(-1,1,-0.5)=0.84375
        const int x=rectX+rectSize/2;
        const float k=0.84375f;
        const Rgb onBorder=mix(rgb(background), mix(fill(x, rectY), rgb(green), k), k);

        CHECK_PIXEL(frame, x, rectY, onBorder);

        // one row in, the 1 px border has faded back to the fill
        CHECK_PIXEL(frame, x, rectY+1, mix(fill(x, rectY+1), rgb(green), 1.0f-k));
        CHECK_PIXEL(frame, x, rectY+rectSize/2, fill(x, rectY+rectSize/2));
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

        for(int y=rectY+inset; y<rectY+rectSize-inset; ++y)
            for(int x=rectX+inset; x<rectX+rectSize-inset; ++x)
                if(!(frame.at(x, y) == red))
                    ++notRed;
        CHECK(notRed == 0);
    }
}
