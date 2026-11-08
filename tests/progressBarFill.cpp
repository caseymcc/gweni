/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's ProgressBar, drawn offscreen and read back: the fill
 *  shows over the track (it is drawn at the bar's z+1, which depthLayers()
 *  reserves), covers its share of the track from the left or, vertical, from
 *  the bottom, and a setProgress after the first frame moves it on the next
 *  one, with the auto label on and off. A frame with no change writes nothing.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/progressBar.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <cstdlib>
#include <vector>

namespace
{

using gweni::Color;

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

bool near(const Color &a, const Color &b, int tolerance=2)
{
    return std::abs(a.r-b.r) <= tolerance && std::abs(a.g-b.g) <= tolerance &&
        std::abs(a.b-b.b) <= tolerance;
}

//! A textured-skin canvas over the whole view holding one ProgressBar.
struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;
    gweni::controls::ProgressBar *bar;

    explicit Scene(bool vertical)
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);
        canvas->setPadding(gweni::Padding(10, 10, 10, 10));

        bar=canvas->newChild<gweni::controls::ProgressBar>();

        if(vertical)
        {
            bar->setVertical();
            bar->setDock(gweni::DockPosition::Left);
            bar->setWidth(40);
        }
        else
        {
            bar->setDock(gweni::DockPosition::Top);
            bar->setHeight(40);
        }
    }

    ~Scene()
    {
        delete canvas;
        delete skin;

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.begin();
        renderer.end();
    }

    Frame frame()
    {
        gweni::test::clearView(gweni::Color(0, 0, 0, 255));
        canvas->updateCanvas();
        canvas->renderCanvas();

        Frame shot;

        gweni::test::readView(shot.pixels);
        return shot;
    }

    //! The pixel at \p fx, \p fy of the bar's bounds (0..1 each), on the canvas.
    Color sample(const Frame &shot, float fx, float fy) const
    {
        const gweni::Point at=bar->localPosToCanvas(gweni::Point(0, 0));

        return shot.at(at.x+(int)(bar->getWidth()*fx), at.y+(int)(bar->getHeight()*fy));
    }
};

}//namespace

TEST_CASE("ProgressBar: the fill draws over the track and follows setProgress after the first frame")
{
    for(const bool autoLabel : {true, false})
    {
        CAPTURE(autoLabel);
        Scene scene(false);

        scene.bar->setAutoLabel(autoLabel);
        scene.bar->setProgress(0.0f);
        scene.frame();

        Frame shot=scene.frame();

        REQUIRE(scene.bar->getWidth() > 200);
        // no fill: the left and the right of the track alike (below the label)
        const Color track=scene.sample(shot, 0.9f, 0.8f);

        CHECK(near(scene.sample(shot, 0.1f, 0.8f), track));

        // half: the left quarter is filled, the right quarter is still track
        scene.bar->setProgress(0.5f);
        shot=scene.frame();

        const Color fill=scene.sample(shot, 0.1f, 0.8f);

        CHECK_FALSE(near(fill, track, 16));
        CHECK(near(scene.sample(shot, 0.4f, 0.8f), fill, 16));
        CHECK(near(scene.sample(shot, 0.6f, 0.8f), track));
        CHECK(near(scene.sample(shot, 0.9f, 0.8f), track));
        if(autoLabel)
            CHECK(scene.bar->getText() == "50%");

        // full
        scene.bar->setProgress(1.0f);
        shot=scene.frame();
        CHECK(near(scene.sample(shot, 0.9f, 0.8f), fill, 16));

        // a frame with no change writes nothing
        gweni::controls::resetChangeStats();
        scene.frame();
        CHECK(gweni::controls::getChangeStats().controls == 0);
        CHECK(gweni::test::gpuRenderer().getFrameStats().primitiveSlotsWritten == 0);
    }
}

TEST_CASE("ProgressBar: a vertical bar fills from the bottom up")
{
    Scene scene(true);

    scene.bar->setAutoLabel(false);
    scene.bar->setProgress(0.0f);
    scene.frame();

    Frame shot=scene.frame();

    REQUIRE(scene.bar->getHeight() > 100);
    const Color track=scene.sample(shot, 0.5f, 0.1f);

    CHECK(near(scene.sample(shot, 0.5f, 0.9f), track));

    scene.bar->setProgress(0.5f);
    shot=scene.frame();

    const Color fill=scene.sample(shot, 0.5f, 0.9f);

    CHECK_FALSE(near(fill, track, 16));
    CHECK(near(scene.sample(shot, 0.5f, 0.1f), track));
    CHECK(near(scene.sample(shot, 0.5f, 0.4f), track));
    CHECK_FALSE(near(scene.sample(shot, 0.5f, 0.6f), track, 16));
}
