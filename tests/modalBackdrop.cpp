/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  What a dialog draws over the app is see-through where the theme makes it
 *  so, on the backend under test, through the textured skin (#685):
 *
 *    scrim   Canvas::setModalScrim(true) with a 160x80 dialogs::Dialog open in
 *            the middle of the canvas: every pixel left of the dialog, where
 *            Labels of 24 px text are, is the frame without the dialog
 *            blended under the ModalScrim surface (each channel within 2),
 *            the text dimmed, not cut out, and without the sorted pass
 *            (setTranslucentSort(false)) some pixel is not; the scrim turned
 *            off again gives back the frame with the dialog and no scrim,
 *            byte for byte.
 *    shadow  DialogShadow given alpha 128 (the shipped theme's is opaque):
 *            every pixel of the shadow's strip below the panel, over a Label's
 *            text, is the frame without the dialog blended under the shadow,
 *            and without the sorted pass some pixel is not.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/dialogs/dialog.h>
#include <gweni/controls/label.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <cstdlib>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;
namespace tokyonight=gweni::skin::textured::tokyonight;

struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;
    gweni::dialogs::Dialog *dialog;

    Scene()
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);

        dialog=canvas->newChild<gweni::dialogs::Dialog>();
        dialog->setTitle("Dialog");
        dialog->setDialogSize(gweni::dialogs::Extent::units(160),
            gweni::dialogs::Extent::units(80));
        dialog->setDeleteOnClose(false);
    }

    ~Scene()
    {
        delete canvas;
        delete skin;

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setTranslucentSort(true);
        renderer.begin();
        renderer.end();
    }

    gweni::controls::Label *label(const gweni::String &text, int x, int y)
    {
        gweni::controls::Label *label=canvas->newChild<gweni::controls::Label>();
        // the overload that owns its font: setFont(const Font &) keeps a pointer
        label->setFont(skin->getDefaultFont().facename, 24, false);
        label->setText(text);
        label->setPos(x, y);
        label->sizeToContents();
        return label;
    }

    std::vector<uint8_t> settle()
    {
        std::vector<uint8_t> pixels;

        for(int i=0; i < 3; ++i)
        {
            gweni::test::clearView(Color(0, 0, 0, 255));
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
        gweni::test::readView(pixels);
        return pixels;
    }
};

const uint8_t *at(const std::vector<uint8_t> &pixels, int x, int y)
{
    return &pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];
}

//! The pixels of \p rect in \p frame more than 2 off \p under blended under
//! \p over, and (in \p distinct) those of \p under unlike its first pixel
size_t badBlends(const std::vector<uint8_t> &frame, const std::vector<uint8_t> &under, const Rect &rect,
    const Color &over, size_t &distinct)
{
    const float a=over.a/255.0f;
    const uint8_t *first=at(under, rect.x, rect.y);
    size_t bad=0;

    distinct=0;
    for(int y=rect.y; y < rect.y+rect.h; ++y)
    {
        for(int x=rect.x; x < rect.x+rect.w; ++x)
        {
            const uint8_t *got=at(frame, x, y);
            const uint8_t *was=at(under, x, y);
            const uint8_t blend[3]={over.r, over.g, over.b};

            if((was[0] != first[0]) || (was[1] != first[1]) || (was[2] != first[2]))
                ++distinct;
            for(int c=0; c < 3; ++c)
            {
                const float want=blend[c]*a+was[c]*(1.0f-a);

                if(std::abs(got[c]-want) > 2.5f)
                {
                    if(bad == 0)
                        MESSAGE("first at ", x, ",", y, " channel ", c, ": got ", int(got[c]), " want ", want);
                    ++bad;
                    break;
                }
            }
        }
    }
    return bad;
}

}//namespace

TEST_CASE("Modal backdrop: the scrim dims the app under a dialog, text included, and off gives the frame back")
{
    Scene scene;

    scene.label("HHHH", 4, 8);
    scene.label("Under", 4, 60);
    scene.label("A scrim", 4, 110);

    const std::vector<uint8_t> app=scene.settle();

    scene.dialog->open();

    const std::vector<uint8_t> withDialog=scene.settle();
    const Rect bounds=scene.dialog->getBounds();
    // left of the dialog, the labels' column
    const Rect left(0, 0, bounds.x, gweni::test::viewSize().y);

    REQUIRE(bounds.x > 100);

    scene.canvas->setModalScrim(true);

    const std::vector<uint8_t> scrim=scene.settle();
    size_t distinct=0;

    CHECK(badBlends(scrim, app, left, tokyonight::ModalScrim.top, distinct) == 0);
    // the labels' text is in the region
    CHECK(distinct > 100);

    gweni::test::gpuRenderer().setTranslucentSort(false);
    CHECK(badBlends(scene.settle(), app, left, tokyonight::ModalScrim.top, distinct) > 0);
    gweni::test::gpuRenderer().setTranslucentSort(true);

    scene.canvas->setModalScrim(false);
    CHECK((scene.settle() == withDialog));
}

TEST_CASE("Modal backdrop: a translucent DialogShadow blends over the app's text")
{
    const tokyonight::Surface shipped=tokyonight::DialogShadow;

    tokyonight::DialogShadow.top.a=128;
    tokyonight::DialogShadow.bottom.a=128;
    tokyonight::DialogShadow.border.a=128;
    {
        Scene scene;

        scene.dialog->open();
        scene.settle();

        const Rect bounds=scene.dialog->getBounds();
        const int shadow=gweni::dialogs::Dialog::ShadowSize;
        // the strip below the panel, clear of the corners' arcs
        const Rect strip(bounds.x+shadow+16, bounds.y+bounds.h-shadow, bounds.w-shadow-32, shadow);

        scene.dialog->close();
        // text across the strip
        scene.label("HHHHHHHHH", bounds.x+20, bounds.y+bounds.h-20);

        const std::vector<uint8_t> app=scene.settle();

        scene.dialog->open();

        const std::vector<uint8_t> frame=scene.settle();
        size_t distinct=0;

        REQUIRE((scene.dialog->getBounds() == bounds));
        CHECK(badBlends(frame, app, strip, tokyonight::DialogShadow.top, distinct) == 0);
        CHECK(distinct > 20);

        gweni::test::gpuRenderer().setTranslucentSort(false);
        CHECK(badBlends(scene.settle(), app, strip, tokyonight::DialogShadow.top, distinct) > 0);
        gweni::test::gpuRenderer().setTranslucentSort(true);
    }
    tokyonight::DialogShadow=shipped;
}
