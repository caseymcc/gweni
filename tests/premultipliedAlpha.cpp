/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The target holds premultiplied colour and its coverage, on every backend: the
 *  colour is blended (SRC_ALPHA, ONE_MINUS_SRC_ALPHA) and the alpha (ONE,
 *  ONE_MINUS_SRC_ALPHA), so an app can composite a frame drawn over a transparent
 *  clear over its own scene with premultiplied "over" (rgb + S*(1-A)).
 *
 *  - Over a (0,0,0,0) clear a fill of colour c and alpha a stores (c*a, a) within
 *    1 level, and two translucent fills one over the other the premultiplied
 *    "over" of the two.
 *  - Over an opaque clear every pixel's alpha is 255, under translucent fills,
 *    anti-aliased edges, lines and text alike (SRC_ALPHA as the alpha's factor
 *    stored a*a + a_d*(1-a) there, below 255 wherever a was neither 0 nor 1).
 *  - A frame of fills, rounded corners and borders, lines and grayscale text
 *    drawn over a transparent clear and composited on the CPU over an opaque
 *    colour S is the same frame drawn over a clear of S, within the bound set
 *    beside its measurement. LCD text is left out: its alpha is the mean of its
 *    three coverages, so no single alpha composites it.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::PointF;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

//! Primitive and text ids for one test case, released (and that flushed) when it
//! ends, so the next case's frame starts empty.
struct Ids
{
    Ids(size_t primitives, size_t texts)
        : prims(primitives), texts(texts)
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        for(size_t &id:prims)
            renderer.generatePrimitive(id);
        // slot order is draw order: prims[0] is drawn first
        std::sort(prims.begin(), prims.end());
        for(size_t &id:this->texts)
            renderer.generateTextPrimitive(id);
    }

    ~Ids()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        for(size_t id:prims)
            renderer.releasePrimitive(id);
        for(size_t id:texts)
            renderer.releaseTextPrimitive(id);
        renderer.begin();
        renderer.end();
    }

    std::vector<size_t> prims;
    std::vector<size_t> texts;
};

struct Frame
{
    std::vector<uint8_t> pixels;
    int width=0;

    const uint8_t *at(int x, int y) const
    {
        return &pixels[(static_cast<size_t>(y)*width+x)*4];
    }
};

//! Clears the target to `clear` (its alpha as given, unlike test::clearView),
//! draws one frame with `draw` and reads it back.
template<typename Draw>
Frame drawFrame(const Color &clear, Draw &&draw)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    renderer.clearTarget(clear);
    renderer.begin();
    draw(renderer);
    renderer.end();

    Frame frame;

    frame.width=gweni::test::viewSize().x;
    gweni::test::readView(frame.pixels);
    return frame;
}

//! A translucent fill of `color` over the whole of `rect`.
void fill(SlotRenderer &renderer, size_t id, const Rect &rect, const Color &color, int z)
{
    renderer.setDrawColor(color);
    renderer.drawFilledRect(id, rect, z);
}

//! What the scene of the composite case draws: translucent and opaque fills, a
//! rounded rect with a border (its arcs partly covered), slanted and straight
//! lines, and grayscale text in two colours, some of it over the fills.
void drawScene(SlotRenderer &renderer, const Ids &ids)
{
    const Font &font=gweni::test::openSans();

    fill(renderer, ids.prims[0], Rect(10, 10, 120, 60), Color(200, 60, 20, 255), 1);
    fill(renderer, ids.prims[1], Rect(60, 30, 120, 60), Color(30, 180, 220, 128), 2);
    renderer.drawRoundedRect(ids.prims[2], Rect(20, 92, 150, 60), 18.0f, 3.0f,
        Color(240, 240, 90, 200), Color(90, 30, 160, 140), Color(10, 250, 120, 230), 3);
    renderer.setDrawColor(Color(250, 250, 250, 255));
    renderer.drawLine(ids.prims[3], PointF(200.0f, 20.0f), PointF(300.0f, 75.0f), 1.0f, 4);
    renderer.setDrawColor(Color(255, 120, 0, 160));
    renderer.drawLine(ids.prims[4], PointF(200.0f, 100.0f), PointF(290.0f, 150.0f), 3.5f, 4);
    renderer.drawLine(ids.prims[5], PointF(195.5f, 90.0f), PointF(300.5f, 90.0f), 2.0f, 4);
    renderer.drawRoundedRect(ids.prims[6], Rect(150, 110, 40, 40), 20.0f, 0.0f,
        Color(255, 255, 255, 90), Color(255, 255, 255, 90), Color(0, 0, 0, 0), 5);

    renderer.setDrawColor(Color(255, 255, 255, 255));
    renderer.renderText(ids.texts[0], font, Point(14, 40), 6, "Grayscale text, over a fill");
    renderer.setDrawColor(Color(20, 40, 255, 220));
    renderer.renderText(ids.texts[1], font, Point(310, 120), 6, "Translucent blue text");
}

}//namespace

