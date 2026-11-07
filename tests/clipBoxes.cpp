/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's shader clipping: every primitive and glyph slot carries the
 *  clipRegion() current when it was drawn, and the fragment shaders discard what
 *  lies outside it. Drawn offscreen and read back.
 *
 *  The renderer is shared with the other cases, so each case puts the clip
 *  region back to the whole view when it ends (ClipGuard), and the scale and
 *  render offset back to 1x and 0,0 (ScaleGuard).
 *
 *  The last cases draw quads past the view's top-left edge and past what the
 *  signed 16-bit vertex position holds: a straddling quad draws exactly its
 *  on-view part, and one with a corner outside +/-32767 device px nothing.
 *
 *  The clipping cases: a slot after a hidden one, a region past the top-left
 *  edge, an empty region, text laid out, moved and re-clipped. Every primitive
 *  style (1, 2, 4, 5, 6, 7) is drawn straddling the view's left edge, top edge
 *  and both, at scales 1 and 1.5: nothing outside the rect's on-view part, and
 *  that part pixel for pixel the same rect drawn wholly inside the view,
 *  shifted. Quads with a corner past +/-32767 device px (straddling 32767 or
 *  65535, the far rows of a list at 2x, a right edge past 32767) or wholly
 *  left of and above the view draw nothing; a culled quad keeps its style
 *  through hide/show, and drawing it again writes and uploads nothing.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::OpenGLCore;

const Color background(0, 0, 255, 255);
const Color white(255, 255, 255, 255);

//! One frame read back from the offscreen framebuffer, top-down RGBA8.
struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }

    bool isBackground(int x, int y) const
    {
        const Color c=at(x, y);

        return (c.r == background.r) && (c.g == background.g) && (c.b == background.b);
    }

    bool isWhite(int x, int y) const
    {
        const Color c=at(x, y);

        return (c.r == white.r) && (c.g == white.g) && (c.b == white.b);
    }

    //! Pixels in columns [x0, x1) that are not the background.
    int drawnInColumns(int x0, int x1) const
    {
        const Point size=gweni::test::viewSize();
        int drawn=0;

        for(int y=0; y<size.y; ++y)
            for(int x=x0; x<x1; ++x)
                if(!isBackground(x, y))
                    ++drawn;
        return drawn;
    }
};

//! Clears to `background`, draws one frame with `draw` and reads it back.
template<typename Draw>
Frame drawFrame(Draw &&draw)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Point size=gweni::test::viewSize();

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

Rect wholeView()
{
    return Rect(Point(0, 0), gweni::test::viewSize());
}

//! Puts the clip region back to the whole view, so the next case draws unclipped.
struct ClipGuard
{
    ~ClipGuard() { gweni::test::gpuRenderer().setClipRegion(wholeView()); }
};

//! Puts the scale and the render offset back, so the next case draws at 1x.
struct ScaleGuard
{
    explicit ScaleGuard(float scale) { gweni::test::gpuRenderer().setScale(scale); }

    ~ScaleGuard()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

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

}//namespace

TEST_CASE("Clip boxes: a primitive is cut at the region it was drawn in, after a hidden slot")
{
    ClipGuard guard;
    Primitives prims(3);
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    // ids[0] is drawn unclipped and hidden: a hidden slot still counts in
    // gl_PrimitiveID, so the slots after it must still find their own boxes
    renderer.setClipRegion(wholeView());
    renderer.setDrawColor(white);
    renderer.drawFilledRect(prims.ids[0], wholeView(), 1);
    renderer.hidePrimitive(prims.ids[0]);

    const Frame frame=drawFrame([&](OpenGLCore &renderer)
        {
            renderer.setDrawColor(white);
            renderer.setClipRegion(Rect(40, 30, 100, 50));
            renderer.drawFilledRect(prims.ids[1], wholeView(), 2);
            renderer.setClipRegion(Rect(200, 30, 100, 50));
            renderer.drawRoundedRect(prims.ids[2], wholeView(), 0.0f, 0.0f, white, white, white, 3);
        });

    // the fill: [40,140) x [30,80)
    CHECK(frame.isWhite(40, 30));
    CHECK(frame.isWhite(139, 79));
    CHECK(frame.isBackground(39, 30));
    CHECK(frame.isBackground(40, 29));
    CHECK(frame.isBackground(140, 79));
    CHECK(frame.isBackground(139, 80));

    // the rounded rect: [200,300) x [30,80)
    CHECK(frame.isWhite(250, 55));
    CHECK(frame.isBackground(199, 55));
    CHECK(frame.isBackground(300, 55));
    CHECK(frame.isBackground(250, 29));
    CHECK(frame.isBackground(250, 80));

    CHECK(frame.isBackground(170, 55));
    CHECK(frame.isBackground(5, 5));
}

