/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's image path (style 2, drawTexturedRect), drawn offscreen and read
 *  back pixel by pixel: a texture loaded on its own (not the skin atlas) samples
 *  its own texels, two textures share the one primitive draw, the draw colour
 *  tints, a hidden slot draws nothing, a texture that did not load draws the
 *  missing-image placeholder, and a Button's setImage icon lands at its bounds in
 *  front of the button and is not re-emitted by a frame where nothing changed.
 *
 *  The expected colours come from pixelColor() (level 0 read back through a
 *  framebuffer), so they do not depend on the draw path under test. Each rect
 *  is drawn 1:1 with the texels, so a pixel centre is a texel centre and the
 *  linear filter returns that texel unblended.
 *
 *  Also: a UV sub-rect in the same frame as the two textures, and a ninth
 *  texture, past the 8 image units.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/imagePanel.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;
using gweni::Texture;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 255, 255);
const Color white(255, 255, 255, 255);
//! drawMissingImage's fill
const Color missingColor=gweni::colors::Missing;

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

void clear()
{
    gweni::test::clearView(background);
}

Frame readBack()
{
    Frame frame;

    gweni::test::readView(frame.pixels);
    return frame;
}

//! Clears to `background`, draws one frame with `draw` and reads it back.
template<typename Draw>
Frame drawFrame(Draw &&draw)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    clear();
    renderer.begin();
    draw(renderer);
    renderer.end();
    return readBack();
}

//! Primitive ids for one test case, released (and that flushed) when it ends.
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

//! A texture loaded from resources/, freed when the case ends (which frees its image unit).
struct LoadedTexture
{
    explicit LoadedTexture(const char *name)
    {
        texture.name=name;
        status=gweni::test::gpuRenderer().loadTexture(texture);
    }

    ~LoadedTexture()
    {
        gweni::test::gpuRenderer().freeTexture(texture);
    }

    Color texel(unsigned int x, unsigned int y) const
    {
        return gweni::test::gpuRenderer().pixelColor(texture, x, y, Color(0, 0, 0, 0));
    }

    Texture texture;
    Texture::Status status;

    bool loaded() const { return status == Texture::Status::Loaded; }
};

//! texel with alpha a*tintAlpha over `background`, as the SRC_ALPHA/ONE_MINUS_SRC_ALPHA
//! blend does
Color over(const Color &texel, int tintAlpha=255)
{
    const float a=texel.a/255.0f*tintAlpha/255.0f;
    auto channel=[a](int c, int b)
    {
        return (unsigned char)std::lround(c*a+b*(1.0f-a));
    };

    // a fully transparent texel is discarded
    if(texel.a == 0)
        return background;
    return Color(channel(texel.r, background.r), channel(texel.g, background.g),
        channel(texel.b, background.b), 255);
}

bool near(const Color &a, const Color &b, int tolerance=2)
{
    return std::abs(a.r-b.r) <= tolerance && std::abs(a.g-b.g) <= tolerance
        && std::abs(a.b-b.b) <= tolerance;
}

//! Pixels of the w x h block at (x, y) that differ from expected(tx, ty) by more than 2.
template<typename Expected>
int blockMismatches(const Frame &frame, int x, int y, int w, int h, Expected &&expected)
{
    int mismatches=0;

    for(int ty=0; ty<h; ++ty)
    {
        for(int tx=0; tx<w; ++tx)
        {
            const Color pixel=frame.at(x+tx, y+ty);
            const Color want=expected(tx, ty);

            if(!near(pixel, want))
            {
                if(mismatches < 4)
                    MESSAGE("pixel (", x+tx, ",", y+ty, ") is (", int(pixel.r), ",", int(pixel.g), ",",
                        int(pixel.b), "), expected (", int(want.r), ",", int(want.g), ",", int(want.b), ")");
                ++mismatches;
            }
        }
    }
    return mismatches;
}

}//namespace

