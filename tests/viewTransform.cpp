/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer::setViewTransform, the hook tools::DepthView draws its camera
 *  through, on every backend, drawn offscreen and read back. Setting the identity
 *  and clearing it again draw the front view byte for byte, and neither frame
 *  writes a slot or uploads a byte (the transform is in the projection, not in
 *  the mirrors). A whole-pixel translation moves every pixel by exactly that
 *  much, text and clipping included (the clip boxes are in the vertexes' own
 *  space). A 180-degree turn about the vertical axis draws the scene from behind:
 *  mirrored, nothing culled (every triangle is turned over), and the layer that
 *  was farthest in front. Under a perspective camera (w not 1), a layer at the
 *  camera's reference depth draws as in the front view, a line included (the
 *  text, whose glyphs sit a fraction of a layer nearer, only roughly), and a
 *  nearer one larger about the centre and in front of it: on Vulkan that needs
 *  CLIP_SPACE_TO_API to map z to [0, w], not to [0, 1] as for w = 1, which would
 *  put the nearer layer past the far plane.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::PointF;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 64, 255);
const Color red(255, 0, 0, 255);
const Color green(0, 255, 0, 255);
const Color white(255, 255, 255, 255);

const float identity[16]={ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }

    bool is(int x, int y, const Color &c) const
    {
        const Color p=at(x, y);

        return (p.r == c.r) && (p.g == c.g) && (p.b == c.b);
    }
};

Rect wholeView()
{
    return Rect(Point(0, 0), gweni::test::viewSize());
}

struct Guard
{
    ~Guard()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.clearViewTransform();
        renderer.setClipRegion(wholeView());
    }
};

//! The case's primitive and text ids, released (and that flushed) when it ends
struct Ids
{
    Ids(size_t primitives, size_t texts)
        : prims(primitives), texts(texts)
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        for(size_t &id:prims)
            renderer.generatePrimitive(id);
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

//! Every style the front view has, at z 1024: a fill, an SDF rounded rect with a
//! border, a polyline and a text run, the run clipped to its left half
void drawScene(SlotRenderer &renderer, Ids &ids, int z=1024)
{
    renderer.setClipRegion(wholeView());
    renderer.setDrawColor(red);
    renderer.drawFilledRect(ids.prims[0], Rect(20, 20, 60, 40), z);
    renderer.drawRoundedRect(ids.prims[1], Rect(100, 20, 80, 50), 8.0f, 2.0f,
        Color(200, 120, 40, 255), Color(120, 60, 20, 255), white, z);

    const PointF a(200.0f, 100.0f), b(240.0f, 40.0f), c(280.0f, 110.0f);

    renderer.setDrawColor(green);
    renderer.drawLine(ids.prims[2], a, b, 2.0f, z, nullptr, &c);
    renderer.drawLine(ids.prims[3], b, c, 2.0f, z, &a, nullptr);

    renderer.setClipRegion(Rect(300, 0, 60, 160));
    renderer.setDrawColor(white);
    renderer.renderText(ids.texts[0], gweni::test::openSans(), Point(300, 60), z, "Depth view");
    renderer.setClipRegion(wholeView());
}

//! Pixels that differ between two frames, but in columns [skip0, skip1)
int differing(const Frame &a, const Frame &b, int skip0=0, int skip1=0)
{
    const Point size=gweni::test::viewSize();
    int count=0;

    for(int y=0; y<size.y; ++y)
        for(int x=0; x<size.x; ++x)
            if((x < skip0 || x >= skip1) && !b.is(x, y, a.at(x, y)))
                ++count;
    return count;
}

}//namespace

