/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  ImagePanel on a canvas, drawn offscreen and read back, on every backend:
 *  - an empty panel (nothing set, or cleared) draws nothing, its slot hidden;
 *    an image that fails to load (bad bytes, a missing file) draws the
 *    missing-image magenta over its bounds;
 *  - a new image on a shown panel writes that panel's one primitive slot and
 *    no other, and the old texture is freed (loadedTextureCount() unchanged);
 *    the same bytes, pixels or file name again marks nothing and the frame
 *    after writes nothing, as does a settled frame;
 *  - clearImage() and deleting the panel give its texture back, the count at
 *    its baseline after a load, a replace, a clear, a reload and a delete;
 *  - each scale mode places a 40x20 image (its left quarter red, the rest
 *    green) in a 60x50 panel where it should, at scale 1, 1.5 and 2: the
 *    drawn pixels' bounding box is the expected logical rect with its edges
 *    snapped to device px, and the red/green edge lands within a pixel of
 *    where the mode puts texel 10 (Fill and None crop through the UVs, so the
 *    edge moves with the crop);
 *  - a NavigationItem's icon (an ImagePanel subclass) given pixels through
 *    getIcon() draws them, and setImage(name) then setImage("") on the item
 *    gives the texture back.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/imagePanel.h>
#include <gweni/controls/navigationRail.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <cmath>
#include <vector>

namespace
{

using gweni::Color;
using gweni::controls::ImagePanel;
using gweni::controls::ImageScaleMode;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 255, 255);

constexpr int ImageW=40;
constexpr int ImageH=20;
constexpr int RedColumns=10;

//! 40x20 RGBA: columns 0..9 red, the rest green, or one colour throughout
std::vector<uint8_t> testPixels(Color left=Color(255, 0, 0, 255), Color right=Color(0, 255, 0, 255))
{
    std::vector<uint8_t> rgba((size_t)ImageW*ImageH*4);

    for(int y=0; y<ImageH; ++y)
    {
        for(int x=0; x<ImageW; ++x)
        {
            const Color c=(x < RedColumns)?left:right;
            uint8_t *t=&rgba[((size_t)y*ImageW+x)*4];

            t[0]=c.r;
            t[1]=c.g;
            t[2]=c.b;
            t[3]=c.a;
        }
    }
    return rgba;
}

//! A canvas at a scale on the shared renderer, a host filling it and one 60x50
//! ImagePanel at (20, 10) in the host
struct Scene
{
    explicit Scene(float scale=1.0f)
        : renderer(gweni::test::gpuRenderer())
    {
        const gweni::Point view=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&renderer);
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);
        canvas=new gweni::controls::Canvas(skin);
        canvas->setScale(scale);
        canvas->setSize((int)std::lround(view.x/scale), (int)std::lround(view.y/scale));

        gweni::controls::Base *host=canvas->newChild<gweni::controls::Base>();

        host->setDock(gweni::DockPosition::Center);
        panel=host->newChild<ImagePanel>();
        panel->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
        panel->setBounds(20, 10, 60, 50);
    }

    ~Scene()
    {
        delete canvas;
        delete skin;
        renderer.setScale(1.0f);
        renderer.begin();
        renderer.end();
    }

    //! One frame, then the target read back (top-down RGBA8)
    std::vector<uint8_t> frame()
    {
        std::vector<uint8_t> pixels;

        gweni::test::clearView(background);
        canvas->updateCanvas();
        canvas->renderCanvas();
        gweni::test::readView(pixels);
        return pixels;
    }

    Color at(const std::vector<uint8_t> &pixels, int x, int y) const
    {
        const uint8_t *p=&pixels[((size_t)y*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }

    //! The panel's bounds on the target, device px
    gweni::Rect deviceBounds() const
    {
        const gweni::Point origin=panel->localPosToCanvas(gweni::Point(0, 0));
        const float s=canvas->scale();
        const int x0=(int)std::floor(origin.x*s+0.5f);
        const int y0=(int)std::floor(origin.y*s+0.5f);

        return gweni::Rect(x0, y0, (int)std::floor((origin.x+60)*s+0.5f)-x0,
            (int)std::floor((origin.y+50)*s+0.5f)-y0);
    }

    //! Pixels inside the panel's device bounds that are not the background
    int drawnPixels(const std::vector<uint8_t> &pixels) const
    {
        const gweni::Rect b=deviceBounds();
        int count=0;

        for(int y=b.y; y<b.y+b.h; ++y)
        {
            for(int x=b.x; x<b.x+b.w; ++x)
            {
                if(!(at(pixels, x, y) == background))
                    ++count;
            }
        }
        return count;
    }

    SlotRenderer &renderer;
    gweni::skin::textured::TexturedSkin *skin=nullptr;
    gweni::controls::Canvas *canvas=nullptr;
    ImagePanel *panel=nullptr;
};

bool isRed(const Color &c) { return c.r > 200 && c.g < 60 && c.b < 60; }
bool isGreen(const Color &c) { return c.g > 200 && c.r < 60 && c.b < 60; }

}//namespace