TEST_CASE("Image (style 2): a texture loaded on its own draws its texels, blended over what is behind")
{
    LoadedTexture icon("test16.png");
    REQUIRE(icon.loaded());

    Primitives prims(1);

    const Frame frame=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(white);
            renderer.drawTexturedRect(icon.texture, prims.ids[0], Rect(16, 16, 16, 16), 1);
        });

    // test16.png has transparent corners and opaque middle: both are checked
    CHECK(icon.texel(0, 0).a == 0);
    CHECK(icon.texel(8, 8).a == 255);
    CHECK(blockMismatches(frame, 16, 16, 16, 16, [&](int x, int y) { return over(icon.texel(x, y)); }) == 0);
    // nothing outside the rect
    CHECK(near(frame.at(15, 20), background));
    CHECK(near(frame.at(32, 20), background));
}

TEST_CASE("Image (style 2): two textures in one frame each sample their own, UVs select a sub-rect")
{
    LoadedTexture icon("test16.png");
    LoadedTexture logo("gwen.png");
    REQUIRE(icon.loaded());
    REQUIRE(logo.loaded());

    const gweni::TextureData logoData=gweni::test::gpuRenderer().getTextureData(logo.texture);
    const float logoW=logoData.width;
    const float logoH=logoData.height;

    Primitives prims(2);

    // a 64x64 block of gwen.png from texel (400, 300), 1:1
    const Frame frame=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(white);
            renderer.drawTexturedRect(icon.texture, prims.ids[0], Rect(16, 16, 16, 16), 1);
            renderer.drawTexturedRect(logo.texture, prims.ids[1], Rect(48, 16, 64, 64), 1,
                400/logoW, 300/logoH, 464/logoW, 364/logoH);
        });

    CHECK(blockMismatches(frame, 16, 16, 16, 16, [&](int x, int y) { return over(icon.texel(x, y)); }) == 0);
    CHECK(blockMismatches(frame, 48, 16, 64, 64,
        [&](int x, int y) { return over(logo.texel(400+x, 300+y)); }) == 0);
}

TEST_CASE("Image (style 2): the draw colour tints the texels, its alpha included")
{
    LoadedTexture icon("test16.png");
    REQUIRE(icon.loaded());

    Primitives prims(1);

    const Frame frame=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(Color(255, 255, 255, 128));
            renderer.drawTexturedRect(icon.texture, prims.ids[0], Rect(16, 16, 16, 16), 1);
        });

    CHECK(blockMismatches(frame, 16, 16, 16, 16, [&](int x, int y) { return over(icon.texel(x, y), 128); }) == 0);
}

TEST_CASE("Image (style 2): a hidden slot draws nothing, and showing it draws it again")
{
    LoadedTexture icon("test16.png");
    REQUIRE(icon.loaded());

    SlotRenderer &renderer=gweni::test::gpuRenderer();
    Primitives prims(1);

    drawFrame([&](SlotRenderer &r)
        {
            r.setDrawColor(white);
            r.drawTexturedRect(icon.texture, prims.ids[0], Rect(16, 16, 16, 16), 1);
        });

    renderer.hidePrimitive(prims.ids[0]);
    const Frame hidden=drawFrame([](SlotRenderer &) {});

    CHECK(blockMismatches(hidden, 16, 16, 16, 16, [](int, int) { return background; }) == 0);

    renderer.showPrimitive(2, prims.ids[0]);
    const Frame shown=drawFrame([](SlotRenderer &) {});

    CHECK(blockMismatches(shown, 16, 16, 16, 16, [&](int x, int y) { return over(icon.texel(x, y)); }) == 0);
}

TEST_CASE("Image (style 2): a texture that did not load draws the missing-image placeholder")
{
    Texture missing;
    missing.name="no-such-texture.png";
    const bool notFound=gweni::test::gpuRenderer().loadTexture(missing) == Texture::Status::ErrorFileNotFound;

    REQUIRE(notFound);

    Primitives prims(1);

    const Frame frame=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(white);
            renderer.drawTexturedRect(missing, prims.ids[0], Rect(16, 16, 16, 16), 1);
        });

    CHECK(blockMismatches(frame, 16, 16, 16, 16, [](int, int) { return missingColor; }) == 0);
}

