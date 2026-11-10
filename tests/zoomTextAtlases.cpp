/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Text in a ZoomPanel is drawn at its size times the zoom, and a continuous
 *  zoom must not leave a font atlas behind per size it passed through: a
 *  grayscale coverage atlas is rasterised at the whole device px nearest
 *  (SlotRenderer::coverageSize), and a font only a use loaded (renderText at a
 *  zoomed size, with no loadFont of its own) is erased when its last run
 *  leaves it. So a Label of the skin's 11 px font in a ZoomPanel swept from
 *  zoom 1 down to 0.25, up to 4 and back to 0.25 in 200 steps, then back to 1,
 *  has at most one font more loaded than at the start on every frame (the size
 *  it is drawn at now), whole sizes only, and as many as at the start once
 *  back at 1; a zoom inside the SDF range uploads nothing to an atlas; and
 *  deleting the panel while it is zoomed leaves no font of its own.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/zoomPanel.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <cmath>
#include <set>

namespace
{

using gweni::renderer::SlotRenderer;
using gweni::renderer::TextMode;

//! The skin's 11 px OpenSans on a canvas the size of the view, with a Label in a
//! ZoomPanel filling it.
struct ZoomScene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;
    gweni::controls::ZoomPanel *panel;
    gweni::controls::Label *label;

    ZoomScene()
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);

        panel=canvas->newChild<gweni::controls::ZoomPanel>();
        panel->setDock(gweni::DockPosition::Center);
        label=panel->newChild<gweni::controls::Label>();
        label->setPos(4, 4);
        label->setText("Zoomed text 0123");
    }

    ~ZoomScene()
    {
        delete canvas;
        delete skin;

        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.begin();
        renderer.end();
    }

    void frame()
    {
        gweni::test::clearView(gweni::Color(0, 0, 0, 255));
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

//! The skin's font at the size the Label draws at under \p zoom.
gweni::Font drawnAt(float zoom)
{
    gweni::Font font=gweni::test::openSans();

    font.size=11.0f*zoom;
    return font;
}

}//namespace

TEST_CASE("Zoomed text: a zoom sweep leaves no font atlas behind")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    REQUIRE(renderer.scale() == 1.0f);

    ZoomScene scene;

    scene.frame();
    scene.frame();

    const size_t start=renderer.getLoadedFontCount();
    std::vector<float> zooms;

    // 1 -> 0.25, then 200 geometric steps 0.25 -> 4 -> 0.25, then 1
    zooms.push_back(0.25f);
    for(int i=1; i <= 100; ++i)
        zooms.push_back(0.25f*std::pow(16.0f, i/100.0f));
    for(int i=99; i >= 0; --i)
        zooms.push_back(0.25f*std::pow(16.0f, i/100.0f));
    zooms.push_back(1.0f);

    size_t peak=start;
    std::set<float> coverageSizes;
    bool sdf=false;

    for(float zoom : zooms)
    {
        scene.panel->setZoom(zoom);
        scene.frame();

        const gweni::Font font=drawnAt(scene.panel->zoom());

        if(renderer.getTextMode(font) == TextMode::Sdf)
            sdf=true;
        else
            coverageSizes.insert(SlotRenderer::coverageSize(font.size));

        CAPTURE(zoom);
        CHECK(renderer.getLoadedFontCount() <= start+1);
        peak=std::max(peak, renderer.getLoadedFontCount());
    }

    MESSAGE(coverageSizes.size(), " whole coverage sizes and ", (sdf?1:0),
        " SDF atlas passed through, at most ", peak-start, " font more loaded at once");
    CHECK(coverageSizes.size() <= 18);
    CHECK(peak <= start+coverageSizes.size()+(sdf?1:0));
    CHECK(renderer.getLoadedFontCount() == start);

    // a zoom inside the SDF range uploads nothing to an atlas
    if(renderer.getTextMode(drawnAt(2.5f)) == TextMode::Sdf)
    {
        scene.panel->setZoom(2.5f);
        scene.frame();
        scene.frame();
        scene.panel->setZoom(3.25f);
        scene.frame();
        CHECK(renderer.getFrameStats().atlasUploadCalls == 0);
        CHECK(renderer.getFrameStats().textSlotsWritten > 0);
    }

    // deleted while drawn at a size of its own: that font goes with it
    scene.panel->setZoom(1.5f);
    scene.frame();
    REQUIRE(renderer.getLoadedFontCount() == start+1);
    scene.panel->delayedDelete();
    scene.frame();
    CHECK(renderer.getLoadedFontCount() == start);
}