TEST_CASE("View transform: the identity and no transform draw the front view, writing and uploading nothing")
{
    Guard guard;
    Ids ids(4, 1);
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    CHECK_FALSE(renderer.hasViewTransform());

    const Frame front=drawFrame([&](SlotRenderer &r) { drawScene(r, ids); });
    // settled: the scene drawn again writes nothing
    const Frame settled=drawFrame([&](SlotRenderer &r) { drawScene(r, ids); });

    CHECK(renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(differing(front, settled) == 0);

    renderer.setViewTransform(identity);
    CHECK(renderer.hasViewTransform());

    const Frame withIdentity=drawFrame([&](SlotRenderer &r) { drawScene(r, ids); });

    CHECK(renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(renderer.frameStats().textSlotsWritten == 0);
    CHECK(renderer.frameStats().uploadBytes == 0);
    CHECK(differing(front, withIdentity) == 0);

    renderer.clearViewTransform();
    CHECK_FALSE(renderer.hasViewTransform());

    const Frame cleared=drawFrame([&](SlotRenderer &r) { drawScene(r, ids); });

    CHECK(renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(renderer.frameStats().textSlotsWritten == 0);
    CHECK(renderer.frameStats().uploadBytes == 0);
    CHECK(differing(front, cleared) == 0);
}

TEST_CASE("View transform: a whole-pixel translation moves every pixel by exactly that much")
{
    Guard guard;
    Ids ids(4, 1);
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Point size=gweni::test::viewSize();
    const int dx=10, dy=5;

    const Frame front=drawFrame([&](SlotRenderer &r) { drawScene(r, ids); });
    const float translate[16]={ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, (float)dx, (float)dy, 0, 1 };

    renderer.setViewTransform(translate);

    const Frame moved=drawFrame([&](SlotRenderer &r) { drawScene(r, ids); });

    CHECK(renderer.frameStats().uploadBytes == 0);

    int differ=0;

    for(int y=0; y+dy<size.y; ++y)
        for(int x=0; x+dx<size.x; ++x)
        {
            const Color c=front.at(x, y);

            if(!moved.is(x+dx, y+dy, c))
                ++differ;
        }
    CHECK(differ == 0);
    // something was drawn, and the band uncovered on the left is the background
    CHECK(moved.is(20+dx+5, 20+dy+5, red));
    CHECK(moved.is(dx-1, 30, background));
}

TEST_CASE("View transform: seen from behind, mirrored, nothing culled and the far layer in front")
{
    Guard guard;
    Ids ids(2, 0);
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Point size=gweni::test::viewSize();

    // red at z 100, green at z 200 inside it: green is in front
    auto draw=[&](SlotRenderer &r)
        {
            r.setClipRegion(wholeView());
            r.setDrawColor(red);
            r.drawFilledRect(ids.prims[0], Rect(20, 20, 100, 80), 100);
            r.setDrawColor(green);
            r.drawFilledRect(ids.prims[1], Rect(40, 40, 40, 30), 200);
        };
    const Frame front=drawFrame(draw);

    CHECK(front.is(60, 55, green));
    CHECK(front.is(30, 30, red));

    // a 180-degree turn about the vertical axis through the view's centre and the
    // z range's middle: x' = W - x, z' = K - z
    const float k=(float)SlotRenderer::kMaxZIndex;
    const float behind[16]={ -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, (float)size.x, 0, k, 1 };

    renderer.setViewTransform(behind);

    const Frame back=drawFrame(draw);

    // mirrored, and the red rect, now the nearer, covers the green one
    CHECK(back.is(size.x-1-60, 55, red));
    CHECK(back.is(size.x-1-30, 30, red));
    CHECK(back.is(60, 55, background));
    CHECK(back.is(5, 5, background));
}

TEST_CASE("View transform: under a perspective camera the reference layer is the front view, a nearer one larger and in front")
{
    Guard guard;
    Ids ids(5, 1);
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Point size=gweni::test::viewSize();
    const float cx=size.x*0.5f, cy=size.y*0.5f;
    // the camera f px in front of the reference layer z0 (powers of two, so w is
    // exactly 1 there): x' = cx + (x-cx)/w, w = 1 - (z-z0)/f, z' = z/w
    const float f=2048.0f, z0=1024.0f;
    const float perspective[16]={
        1, 0, 0, 0,
        0, 1, 0, 0,
        -cx/f, -cy/f, 1, -1/f,
        cx*z0/f, cy*z0/f, 0, 1+z0/f };

    const Frame front=drawFrame([&](SlotRenderer &r) { drawScene(r, ids, (int)z0); });

    renderer.setViewTransform(perspective);

    const Frame reference=drawFrame([&](SlotRenderer &r) { drawScene(r, ids, (int)z0); });

    CHECK(renderer.frameStats().uploadBytes == 0);
    // glyph i of a run is at z0+(i+1)/(glyphs+1), a fraction of a layer nearer,
    // so the text (columns 300..359) is a hair larger; the rest is exact
    CHECK(differing(front, reference, 300, 360) == 0);

    int textDrawn=0;

    for(int y=0; y<size.y; ++y)
        for(int x=300; x<360; ++x)
            if(!reference.is(x, y, background))
                ++textDrawn;
    CHECK(textDrawn > 50);

    // a 40 px square about the centre at z0 + f/4 (w 0.75) is 4/3 as large, in
    // front of the reference layer's fill under it
    renderer.clearViewTransform();

    auto nearer=[&](SlotRenderer &r)
        {
            r.setClipRegion(wholeView());
            r.setDrawColor(red);
            r.drawFilledRect(ids.prims[4], Rect((int)cx-60, (int)cy-40, 120, 80), (int)z0);
            r.setDrawColor(green);
            r.drawFilledRect(ids.prims[0], Rect((int)cx-30, (int)cy-15, 60, 30), (int)(z0+f/4));
            // what the scene drew in the other slots is gone
            for(size_t i=1; i<4; ++i)
                r.hidePrimitive(ids.prims[i]);
            r.hideText(ids.texts[0]);
        };
    const Frame flat=drawFrame(nearer);

    CHECK(flat.is((int)cx+35, (int)cy, red));

    renderer.setViewTransform(perspective);

    const Frame deep=drawFrame(nearer);

    CHECK(deep.is((int)cx, (int)cy, green));
    // 30 px half-width at w 0.75 is 40
    CHECK(deep.is((int)cx+35, (int)cy, green));
    CHECK(deep.is((int)cx+45, (int)cy, red));
    CHECK(deep.is((int)cx, (int)cy+18, green));
    CHECK(deep.is((int)cx, (int)cy+22, red));
}
