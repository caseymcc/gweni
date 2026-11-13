/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A plain SearchComboBox looks like a ComboBox, drawn by the textured skin
 *  offscreen on the backend under test at scale 1 and 2: a ComboBox and a
 *  SearchComboBox showing the same text at the same bounds draw the same
 *  pixels, byte for byte over the whole view, at rest and disabled (the
 *  shared paintComboBox: the Button surface for the state and the chevron in
 *  the right padding, the text inset as a ComboBox's). Each is drawn with the
 *  other hidden, so the two frames differ in nothing but which box draws.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/searchComboBox.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <vector>

namespace
{

using gweni::Color;

struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;

    explicit Scene(float scale)
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setScale(scale);
        canvas->setSize(static_cast<int>(viewSize.x/scale), static_cast<int>(viewSize.y/scale));
    }

    ~Scene()
    {
        delete canvas;
        delete skin;
        gweni::test::gpuRenderer().setScale(1.0f);

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.begin();
        renderer.end();
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

//! How many bytes of \p a and \p b differ.
size_t differences(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
{
    size_t count=0;

    for(size_t i=0; i < a.size() && i < b.size(); ++i)
    {
        if(a[i] != b[i])
            ++count;
    }
    return count;
}

}//namespace

TEST_CASE("SearchComboBox look: a plain box draws a ComboBox's pixels showing the same text at the same size")
{
    for(float scale : {1.0f, 2.0f})
    {
        CAPTURE(scale);

        Scene scene(scale);
        gweni::controls::ComboBox *combo=scene.canvas->newChild<gweni::controls::ComboBox>();
        gweni::controls::SearchComboBox *search=scene.canvas->newChild<gweni::controls::SearchComboBox>();

        for(gweni::controls::Base *box : {static_cast<gweni::controls::Base *>(combo),
            static_cast<gweni::controls::Base *>(search)})
        {
            box->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
            box->setBounds(30, 40, 150, 22);
        }

        combo->addItem("Option One", "one");
        combo->addItem("Two", "two");
        search->addOption("Option One", "one");
        search->addOption("Two", "two");
        REQUIRE(combo->getText() == search->getText());

        for(bool disabled : {false, true})
        {
            CAPTURE(disabled);
            combo->setDisabled(disabled);
            search->setDisabled(disabled);

            combo->show();
            search->hide();

            const std::vector<uint8_t> comboPixels=scene.settle();

            combo->hide();
            search->show();

            const std::vector<uint8_t> searchPixels=scene.settle();

            REQUIRE(comboPixels.size() == searchPixels.size());
            CHECK(differences(comboPixels, searchPixels) == 0);

            // the box drew something: not the empty canvas
            search->hide();
            CHECK(differences(scene.settle(), searchPixels) > 0);
        }
    }
}