TEST_CASE("ImagePanel: an empty panel draws nothing, a failed load draws the missing image")
{
    Scene scene;

    // nothing set: the slot hidden, so nothing in the bounds
    std::vector<uint8_t> pixels=scene.frame();

    CHECK(scene.panel->isEmpty());
    CHECK_FALSE(scene.panel->failedToLoad());
    CHECK(scene.drawnPixels(pixels) == 0);

    const uint8_t bad[]={ 'n', 'o', 't', ' ', 'a', 'n', ' ', 'i', 'm', 'a', 'g', 'e' };

    scene.panel->setImageData(bad, sizeof(bad));
    pixels=scene.frame();
    CHECK_FALSE(scene.panel->isEmpty());
    CHECK(scene.panel->failedToLoad());

    const gweni::Rect b=scene.deviceBounds();

    CHECK(scene.drawnPixels(pixels) == b.w*b.h);
    CHECK((scene.at(pixels, b.x+b.w/2, b.y+b.h/2) == gweni::colors::Missing));

    // cleared, it is empty again
    scene.panel->clearImage();
    pixels=scene.frame();
    CHECK(scene.panel->isEmpty());
    CHECK(scene.drawnPixels(pixels) == 0);
}

TEST_CASE("ImagePanel: a new image writes its one slot and frees the old, the same image writes nothing")
{
    Scene scene;
    SlotRenderer &renderer=scene.renderer;
    // after the scene's skin loaded its atlas
    const size_t baseline=renderer.loadedTextureCount();
    const std::vector<uint8_t> first=testPixels();
    const std::vector<uint8_t> second=testPixels(Color(0, 255, 0, 255), Color(255, 0, 0, 255));

    scene.panel->setImagePixels(ImageW, ImageH, ImageW*4, first.data());
    scene.frame();
    scene.frame();
    CHECK(renderer.loadedTextureCount() == baseline+1);
    REQUIRE(scene.panel->primitiveId() != std::numeric_limits<size_t>::max());

    // another image: the panel's slot alone
    scene.panel->setImagePixels(ImageW, ImageH, ImageW*4, second.data());
    std::vector<uint8_t> pixels=scene.frame();
    const std::vector<size_t> written=renderer.writtenPrimitiveIds();

    CHECK(renderer.loadedTextureCount() == baseline+1);
    // at most its one slot: the new image takes the layer the old one gave
    // back, so with the same size the slot's bytes can be the same
    CHECK(written.size() <= 1);
    for(size_t id:written)
        CHECK(id == scene.panel->primitiveId());
    CHECK(renderer.getFrameStats().textSlotsWritten == 0);

    const gweni::Rect b=scene.deviceBounds();

    // the second image: green on its left
    CHECK(isGreen(scene.at(pixels, b.x+2, b.y+b.h/2)));
    CHECK(isRed(scene.at(pixels, b.x+b.w-2, b.y+b.h/2)));

    // the same pixels again, from another buffer: no change, nothing written
    const std::vector<uint8_t> again(second);

    gweni::controls::resetChangeStats();
    scene.panel->setImagePixels(ImageW, ImageH, ImageW*4, again.data());
    scene.frame();
    CHECK(gweni::controls::getChangeStats().controls == 0);
    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 0);
    CHECK(renderer.loadedTextureCount() == baseline+1);

    // the same file name again, and the same bytes again
    scene.panel->setImage("test16.png");
    scene.frame();
    gweni::controls::resetChangeStats();
    scene.panel->setImage("test16.png");
    scene.frame();
    CHECK(gweni::controls::getChangeStats().controls == 0);
    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 0);

    const uint8_t bad[]={ 1, 2, 3, 4 };

    scene.panel->setImageData(bad, sizeof(bad));
    scene.frame();
    CHECK(renderer.loadedTextureCount() == baseline);
    gweni::controls::resetChangeStats();
    scene.panel->setImageData(bad, sizeof(bad));
    scene.frame();
    CHECK(gweni::controls::getChangeStats().controls == 0);
    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 0);

    // a settled frame writes nothing
    scene.frame();
    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 0);
}

TEST_CASE("ImagePanel: clearImage and deleting the panel free its texture")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const size_t baseline=renderer.loadedTextureCount();
    const std::vector<uint8_t> image=testPixels();

    {
        Scene scene;

        // the skin's atlas
        CHECK(renderer.loadedTextureCount() == baseline+1);
        scene.panel->setImagePixels(ImageW, ImageH, ImageW*4, image.data());
        scene.frame();
        CHECK(renderer.loadedTextureCount() == baseline+2);

        // cleared: freed, and the frame hides the slot
        scene.panel->clearImage();
        std::vector<uint8_t> pixels=scene.frame();

        CHECK(renderer.loadedTextureCount() == baseline+1);
        CHECK(scene.drawnPixels(pixels) == 0);

        // set again, then the panel deleted with its canvas
        scene.panel->setImage("test16.png");
        scene.frame();
        CHECK(renderer.loadedTextureCount() == baseline+2);
    }
    CHECK(renderer.loadedTextureCount() == baseline);

    {
        Scene scene;

        scene.panel->setImagePixels(ImageW, ImageH, ImageW*4, image.data());
        scene.frame();
        CHECK(renderer.loadedTextureCount() == baseline+2);
        delete scene.panel;
        scene.panel=nullptr;
        CHECK(renderer.loadedTextureCount() == baseline+1);
    }
}

