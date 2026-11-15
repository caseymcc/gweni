/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni drawn over an app's own scene (#687), on the backend under test,
 *  through the textured skin: a canvas with its background off holding an
 *  opaque Button and a translucent panel (the Panel renderer, its surface
 *  given alpha 128 for the case), settled over a plain clear first.
 *
 *    over the scene   the scene's colour and a near depth (0.0) written into
 *                     the target through the backend's own API
 *                     (test::writeAppScene: glClear, a Vulkan clear pass of
 *                     its own), then SlotRenderer::clearDepth() and a frame:
 *                     the scene's colour where no control is, the Button as
 *                     it is drawn over a clearTarget() of the scene's colour
 *                     (unlike the scene), the panel's middle its colour
 *                     blended over the scene's (each channel within 2), and
 *                     the whole frame byte for byte that clearTarget() one.
 *    no clearDepth    the same without clearDepth(): the scene's depth is in
 *                     front of every control, so the frame is the scene alone.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;
namespace tokyonight=gweni::skin::textured::tokyonight;

//! A control the textured skin draws with its Panel renderer (the Panel surface).
class Panel: public gweni::controls::Base
{
public:
    GWENI_CONTROL_INLINE(Panel, gweni::controls::Base)
    {}
};

const Color SceneColor(40, 120, 70, 255);
const Color PanelColor(200, 60, 160, 128);

struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;
    gweni::controls::Button *button;
    Panel *panel;
    const tokyonight::Surface shipped=tokyonight::Panel;

    Scene()
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        tokyonight::Panel.top=PanelColor;
        tokyonight::Panel.bottom=PanelColor;
        tokyonight::Panel.border=PanelColor;

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);
        canvas->setDrawBackground(false);

        button=canvas->newChild<gweni::controls::Button>();
        button->setText("Button");
        button->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
        button->setBounds(20, 20, 120, 40);

        panel=canvas->newChild<Panel>();
        panel->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
        panel->setBounds(200, 20, 160, 100);
    }

    ~Scene()
    {
        delete canvas;
        delete skin;
        tokyonight::Panel=shipped;
    }

    //! Settled over a clearTarget() of the scene's colour; the last frame's pixels.
    std::vector<uint8_t> settle()
    {
        std::vector<uint8_t> pixels;

        for(int i=0; i < 3; ++i)
        {
            gweni::test::clearView(SceneColor);
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
        gweni::test::readView(pixels);
        return pixels;
    }

    //! One frame over the app's scene, with or without clearDepth().
    std::vector<uint8_t> overScene(bool clearDepth)
    {
        std::vector<uint8_t> pixels;

        gweni::test::writeAppScene(SceneColor, 0.0f);
        if(clearDepth)
            gweni::test::gpuRenderer().clearDepth();
        canvas->updateCanvas();
        canvas->renderCanvas();
        gweni::test::readView(pixels);
        return pixels;
    }
};

const uint8_t *at(const std::vector<uint8_t> &pixels, int x, int y)
{
    return &pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];
}

bool isScene(const uint8_t *pixel)
{
    return pixel[0] == SceneColor.r && pixel[1] == SceneColor.g && pixel[2] == SceneColor.b;
}

//! The pixels unlike the scene's colour.
size_t notScene(const std::vector<uint8_t> &pixels)
{
    size_t count=0;

    for(size_t i=0; i < pixels.size(); i+=4)
        if(!isScene(&pixels[i]))
            ++count;
    return count;
}

}//namespace

TEST_CASE("HUD: over an app's scene, clearDepth lets the UI draw and blend over it")
{
    Scene scene;
    const std::vector<uint8_t> reference=scene.settle();
    const std::vector<uint8_t> frame=scene.overScene(true);

    // where no control is
    CHECK(isScene(at(frame, 5, 5)));
    CHECK(isScene(at(frame, 420, 140)));

    // the Button's own look, inside its border, left of its text
    const uint8_t *button=at(frame, 26, 40);
    CHECK_FALSE(isScene(button));
    CHECK(std::memcmp(button, at(reference, 26, 40), 4) == 0);

    // the panel's middle, its colour blended over the scene's
    const uint8_t *panel=at(frame, 280, 70);
    const float a=PanelColor.a/255.0f;
    const uint8_t over[3]={PanelColor.r, PanelColor.g, PanelColor.b};
    const uint8_t under[3]={SceneColor.r, SceneColor.g, SceneColor.b};

    for(int c=0; c < 3; ++c)
    {
        INFO("channel ", c, ": got ", int(panel[c]));
        CHECK(std::abs(panel[c]-(over[c]*a+under[c]*(1.0f-a))) <= 2.5f);
    }

    REQUIRE(frame.size() == reference.size());
    size_t differing=0;

    for(size_t i=0; i < frame.size(); ++i)
        if(frame[i] != reference[i])
            ++differing;
    CHECK(differing == 0);
}

TEST_CASE("HUD: without clearDepth the app's near depth hides every control")
{
    Scene scene;
    const std::vector<uint8_t> reference=scene.settle();

    REQUIRE(notScene(reference) > 0);
    CHECK(notScene(scene.overScene(false)) == 0);
}