TEST_CASE("Clip boxes: a region past the top-left edge is clamped, an empty one draws nothing")
{
    ClipGuard guard;
    Primitives prims(2);

    const Frame frame=drawFrame([&](OpenGLCore &renderer)
        {
            renderer.setDrawColor(white);
            // a negative corner must not wrap in uint16_t: this keeps [0,40) x [0,30)
            renderer.setClipRegion(Rect(-20, -20, 60, 50));
            renderer.drawFilledRect(prims.ids[0], wholeView(), 1);
            // as addClipRegion leaves a region that misses its parent: negative width
            renderer.setClipRegion(Rect(200, 40, -10, 50));
            renderer.drawFilledRect(prims.ids[1], wholeView(), 2);
        });

    CHECK(frame.isWhite(0, 0));
    CHECK(frame.isWhite(39, 29));
    CHECK(frame.isBackground(40, 0));
    CHECK(frame.isBackground(0, 30));

    CHECK(frame.isBackground(195, 60));
    CHECK(frame.isBackground(200, 60));
}

TEST_CASE("Clip boxes: text is cut, and follows a new region when moved or only re-clipped")
{
    ClipGuard guard;
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const gweni::String text="MMMMMMMMMMMMMMMM";
    const int cut=30;
    const int width=gweni::test::viewSize().x;
    size_t textId=0;

    renderer.generateTextPrimitive(textId);

    auto drawText=[&](Point pos, Rect clip)
    {
        return drawFrame([&](OpenGLCore &renderer)
            {
                renderer.setDrawColor(white);
                renderer.setClipRegion(clip);
                renderer.renderText(textId, gweni::test::openSans(), pos, 1, text);
            });
    };

    SUBCASE("laid out")
    {
        const Frame frame=drawText(Point(4, 40), Rect(0, 0, cut, 160));

        CHECK(frame.drawnInColumns(0, cut) > 0);
        CHECK(frame.drawnInColumns(cut, width) == 0);
    }

    SUBCASE("moved: the unchanged string takes the region it is drawn in now")
    {
        drawText(Point(4, 40), Rect(0, 0, cut, 160));

        const Frame frame=drawText(Point(8, 60), wholeView());

        CHECK(frame.drawnInColumns(cut, width) > 0);
    }

    SUBCASE("not moved, only re-clipped")
    {
        const Frame unclipped=drawText(Point(4, 40), wholeView());

        CHECK(unclipped.drawnInColumns(cut, width) > 0);

        const Frame clipped=drawText(Point(4, 40), Rect(0, 0, cut, 160));

        CHECK(gweni::test::gpuRenderer().getFrameStats().textSlotsWritten > 0);
        CHECK(clipped.drawnInColumns(0, cut) > 0);
        CHECK(clipped.drawnInColumns(cut, width) == 0);

        // the same string, place and region again: nothing to upload
        drawText(Point(4, 40), Rect(0, 0, cut, 160));
        CHECK(gweni::test::gpuRenderer().getFrameStats().textSlotsWritten == 0);
        CHECK(gweni::test::gpuRenderer().getFrameStats().uploadBytes == 0);
    }

    renderer.releaseTextPrimitive(textId);
    renderer.begin();
    renderer.end();
}