TEST_CASE("ImagePanel: each scale mode draws its rect, edges on device px, at scale 1, 1.5 and 2")
{
    struct Expected
    {
        ImageScaleMode mode;
        const char *name;
        float x, y, w, h;   //!< the drawn rect, logical units from the panel's origin
        float edge;         //!< where texel 10 (red to green) lands, the same units
    };
    // 40x20 into 60x50: Fit is 1.5x (60x30), Fill 2.5x (100x50 from x -20,
    // cropped), None 1x centred
    const Expected modes[]=
    {
        { ImageScaleMode::Stretch, "Stretch", 0, 0, 60, 50, 15 },
        { ImageScaleMode::Fit, "Fit", 0, 10, 60, 30, 15 },
        { ImageScaleMode::Fill, "Fill", 0, 0, 60, 50, 5 },
        { ImageScaleMode::None, "None", 10, 15, 40, 20, 20 },
    };
    const std::vector<uint8_t> image=testPixels();

    for(float scale:{ 1.0f, 1.5f, 2.0f })
    {
        for(const Expected &expected:modes)
        {
            INFO("scale ", scale, ", ", std::string(expected.name));
            Scene scene(scale);

            scene.panel->setImagePixels(ImageW, ImageH, ImageW*4, image.data());
            scene.panel->setScaleMode(expected.mode);

            const std::vector<uint8_t> pixels=scene.frame();
            const gweni::Point origin=scene.panel->localPosToCanvas(gweni::Point(0, 0));
            auto snap=[scale](float v) { return (int)std::floor(v*scale+0.5f); };
            const int x0=snap(origin.x+expected.x);
            const int y0=snap(origin.y+expected.y);
            const int x1=snap(origin.x+expected.x+expected.w);
            const int y1=snap(origin.y+expected.y+expected.h);

            // the drawn pixels' bounding box, and nothing else drawn
            int minX=1 << 20, minY=1 << 20, maxX=-1, maxY=-1;
            const gweni::Point view=gweni::test::viewSize();

            for(int y=0; y<view.y; ++y)
            {
                for(int x=0; x<view.x; ++x)
                {
                    const Color c=scene.at(pixels, x, y);

                    if(!isRed(c) && !isGreen(c))
                        continue;
                    minX=std::min(minX, x);
                    minY=std::min(minY, y);
                    maxX=std::max(maxX, x);
                    maxY=std::max(maxY, y);
                }
            }
            CHECK(minX == x0);
            CHECK(minY == y0);
            CHECK(maxX == x1-1);
            CHECK(maxY == y1-1);
            CHECK(scene.drawnPixels(pixels) == (x1-x0)*(y1-y0));

            // the first column on the middle row more green than red: the
            // filter blends the two texels at the edge, half and half on it
            const int row=(y0+y1)/2;
            int edge=x0;

            while(edge < x1 && scene.at(pixels, edge, row).g <= scene.at(pixels, edge, row).r)
                ++edge;

            // its pixel centre past the edge: the first such column
            const float expectedEdge=(origin.x+expected.edge)*scale-0.5f;

            INFO("edge at ", edge, ", expected ", expectedEdge, ", origin ", origin.x);
            CHECK(std::fabs(edge-expectedEdge) <= 1.0f);
        }
    }
}

TEST_CASE("ImagePanel: a NavigationItem's icon draws an image from pixels and gives a file image back")
{
    Scene scene;
    SlotRenderer &renderer=scene.renderer;
    const size_t baseline=renderer.loadedTextureCount();

    scene.panel->hide();

    auto *rail=scene.canvas->newChild<gweni::controls::NavigationRail>();
    gweni::controls::NavigationItem *item=rail->addItem("Home");
    ImagePanel *icon=item->getIcon();
    const Color red(255, 0, 0, 255);
    const std::vector<uint8_t> pixels=testPixels(red, red);

    // pixels set on the icon itself: it has no name, and it still draws
    icon->setImagePixels(ImageW, ImageH, ImageW*4, pixels.data());
    icon->show();
    scene.frame();

    std::vector<uint8_t> frame=scene.frame();
    const gweni::Point centre=icon->localPosToCanvas(
        gweni::Point(icon->getWidth()/2, icon->getHeight()/2));

    CHECK(renderer.loadedTextureCount() == baseline+1);
    CHECK(isRed(scene.at(frame, centre.x, centre.y)));

    // a file image through the item, then none: the texture is given back
    item->setImage("test16.png");
    scene.frame();
    CHECK(renderer.loadedTextureCount() == baseline+1);
    item->setImage("");
    scene.frame();
    CHECK(icon->isEmpty());
    CHECK(renderer.loadedTextureCount() == baseline);
}
