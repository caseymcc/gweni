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
 *  Also: a UV sub-rect in the same frame as the two textures; 50 distinct
 *  textures in one frame, each its own layer of the smallest class's image page,
 *  which grows (copying its layers) as they load and gives a freed layer to the
 *  next load, also while frames drawing from the old page are in flight; a
 *  translucent image over another in the same draw; and an image of
 *  each size class, one larger than the largest page loaded at the first level
 *  of its mip chain that fits.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/imagePanel.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <gweni/utils/filesystem.h>

#include <imglib/png.h>
#include <imglib/simpleImage.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <system_error>
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

//! A texture loaded from resources/, freed when the case ends (which frees its layer).
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

namespace
{

//! Names this process's files apart from another run's (unittests and
//! unittests_vulkan run side by side)
const std::string &runToken()
{
    static const std::string token=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    return token;
}

//! A size x size RGBA image of its own, written as a PNG under the temp dir and
//! loaded from there by its absolute path (CurrentPath appends it to nothing),
//! freed and removed when the case ends. Texel (x, y) is (seed, 16x, 16y, alpha),
//! so no two seeds draw the same pixels.
struct GeneratedTexture
{
    GeneratedTexture(int seed, int size, uint8_t alpha=255)
        : seed(seed)
        , alpha(alpha)
    {
        imglib::SimpleImage image(imglib::Format::RGBA, imglib::Depth::Bit8, size, size);

        for(int y=0; y<size; ++y)
        {
            for(int x=0; x<size; ++x)
            {
                uint8_t *texel=image.data+((size_t)y*size+x)*4;

                texel[0]=(uint8_t)seed;
                texel[1]=(uint8_t)(x*16);
                texel[2]=(uint8_t)(y*16);
                texel[3]=alpha;
            }
        }
        path=(fs::temp_directory_path()/("gweni-imageRects-"+runToken()+"-"+std::to_string(seed)+".png")).string();
        REQUIRE(imglib::savePng(image, path.c_str()));
        texture.name=path;
        status=gweni::test::gpuRenderer().loadTexture(texture);
    }

    ~GeneratedTexture()
    {
        gweni::test::gpuRenderer().freeTexture(texture);
        std::error_code error;
        fs::remove(path, error);
    }

    Color texel(int x, int y) const { return Color((uint8_t)seed, (uint8_t)(x*16), (uint8_t)(y*16), alpha); }
    bool loaded() const { return status == Texture::Status::Loaded; }

    int seed;
    uint8_t alpha;
    std::string path;
    Texture texture;
    Texture::Status status;
};

size_t textureLayer(const Texture &texture)
{
    size_t imageClass=0;
    size_t layer=gweni::renderer::NoImageLayer;

    gweni::test::gpuRenderer().getTextureLayer(texture, imageClass, layer);
    return layer;
}

}//namespace

TEST_CASE("Image (style 2): 50 distinct textures in one frame each draw their own texels, the full page growing")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    // 16x16 images: the smallest class, whose page starts at 8 layers, so 50 of
    // them grow it (each growth copying the layers in use) at least twice
    const size_t layersBefore=renderer.imagePageLayers(0);
    constexpr int count=50;
    std::vector<std::unique_ptr<GeneratedTexture>> textures;

    for(int i=0; i<count; ++i)
    {
        textures.emplace_back(new GeneratedTexture(i*5, 16));
        REQUIRE(textures.back()->loaded());
    }
    CHECK(renderer.imagePageLayers(0) >= (size_t)count);
    CHECK(renderer.imagePageLayers(0) >= std::max<size_t>(layersBefore, 8)*4);

    Primitives prims(count);
    const auto rectOf=[](int i) { return Rect(8+(i%10)*20, 8+(i/10)*20, 16, 16); };
    size_t regrows=0;

    const Frame frame=drawFrame([&](SlotRenderer &r)
        {
            r.setDrawColor(white);
            for(int i=0; i<count; ++i)
                r.drawTexturedRect(textures[i]->texture, prims.ids[i], rectOf(i), 1);
        });
    // the growths were counted by the frame's flush
    regrows=renderer.getFrameStats().imagePageRegrows;
    CHECK(regrows >= 2);

    for(int i=0; i<count; ++i)
    {
        INFO("image ", i);
        const Rect rect=rectOf(i);

        CHECK(blockMismatches(frame, rect.x, rect.y, 16, 16,
            [&](int x, int y) { return over(textures[i]->texel(x, y)); }) == 0);
        // what pixelColor reads back from its layer is what was loaded
        CHECK((textures[i]->texel(5, 9) == gweni::test::gpuRenderer().pixelColor(textures[i]->texture, 5, 9, Color())));
    }

    // a freed layer is the next load's, and the page does not grow for it
    const size_t freedLayer=textureLayer(textures[7]->texture);
    const size_t layers=renderer.imagePageLayers(0);

    textures[7].reset();
    textures[7].reset(new GeneratedTexture(251, 16));
    REQUIRE(textures[7]->loaded());
    CHECK(textureLayer(textures[7]->texture) == freedLayer);
    CHECK(renderer.imagePageLayers(0) == layers);

    const Frame reused=drawFrame([&](SlotRenderer &r)
        {
            r.setDrawColor(white);
            r.drawTexturedRect(textures[7]->texture, prims.ids[7], rectOf(7), 1);
        });

    CHECK(renderer.getFrameStats().imagePageRegrows == 0);
    CHECK(blockMismatches(reused, rectOf(7).x, rectOf(7).y, 16, 16,
        [&](int x, int y) { return over(textures[7]->texel(x, y)); }) == 0);
    CHECK(blockMismatches(reused, rectOf(8).x, rectOf(8).y, 16, 16,
        [&](int x, int y) { return over(textures[8]->texel(x, y)); }) == 0);
}