namespace
{

//! The primitive styles a quad can be drawn with, each through its own draw call.
enum class Style { Fill=1, Image=2, Rounded=4, Icon=5, Bilinear=6, Checker=7 };

const Style allStyles[]={Style::Fill, Style::Image, Style::Rounded, Style::Icon, Style::Bilinear,
    Style::Checker};

//! Draws `rect` (canvas units) into `id` with `style`, in colours that make every
//! part of it show: a gradient, a border, round corners, a texture.
void drawStyled(OpenGLCore &renderer, Style style, size_t id, const Rect &rect,
    const gweni::Texture &texture)
{
    const Color red(220, 40, 40, 255);
    const Color green(40, 200, 60, 255);
    const Color yellow(240, 220, 40, 255);

    switch(style)
    {
    case Style::Fill:
        renderer.setDrawColor(red);
        renderer.drawFilledRect(id, rect, 1);
        break;
    case Style::Image:
        renderer.setDrawColor(white);
        renderer.drawTexturedRect(texture, id, rect, 1);
        break;
    case Style::Rounded:
        renderer.drawRoundedRect(id, rect, 6.0f, 2.0f, red, green, yellow, 1);
        break;
    case Style::Icon:
        renderer.drawIcon(id, rect, 1, 0, yellow, 1);
        break;
    case Style::Bilinear:
        renderer.drawBilinearRect(id, rect, 1, white, red, green, yellow, 4.0f, 1.0f, white);
        break;
    case Style::Checker:
        renderer.drawCheckerRect(id, rect, 1, Color(255, 0, 0, 128), white, Color(60, 60, 60, 255),
            4.0f, 1.0f, yellow);
        break;
    }
}

//! A device-pixel rect [x0,x1) x [y0,y1).
struct DeviceRect
{
    int x0, y0, x1, y1;

    bool contains(int x, int y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; }
};

DeviceRect deviceRect(const Rect &rect, float scale)
{
    using gweni::renderer::Base;

    return {Base::snap(float(rect.x), scale), Base::snap(float(rect.y), scale),
        Base::snap(float(rect.x+rect.w), scale), Base::snap(float(rect.y+rect.h), scale)};
}

int notBackground(const Frame &frame)
{
    const Point size=gweni::test::viewSize();
    int drawn=0;

    for(int y=0; y<size.y; ++y)
        for(int x=0; x<size.x; ++x)
            if(!frame.isBackground(x, y))
                ++drawn;
    return drawn;
}

}//namespace

TEST_CASE("Clip boxes: a quad straddling the view's left or top edge draws exactly its on-view part")
{
    ClipGuard guard;
    Primitives prims(1);
    const Point size=gweni::test::viewSize();

    gweni::Texture texture;

    texture.name="test16.png";
    REQUIRE(gweni::test::gpuRenderer().loadTexture(texture) == gweni::Texture::Status::Loaded);

    // the straddling rect and the same rect moved by `shift` canvas units, wholly in
    // the view; 40 is a whole, even number of device px at 1 and 1.5, so the snapped
    // edges (and the icon's 2x2-pixel AA blocks) move by exactly that much
    struct Edge
    {
        const char *name;
        Rect rect;
        Point shift;
    };
    const Edge edges[]=
    {
        {"left", Rect(-17, 30, 46, 34), Point(40, 0)},
        {"top", Rect(30, -13, 46, 34), Point(0, 40)},
        {"top-left", Rect(-17, -13, 46, 34), Point(40, 40)},
    };

    for(float scale : {1.0f, 1.5f})
    {
        ScaleGuard scaleGuard(scale);

        for(const Edge &edge : edges)
        {
            for(Style style : allStyles)
            {
                CAPTURE(scale);
                CAPTURE(edge.name);
                CAPTURE(int(style));

                const Rect placed(edge.rect.x+edge.shift.x, edge.rect.y+edge.shift.y,
                    edge.rect.w, edge.rect.h);
                const Frame whole=drawFrame([&](OpenGLCore &renderer)
                    {
                        drawStyled(renderer, style, prims.ids[0], placed, texture);
                    });
                const Frame frame=drawFrame([&](OpenGLCore &renderer)
                    {
                        drawStyled(renderer, style, prims.ids[0], edge.rect, texture);
                    });

                const DeviceRect onView=deviceRect(edge.rect, scale);
                const int dx=gweni::renderer::Base::snap(float(edge.shift.x), scale);
                const int dy=gweni::renderer::Base::snap(float(edge.shift.y), scale);

                REQUIRE(notBackground(whole) > 0);

                int outside=0;
                int differ=0;

                for(int y=0; y<size.y; ++y)
                {
                    for(int x=0; x<size.x; ++x)
                    {
                        if(!onView.contains(x, y))
                        {
                            if(!frame.isBackground(x, y))
                                ++outside;
                            continue;
                        }

                        const Color a=frame.at(x, y);
                        const Color b=whole.at(x+dx, y+dy);

                        if(a.r != b.r || a.g != b.g || a.b != b.b)
                            ++differ;
                    }
                }

                CHECK(outside == 0);
                CHECK(differ == 0);
            }
        }
    }

    gweni::test::gpuRenderer().freeTexture(texture);
}

