/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Text in a ZoomPanel is drawn at its size times the zoom, and a continuous
 *  zoom must not leave a font atlas behind per size it passed through: a
 *  grayscale coverage atlas is rasterised at the whole device px nearest
 *  (SlotRenderer::coverageSize), and a font only a use loaded (renderText at a
 *  zoomed size, with no loadFont of its own) is erased once it has had no run
 *  for SlotRenderer::kIdleFontFlushes flushes. So a Label of the skin's 11 px
 *  font in a ZoomPanel swept from zoom 1 down to 0.25, up to 4 and back to
 *  0.25 in 200 steps, then back to 1, never has more fonts loaded than at the
 *  start plus one per whole size (and the SDF atlas) passed through, and no
 *  more than at the start once kIdleFontFlushes frames have passed at 1 (an
 *  owner-less font an earlier case left at a size the sweep drew in goes the
 *  same way); a zoom
 *  inside the SDF range uploads nothing to an atlas; a zoom back to a size
 *  left a few frames before uploads nothing either; and deleting the panel
 *  while it is zoomed leaves no font of its own once those frames have
 *  passed.
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

    // what earlier cases left waiting out kIdleFontFlushes goes first
    auto settle=[&]() {
        for(uint64_t flush=0; flush <= SlotRenderer::kIdleFontFlushes; ++flush)
            scene.frame();
    };

    settle();

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

        peak=std::max(peak, renderer.getLoadedFontCount());
    }

    MESSAGE(coverageSizes.size(), " whole coverage sizes and ", (sdf?1:0),
        " SDF atlas passed through, at most ", peak-start, " font more loaded at once");
    CHECK(coverageSizes.size() <= 18);
    CHECK(peak <= start+coverageSizes.size()+(sdf?1:0));

    // the sizes left wait out kIdleFontFlushes, then go
    settle();
    // none left behind: an owner-less font an earlier case left at a size the
    // sweep drew in goes the same way
    CHECK(renderer.getLoadedFontCount() <= start);

    const size_t settled=renderer.getLoadedFontCount();

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

    scene.panel->setZoom(1.0f);
    settle();
    REQUIRE(renderer.getLoadedFontCount() == settled);

    // a size left and come back to a few frames later is not rasterised again
    scene.panel->setZoom(1.5f);
    scene.frame();
    REQUIRE(renderer.getLoadedFontCount() == settled+1);
    scene.panel->setZoom(1.0f);
    scene.frame();
    scene.frame();
    scene.panel->setZoom(1.5f);
    scene.frame();
    CHECK(renderer.getFrameStats().atlasUploadCalls == 0);
    CHECK(renderer.getLoadedFontCount() == settled+1);

    // deleted while drawn at a size of its own: that font goes after it
    scene.panel->delayedDelete();
    scene.frame();
    settle();
    CHECK(renderer.getLoadedFontCount() == settled);
}