TEST_CASE("Premultiplied alpha: a fill of colour c and alpha a over a transparent clear stores (c*a, a)")
{
    const int alphas[]={0, 1, 37, 64, 128, 191, 200, 254, 255};
    const int count=sizeof(alphas)/sizeof(alphas[0]);
    const Color colour(200, 100, 50, 255);
    Ids ids(count, 0);

    const Frame frame=drawFrame(Color(0, 0, 0, 0), [&](SlotRenderer &renderer)
        {
            for(int i=0; i<count; ++i)
                fill(renderer, ids.prims[i], Rect(8+i*24, 8, 20, 20),
                    Color(colour.r, colour.g, colour.b, alphas[i]), 1);
        });

    for(int i=0; i<count; ++i)
    {
        const float a=alphas[i]/255.0f;
        const uint8_t *p=frame.at(18+i*24, 18);

        INFO("alpha ", alphas[i], ": stored (", int(p[0]), ",", int(p[1]), ",", int(p[2]), ",",
            int(p[3]), "), expected (", colour.r*a, ",", colour.g*a, ",", colour.b*a, ",", alphas[i], ")");
        CHECK(std::abs(p[3]-alphas[i]) <= 1);
        CHECK(std::fabs(p[0]-colour.r*a) <= 1.0f);
        CHECK(std::fabs(p[1]-colour.g*a) <= 1.0f);
        CHECK(std::fabs(p[2]-colour.b*a) <= 1.0f);
    }

    // and outside them the clear, untouched
    const uint8_t *outside=frame.at(4, 4);

    CHECK((outside[0] == 0 && outside[1] == 0 && outside[2] == 0 && outside[3] == 0));
}

TEST_CASE("Premultiplied alpha: two translucent fills over a transparent clear store their premultiplied over")
{
    struct Layer
    {
        Color under, over;
    };
    const Layer layers[]={
        {Color(200, 100, 50, 128), Color(30, 180, 220, 100)},
        {Color(255, 255, 255, 64), Color(0, 0, 0, 191)},
        {Color(10, 250, 120, 230), Color(250, 20, 140, 40)},
    };
    const int count=sizeof(layers)/sizeof(layers[0]);
    Ids ids(count*2, 0);

    const Frame frame=drawFrame(Color(0, 0, 0, 0), [&](SlotRenderer &renderer)
        {
            for(int i=0; i<count; ++i)
            {
                fill(renderer, ids.prims[i*2], Rect(8+i*40, 8, 30, 30), layers[i].under, 1);
                fill(renderer, ids.prims[i*2+1], Rect(18+i*40, 18, 30, 30), layers[i].over, 2);
            }
        });

    for(int i=0; i<count; ++i)
    {
        const Color &u=layers[i].under;
        const Color &o=layers[i].over;
        const float ua=u.a/255.0f;
        const float oa=o.a/255.0f;
        // premultiplied over: over + under*(1-over.a), colour and alpha alike
        const float expected[4]={
            o.r*oa+u.r*ua*(1.0f-oa),
            o.g*oa+u.g*ua*(1.0f-oa),
            o.b*oa+u.b*ua*(1.0f-oa),
            o.a+u.a*(1.0f-oa),
        };
        const uint8_t *p=frame.at(28+i*40, 28);

        INFO("layers ", i, ": stored (", int(p[0]), ",", int(p[1]), ",", int(p[2]), ",", int(p[3]),
            "), expected (", expected[0], ",", expected[1], ",", expected[2], ",", expected[3], ")");
        // two blends, each rounding once: within 1 level
        for(int c=0; c<4; ++c)
            CHECK(std::fabs(p[c]-expected[c]) <= 1.0f);
    }
}

TEST_CASE("Premultiplied alpha: over an opaque clear every pixel's alpha is 255")
{
    Ids ids(7, 2);
    const Point size=gweni::test::viewSize();

    const Frame frame=drawFrame(Color(40, 50, 60, 255), [&](SlotRenderer &renderer)
        {
            drawScene(renderer, ids);
        });

    int below=0;
    int lowest=255;

    for(int y=0; y<size.y; ++y)
        for(int x=0; x<size.x; ++x)
        {
            const int a=frame.at(x, y)[3];

            below+=(a != 255);
            lowest=std::min(lowest, a);
        }
    INFO(below, " pixels below 255, the lowest ", lowest);
    CHECK(below == 0);
}

