/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A GradientEditor's span drawn as its bar draws it (drawBilinearRect with
 *  the left corners one stop's colour and the right ones the next's), read
 *  back pixel by pixel and held to gweni::Gradient::evaluate(t) at each pixel
 *  centre, t = (x - left + 0.5)/width: what the app samples is what the bar
 *  shows. Opaque stops are compared with evaluate() directly; translucent
 *  ones over an opaque black clear with evaluate()'s colour composited over
 *  it (straight alpha: c*a/255). Every channel of every pixel of the span must
 *  be within the bound measured beside it; the worst is printed.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/gradient.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Gradient;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

// Measured: every channel of every pixel exact for the opaque span on
// llvmpipe (GL), lavapipe (Vulkan) and the browser's SwiftShader (WebGL 2,
// web_unittests); the translucent one exact on llvmpipe and lavapipe and 1
// level off at most on SwiftShader, whose blend rounds c*a/255 otherwise.
constexpr int MaxOpaqueDelta=0;
constexpr int MaxTranslucentDelta=1;

const Rect span(16, 16, 200, 8);

int channelDelta(const Color &a, const Color &b)
{
    return std::max({std::abs(int(a.r)-int(b.r)), std::abs(int(a.g)-int(b.g)), std::abs(int(a.b)-int(b.b))});
}

//! The worst channel difference over the span's pixels between what is drawn
//! and \p expected(t) at each pixel centre.
template<typename Expected>
int worstDelta(const Color &left, const Color &right, Expected &&expected)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    size_t id;

    renderer.generatePrimitive(id);
    gweni::test::clearView(Color(0, 0, 0, 255));
    renderer.begin();
    renderer.drawBilinearRect(id, span, 1, left, right, left, right, 0.0f, 0.0f, left);
    renderer.end();

    std::vector<uint8_t> pixels;

    gweni::test::readView(pixels);

    const int width=gweni::test::viewSize().x;
    int worst=0;

    for(int y=span.y; y < span.y+span.h; ++y)
    {
        for(int x=span.x; x < span.x+span.w; ++x)
        {
            const uint8_t *p=&pixels[(size_t(y)*width+x)*4];
            const float t=(float(x-span.x)+0.5f)/float(span.w);

            worst=std::max(worst, channelDelta(Color(p[0], p[1], p[2], 255), expected(t)));
        }
    }

    renderer.releasePrimitive(id);
    renderer.begin();
    renderer.end();
    return worst;
}

}//namespace

TEST_CASE("Gradient span: the pixels are evaluate(t) at each pixel centre")
{
    SUBCASE("opaque stops")
    {
        const Color a(10, 200, 40, 255), b(250, 30, 181, 255);
        const Gradient gradient({{0.0f, a}, {1.0f, b}});
        const int worst=worstDelta(a, b, [&](float t) { return gradient.evaluate(t); });

        MESSAGE("opaque span: worst channel difference ", worst);
        CHECK(worst <= MaxOpaqueDelta);
    }

    SUBCASE("translucent stops over black")
    {
        const Color a(255, 120, 0, 255), b(0, 60, 255, 40);
        const Gradient gradient({{0.0f, a}, {1.0f, b}});
        const int worst=worstDelta(a, b, [&](float t)
            {
                const Color c=gradient.evaluate(t);
                auto over=[&](unsigned char v) { return (unsigned char)((v*c.a+127)/255); };

                return Color(over(c.r), over(c.g), over(c.b), 255);
            });

        MESSAGE("translucent span: worst channel difference ", worst);
        CHECK(worst <= MaxTranslucentDelta);
    }
}
