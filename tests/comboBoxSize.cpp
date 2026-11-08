/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  ComboBox's content size, measured with the real renderer: a Shrink box is
 *  its selected text, the text padding and the chevron's column wide, so the
 *  chevron the textured skin paints at the right never covers the text; it
 *  is a line high plus its padding with or without an item; its hidden menu
 *  (however wide its items) is not counted; Fixed takes the requested width.
 *  The first item added is selected, and Up/Down step the selection.
 *
 *  The chevron's column is ComboBox::ChevronSize+ChevronInset, kept as the
 *  box's right padding, and the glyphs end before it. Up/Down also fire
 *  onSelection.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/comboBox.h>
#include <gweni/skins/textured/texturedSkin.h>

namespace
{

using gweni::controls::ComboBox;

//! A textured-skin canvas over the whole view.
struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;

    Scene()
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        // as in textRunCache.cpp: no setDefaultFont, so deleting the skin keeps test::openSans()
        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);
    }

    ~Scene()
    {
        delete canvas;
        delete skin;

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.begin();
        renderer.end();
    }

    //! Dock None, so its size is its own: they overlap, which sizes do not mind.
    ComboBox *combo()
    {
        return canvas->newChild<ComboBox>();
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

int textWidth(ComboBox *box, const gweni::String &text)
{
    return gweni::test::gpuRenderer().measureText(box->getFont(), text).x;
}

int lineHeight(ComboBox *box)
{
    return gweni::test::gpuRenderer().measureText(box->getFont(), " ").y;
}

//! The left edge of the chevron the textured skin paints, in the box.
int chevronLeft(ComboBox *box)
{
    return box->getWidth()-ComboBox::ChevronSize-ComboBox::ChevronInset;
}

}//namespace

TEST_CASE("ComboBox: a Shrink box is its text, text padding and chevron wide, and the chevron clears the text")
{
    Scene scene;
    ComboBox *box=scene.combo();

    box->addItem("Option One", "one");
    // the hidden menu, as wide as this item, takes no part in the box's size
    box->addItem("A menu item much wider than the selected one", "two");
    scene.frame();

    REQUIRE((box->getSizeFlags().horizontal == gweni::SizeFlag::Shrink));
    REQUIRE(box->getSelectedItem() != nullptr);
    CHECK(box->getText() == "Option One");

    const gweni::Padding textPadding=box->getTextPadding();
    const int text=textWidth(box, "Option One");

    CHECK(box->getWidth() == text+textPadding.left+textPadding.right+
        ComboBox::ChevronSize+ComboBox::ChevronInset);
    // the glyphs end before the chevron starts, with the text's right padding between
    CHECK(box->textX()+textPadding.left+text+textPadding.right <= chevronLeft(box));
    CHECK(box->getHeight() >= lineHeight(box)+2*ComboBox::VerticalPadding);
    CHECK(box->getHeight() >= ComboBox::ChevronSize);
}

TEST_CASE("ComboBox: an empty box is as high as one with an item, and Fixed takes the requested width")
{
    Scene scene;
    ComboBox *filled=scene.combo();
    ComboBox *empty=scene.combo();
    ComboBox *fixed=scene.combo();

    filled->addItem("Option One");
    fixed->addItem("Option One");
    filled->setSizeFlags({gweni::SizeFlag::Shrink, gweni::SizeFlag::Shrink});
    empty->setSizeFlags({gweni::SizeFlag::Shrink, gweni::SizeFlag::Shrink});
    fixed->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Shrink});
    fixed->setWidth(200);
    scene.frame();

    CHECK(empty->getText() == "");
    CHECK(empty->getHeight() == filled->getHeight());
    CHECK(empty->getHeight() >= lineHeight(empty)+2*ComboBox::VerticalPadding);
    CHECK(fixed->getWidth() == 200);
    CHECK(fixed->getHeight() == filled->getHeight());
}

TEST_CASE("ComboBox: Up and Down step the selection and fire onSelection")
{
    struct Counter: public gweni::event::Handler
    {
        int calls=0;

        void onSelection(gweni::event::Info)
        {
            calls++;
        }
    } counter;

    Scene scene;
    ComboBox *box=scene.combo();

    box->addItem("Option One", "one");
    box->addItem("Number Two", "two");
    box->addItem("Door Three", "three");
    box->onSelection.add(&counter, &Counter::onSelection);
    scene.frame();

    CHECK(box->getSelectedItem()->getName() == "one");

    box->onKeyDown(true);
    CHECK(box->getSelectedItem()->getName() == "two");
    CHECK(box->getText() == "Number Two");
    box->onKeyDown(true);
    box->onKeyDown(true);
    CHECK(box->getSelectedItem()->getName() == "three");
    box->onKeyUp(true);
    CHECK(box->getSelectedItem()->getName() == "two");
    CHECK(counter.calls == 3);
}
