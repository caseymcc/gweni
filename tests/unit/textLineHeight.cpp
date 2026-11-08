/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  How an unwrapped internal::Text sizes itself, counted on the StubRenderer.
 *  A Text with a string measures that string alone: its measureText y is the
 *  line box already, so it asks for no lineHeight() of its own, and a Label's
 *  first layout makes one measureText(), kept for both axes (textMeasure.cpp),
 *  with no " " measured. An empty Text measures no string: it asks the
 *  renderer's lineHeight(font) and is one caret (1 + padding) wide and that
 *  plus padding high (#268). Every call is in LayoutStats::measureCalls.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/label.h>
#include <gweni/controls/text.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    const controls::LayoutStats &stats() const { return canvas->getLayoutStats(); }
};

}//namespace

TEST_CASE("Text line height: a Label with a string measures it once and no \" \"")
{
    Scene scene;
    const int Labels=3;

    for(int i=0; i<Labels; ++i)
    {
        controls::Label *label=scene.canvas->newChild<controls::Label>();

        label->setText("label");
        label->setPos(0, i*30);
    }

    const size_t measures=scene.renderer.measures;
    const size_t lineHeights=scene.renderer.lineHeights;

    scene.frame();

    REQUIRE(scene.stats().passes == 1);
    // a refreshSize in X and in Y for each Text, the second one kept
    CHECK(scene.renderer.measures-measures == Labels);
    CHECK(scene.renderer.lineHeights-lineHeights == 0);
    CHECK(scene.stats().measureCalls == Labels);
}

TEST_CASE("Text line height: an empty Text asks for lineHeight() and is that high plus padding")
{
    Scene scene;
    controls::internal::Text *text=scene.canvas->newChild<controls::internal::Text>();

    text->setPadding(Padding(2, 3, 4, 5));
    text->setPos(10, 10);

    const size_t measures=scene.renderer.measures;
    const size_t lineHeights=scene.renderer.lineHeights;

    scene.frame();

    const size_t asked=scene.renderer.lineHeights-lineHeights;

    CHECK(asked > 0);
    // the stub's lineHeight() is one measureText(" "), and nothing else is measured
    CHECK(scene.renderer.measures-measures == asked);
    CHECK(scene.stats().measureCalls == asked);

    const int lineHeight=scene.renderer.lineHeight(text->getFont());

    REQUIRE(lineHeight > 0);
    CHECK(text->getWidth() == 1+2+4);
    CHECK(text->getHeight() == lineHeight+3+5);

    // the first character typed keeps the height; the width is the string's
    text->setText("x");
    scene.frame();
    CHECK(text->getHeight() == lineHeight+3+5);
    CHECK(text->getWidth() == scene.renderer.measureText(text->getFont(), "x").x+2+4);

    text->setText("");
    scene.frame();
    CHECK(text->getWidth() == 1+2+4);
    CHECK(text->getHeight() == lineHeight+3+5);
}