TEST_CASE("Image (style 2): past the image units in use, a new texture draws the placeholder until one is freed")
{
    // one more texture than there are units; each load is its own GL texture
    std::vector<std::unique_ptr<LoadedTexture>> textures;

    for(int i=0; i<9; ++i)
    {
        textures.emplace_back(new LoadedTexture("test16.png"));
        REQUIRE(textures.back()->loaded());
    }

    Primitives prims(9);

    const auto draw=[&](SlotRenderer &renderer)
    {
        renderer.setDrawColor(white);
        for(size_t i=0; i<9; ++i)
            renderer.drawTexturedRect(textures[i]->texture, prims.ids[i], Rect(16+(int)i*20, 16, 16, 16), 1);
    };

    const Frame full=drawFrame(draw);

    for(int i=0; i<8; ++i)
    {
        INFO("rect ", i);
        CHECK(blockMismatches(full, 16+i*20, 16, 16, 16,
            [&](int x, int y) { return over(textures[i]->texel(x, y)); }) == 0);
    }
    CHECK(blockMismatches(full, 16+8*20, 16, 16, 16, [](int, int) { return missingColor; }) == 0);

    // freeing the first gives its unit to the ninth
    textures[0].reset();
    gweni::test::gpuRenderer().hidePrimitive(prims.ids[0]);

    const Frame freed=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(white);
            renderer.drawTexturedRect(textures[8]->texture, prims.ids[8], Rect(16+8*20, 16, 16, 16), 1);
        });

    CHECK(blockMismatches(freed, 16+8*20, 16, 16, 16,
        [&](int x, int y) { return over(textures[8]->texel(x, y)); }) == 0);
}

TEST_CASE("ImagePanel: a Button's setImage icon draws at its bounds in front of the button, once")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const gweni::Point viewSize=gweni::test::viewSize();

    // as in textRunCache.cpp: no setDefaultFont, so deleting the skin keeps test::openSans()
    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(&renderer);
    REQUIRE(skin->init("DefaultSkin.json"));

    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);

    canvas->setSize(viewSize.x, viewSize.y);

    gweni::controls::Button *button=canvas->newChild<gweni::controls::Button>();

    button->setText("Icon");
    // the canvas does not keep a child's x, y (#104), so its padding moves the button
    // off the origin, where a rect drawn in local coordinates would land
    canvas->setPadding(gweni::Padding(30, 20, 0, 0));
    button->setDock(gweni::DockPosition::Top);
    // a fixed 20 px high, tall enough for the 16 px icon: the button clips its children,
    // so an icon centred in the text-high button Shrink gives would lose its outer rows
    button->setSizeFlags({gweni::SizeFlag::Shrink, gweni::SizeFlag::Fixed});
    button->setHeight(20);
    button->setImage("test16.png");

    gweni::controls::ImagePanel *image=nullptr;

    for(gweni::controls::Base *child:button->getChildren())
    {
        if(auto *panel=dynamic_cast<gweni::controls::ImagePanel *>(child))
            image=panel;
    }
    REQUIRE(image != nullptr);

    const auto frame=[&]()
    {
        clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    };

    frame();
    frame();

    const Frame shot=readBack();
    const gweni::Point at=image->localPosToCanvas(gweni::Point(0, 0));

    INFO("button at ", button->getX(), ",", button->getY(), ", image at ", at.x, ",", at.y);
    REQUIRE(image->getWidth() == 16);
    REQUIRE(image->getHeight() == 16);
    // not at the button's (or the canvas's) origin
    CHECK(at.x > 30);
    CHECK(at.y > 10);

    // the opaque texels show unblended: in front of the button's background
    LoadedTexture icon("test16.png");
    int opaque=0;
    int mismatches=0;

    for(int y=0; y<16; ++y)
    {
        for(int x=0; x<16; ++x)
        {
            const Color texel=icon.texel(x, y);

            if(texel.a != 255)
                continue;
            ++opaque;
            if(!near(shot.at(at.x+x, at.y+y), texel))
                ++mismatches;
        }
    }
    CHECK(opaque > 100);
    CHECK(mismatches == 0);

    // a frame where nothing changed writes nothing, the ImagePanel included
    gweni::controls::resetChangeStats();
    frame();
    CHECK(gweni::controls::getChangeStats().controls == 0);
    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 0);

    // a new tint is a change, drawn in that frame
    button->setImageAlpha(0.5f);
    gweni::controls::resetChangeStats();
    frame();
    CHECK(gweni::controls::getChangeStats().controls >= 1);
    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 1);

    delete canvas;
    delete skin;
    renderer.begin();
    renderer.end();
}
