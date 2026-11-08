/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  An empty unwrapped Text measures one caret wide and one line box high,
 *  as high as the same Text with a string in it, at a coverage and an SDF
 *  size and at a canvas scale. It used to be the font's size high (11 for
 *  OpenSans 11, a typed string 15), so the first character typed into an
 *  empty TextBox, or the last one deleted, changed the box's height and its
 *  layout moved every sibling.
 *
 *  One caret is 1 + padding wide and the line box lineHeight() + padding high;
 *  checked at 11, 14, 24 and 36 px and at scales 1, 1.5 and 2. A TextBox keeps
 *  its height when its first character is typed and its last deleted (#268).
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/text.h>
#include <gweni/controls/textBox.h>
#include <gweni/skins/textured/texturedSkin.h>

namespace
{

using gweni::Size;
using gweni::controls::internal::Text;

//! A textured-skin canvas over the whole view, put back at scale 1 when the
//! case ends.
struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;

    Scene()
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);
    }

    ~Scene()
    {
        canvas->setScale(1.0f);
        delete canvas;
        delete skin;

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.begin();
        renderer.end();
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

gweni::Font openSansAt(float size)
{
    gweni::Font font=gweni::test::openSans();

    font.size=size;
    return font;
}

int lineHeight(const gweni::Font &font)
{
    return gweni::test::gpuRenderer().measureText(font, " ").y;
}

}//namespace

TEST_CASE("Text: an empty unwrapped Text is a caret wide and as high as a typed one, at every size and scale")
{
    const gweni::Padding padding(2, 3, 4, 5);

    // 11 and 14 below the SDF threshold (18), 24 and 36 at or above it
    for(float size : {11.0f, 14.0f, 24.0f, 36.0f})
    {
        for(float scale : {1.0f, 1.5f, 2.0f})
        {
            CAPTURE(size);
            CAPTURE(scale);

            Scene scene;
            scene.canvas->setScale(scale);

            const gweni::Font font=openSansAt(size);
            Text *text=scene.canvas->newChild<Text>();

            text->setFont(font);
            text->setPadding(padding);
            text->setText("");
            scene.frame();

            // measureText is in canvas units, the same at every scale
            const int line=lineHeight(font);

            REQUIRE(line > 0);
            CHECK(text->getPreferredSize().width == 1+padding.left+padding.right);
            CHECK(text->getPreferredSize().height == line+padding.top+padding.bottom);
            CHECK(text->getHeight() == line+padding.top+padding.bottom);

            text->setText("a");
            scene.frame();

            CHECK(text->getPreferredSize().height == line+padding.top+padding.bottom);
            CHECK(text->getHeight() == line+padding.top+padding.bottom);

            text->setText("");
            scene.frame();

            CHECK(text->getPreferredSize().width == 1+padding.left+padding.right);
            CHECK(text->getHeight() == line+padding.top+padding.bottom);
        }
    }
}

TEST_CASE("Text: a TextBox keeps its height when its first character is typed and when its last is deleted")
{
    Scene scene;
    gweni::controls::TextBox *box=scene.canvas->newChild<gweni::controls::TextBox>();

    box->setText("");
    scene.frame();

    const Size empty=box->getPreferredSize();
    const int emptyHeight=box->getHeight();

    box->setText("a");
    scene.frame();

    CHECK(box->getPreferredSize().height == empty.height);
    CHECK(box->getHeight() == emptyHeight);

    box->setText("");
    scene.frame();

    CHECK(box->getPreferredSize().height == empty.height);
    CHECK(box->getHeight() == emptyHeight);
}