TEST_CASE("Image (style 2): a page that grows between frames still in flight draws every image")
{
    // each load is followed by a frame that draws every image so far and is not
    // read back, so on Vulkan the frames before a growth may still be sampling
    // the old page when it is replaced (it must be retired, not destroyed; the
    // validation layer fails the run on a destroyed image in use)
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    // more images than the page has layers now (an earlier case may have grown
    // it and freed them), so it grows at least once whatever ran before
    const int count=std::max(40, (int)renderer.imagePageLayers(0)+8);
    std::vector<std::unique_ptr<GeneratedTexture>> textures;
    Primitives prims((size_t)count);
    // 20 a row: up to 140 in the 448x160 view
    const auto rectOf=[](int i) { return Rect(8+(i%20)*20, 8+(i/20)*20, 16, 16); };

    REQUIRE(count <= 140);
    size_t regrows=0;

    for(int i=0; i<count; ++i)
    {
        textures.emplace_back(new GeneratedTexture((1+i*3) & 0xff, 16));
        REQUIRE(textures.back()->loaded());

        renderer.begin();
        renderer.setDrawColor(white);
        for(int j=0; j<=i; ++j)
            renderer.drawTexturedRect(textures[j]->texture, prims.ids[j], rectOf(j), 1);
        renderer.end();
        regrows+=renderer.getFrameStats().imagePageRegrows;
    }
    CHECK(regrows >= 1);

    const Frame frame=drawFrame([&](SlotRenderer &r)
        {
            r.setDrawColor(white);
            for(int i=0; i<count; ++i)
                r.drawTexturedRect(textures[i]->texture, prims.ids[i], rectOf(i), 1);
        });

    for(int i=0; i<count; ++i)
    {
        INFO("image ", i);
        const Rect rect=rectOf(i);

        CHECK(blockMismatches(frame, rect.x, rect.y, 16, 16,
            [&](int x, int y) { return over(textures[i]->texel(x, y)); }) == 0);
    }
}

TEST_CASE("Image (style 2): a translucent image over another in one draw blends over it, in slot order")
{
    GeneratedTexture under(10, 16);
    GeneratedTexture top(200, 16, 128);
    REQUIRE(under.loaded());
    REQUIRE(top.loaded());

    Primitives prims(2);

    const Frame frame=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(white);
            renderer.drawTexturedRect(under.texture, prims.ids[0], Rect(16, 16, 16, 16), 1);
            renderer.drawTexturedRect(top.texture, prims.ids[1], Rect(16, 16, 16, 16), 2);
        });

    CHECK(blockMismatches(frame, 16, 16, 16, 16, [&](int x, int y)
        {
            const Color below=under.texel(x, y);
            const Color above=top.texel(x, y);
            const float a=above.a/255.0f;
            auto channel=[a](int c, int b) { return (unsigned char)std::lround(c*a+b*(1.0f-a)); };

            return Color(channel(above.r, below.r), channel(above.g, below.g), channel(above.b, below.b), 255);
        }) == 0);
}

TEST_CASE("Image (style 2): an image of each class draws its own texels, the largest reduced to the largest page")
{
    // one image per page, 1:1 in a 64x64 block of each, and one larger than the
    // largest page, loaded as the first level of its mip chain that fits
    struct Sized
    {
        int size;
        size_t imageClass;
    };
    const Sized sizes[]={ { 100, 0 }, { 200, 1 }, { 300, 2 }, { 700, 3 }, { 1500, 4 }, { 4000, 4 } };
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    for(const Sized &sized:sizes)
    {
        INFO("size ", sized.size);
        GeneratedTexture image(sized.size & 0xff, sized.size);
        REQUIRE(image.loaded());

        size_t imageClass=0;
        size_t layer=0;

        REQUIRE(renderer.getTextureLayer(image.texture, imageClass, layer));
        CHECK(imageClass == sized.imageClass);

        const gweni::TextureData data=renderer.getTextureData(image.texture);

        if(sized.size > SlotRenderer::kMaxImageSide)
            CHECK(data.width == (float)(sized.size >> 1));
        else
            CHECK(data.width == (float)sized.size);

        Primitives prims(1);
        const float w=data.width;

        // the block at texel (8, 8) of level 0, past the image's first 16
        const Frame frame=drawFrame([&](SlotRenderer &r)
            {
                r.setDrawColor(white);
                r.drawTexturedRect(image.texture, prims.ids[0], Rect(16, 16, 8, 8), 1, 8/w, 8/w, 16/w, 16/w);
            });

        CHECK(blockMismatches(frame, 16, 16, 8, 8, [&](int x, int y)
            {
                return over(renderer.pixelColor(image.texture, 8+x, 8+y, Color()));
            }) == 0);
    }
}

TEST_CASE("ImagePanel: a Button's setImage icon draws at its bounds in front of the button, once")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const gweni::Point viewSize=gweni::test::viewSize();

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(&renderer);
    REQUIRE(skin->init("DefaultSkin.json"));
    skin->setDefaultFont("OpenSans.ttf", 11);

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