TEST_CASE("Clip boxes: a quad with a corner past +/-32767 device px draws nothing, nor does one wholly left of and above the view")
{
    ClipGuard guard;
    Primitives prims(1);
    OpenGLCore &renderer=gweni::test::gpuRenderer();

    gweni::Texture texture;

    texture.name="test16.png";
    REQUIRE(renderer.loadTexture(texture) == gweni::Texture::Status::Loaded);

    struct Case
    {
        const char *name;
        Rect rect;
        float scale;
    };
    const Case cases[]=
    {
        // its bottom edge past int16_t, which would wrap to -32746 and span the view
        {"straddling 32767", Rect(40, 32760, 40, 30), 1.0f},
        {"past 32767", Rect(40, 40000, 40, 30), 1.0f},
        // its bottom edge past uint16_t, which wrapped to 24 before
        {"straddling 65535", Rect(40, 65530, 40, 30), 1.0f},
        // the far rows of a long list at scale 2: 16400*2 is past 32767
        {"past 32767 at 2x", Rect(40, 16400, 40, 30), 2.0f},
        {"right edge past 32767", Rect(32750, 40, 40, 30), 1.0f},
        {"wholly left of and above the view", Rect(-80, -60, 40, 30), 1.0f},
        {"wholly left of the view", Rect(-80, 40, 40, 30), 1.5f},
    };

    for(const Case &c : cases)
    {
        ScaleGuard scaleGuard(c.scale);

        for(Style style : allStyles)
        {
            CAPTURE(c.name);
            CAPTURE(int(style));

            const Frame frame=drawFrame([&](OpenGLCore &renderer)
                {
                    drawStyled(renderer, style, prims.ids[0], c.rect, texture);
                });

            CHECK(notBackground(frame) == 0);
        }
    }

    SUBCASE("a culled quad keeps its style through hide and show, and a settled frame uploads nothing")
    {
        const Rect far(40, 40000, 40, 30);
        auto styleOf=[&](size_t id)
        {
            const OpenGLCore::MirrorSnapshot mirror=renderer.snapshotMirror();
            const std::vector<uint8_t> &bytes=mirror.primitives.at(id);
            uint16_t style=0;

            std::copy(bytes.begin(), bytes.begin()+sizeof(style), reinterpret_cast<uint8_t *>(&style));
            return style;
        };

        drawFrame([&](OpenGLCore &renderer)
            {
                drawStyled(renderer, Style::Rounded, prims.ids[0], far, texture);
            });
        CHECK(styleOf(prims.ids[0]) == 4);

        const Frame again=drawFrame([&](OpenGLCore &renderer)
            {
                drawStyled(renderer, Style::Rounded, prims.ids[0], far, texture);
            });
        CHECK(renderer.getFrameStats().primitiveSlotsWritten == 0);
        CHECK(renderer.getFrameStats().uploadBytes == 0);
        CHECK(notBackground(again) == 0);

        renderer.hidePrimitive(prims.ids[0]);
        CHECK(styleOf(prims.ids[0]) == 0);
        renderer.showPrimitive(4, prims.ids[0]);
        CHECK(styleOf(prims.ids[0]) == 4);

        const Frame shown=drawFrame([&](OpenGLCore &) {});

        CHECK(notBackground(shown) == 0);
    }

    renderer.freeTexture(texture);
}