TEST_CASE("Premultiplied alpha: a frame over a transparent clear, composited over S, is the frame drawn over S")
{
    // the largest |difference| of a channel, measured over these backgrounds:
    // 1 on llvmpipe (GL) and on lavapipe (Vulkan), the two roundings (the
    // blend's into the target and the composite's) apart by at most one level
    const int bound=1;
    const Color backgrounds[]={
        Color(255, 255, 255, 255),
        Color(0, 0, 0, 255),
        Color(40, 50, 60, 255),
        Color(200, 30, 120, 255),
    };
    Ids ids(7, 2);
    const Point size=gweni::test::viewSize();

    const Frame transparent=drawFrame(Color(0, 0, 0, 0), [&](SlotRenderer &renderer)
        {
            drawScene(renderer, ids);
        });

    // the scene draws something partly covered: otherwise this checks nothing
    int translucent=0;

    for(int y=0; y<size.y; ++y)
        for(int x=0; x<size.x; ++x)
        {
            const int a=transparent.at(x, y)[3];

            translucent+=(a > 0 && a < 255);
        }
    CHECK(translucent > 1000);

    for(const Color &s:backgrounds)
    {
        const Frame opaque=drawFrame(s, [&](SlotRenderer &renderer)
            {
                drawScene(renderer, ids);
            });
        const int sc[3]={s.r, s.g, s.b};
        int worst=0;
        int worstX=0, worstY=0;

        for(int y=0; y<size.y; ++y)
            for(int x=0; x<size.x; ++x)
            {
                const uint8_t *t=transparent.at(x, y);
                const uint8_t *o=opaque.at(x, y);

                for(int c=0; c<3; ++c)
                {
                    const float composite=t[c]+sc[c]*(1.0f-t[3]/255.0f);
                    const int delta=static_cast<int>(std::lround(std::fabs(composite-o[c])));

                    if(delta > worst)
                    {
                        worst=delta;
                        worstX=x;
                        worstY=y;
                    }
                }
            }
        INFO("over (", int(s.r), ",", int(s.g), ",", int(s.b), "): the worst channel is ", worst,
            " levels off, at (", worstX, ",", worstY, ")");
        MESSAGE("composite over (", int(s.r), ",", int(s.g), ",", int(s.b), "): max |delta| ", worst);
        CHECK(worst <= bound);
    }
}

TEST_CASE("LCD text: its alpha is the mean coverage over a transparent clear, and 255 over an opaque one")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const gweni::renderer::TextAA aa=renderer.textAA();
    Font font=gweni::test::openSans();

    font.size=12;
    renderer.setTextAA(gweni::renderer::TextAA::LcdRGB);
    if(!renderer.lcdTextAvailable() || renderer.getTextMode(font) == gweni::renderer::TextMode::Sdf)
    {
        MESSAGE("skipped: no LCD text here (no dual-source blending, or 12 px drawn from the SDF atlas)");
        renderer.setTextAA(aa);
        return;
    }

    Ids ids(0, 1);
    const Point size=gweni::test::viewSize();
    auto draw=[&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(Color(255, 255, 255, 255));
            // glyphs apart, so no pixel has two glyphs' ink: where two overlap, each
            // channel and the alpha are two "over"s of their own, and the mean
            // of the first is not the second
            renderer.renderText(ids.texts[0], font, Point(10, 20), 1, "L  C  D  s  u  b  p  i  x  e  l  W  m  &");
        };

    // white text over (0,0,0,0): each channel holds its own coverage, the alpha
    // their mean (the shader's FragColor.a == Coverage.a, under ONE)
    const Frame transparent=drawFrame(Color(0, 0, 0, 0), draw);
    int inked=0;
    int worst=0;

    for(int y=0; y<size.y; ++y)
        for(int x=0; x<size.x; ++x)
        {
            const uint8_t *p=transparent.at(x, y);
            const float mean=(p[0]+p[1]+p[2])/3.0f;

            if(p[0] || p[1] || p[2] || p[3])
                ++inked;
            worst=std::max(worst, static_cast<int>(std::lround(std::fabs(p[3]-mean))));
        }
    INFO(inked, " pixels inked, alpha at most ", worst, " levels from the mean coverage");
    CHECK(inked > 100);
    // each channel and the alpha rounded once from the same coverages
    CHECK(worst <= 1);

    const Frame opaque=drawFrame(Color(40, 50, 60, 255), draw);
    int below=0;

    for(int y=0; y<size.y; ++y)
        for(int x=0; x<size.x; ++x)
            below+=(opaque.at(x, y)[3] != 255);
    CHECK(below == 0);

    renderer.setTextAA(aa);
}
