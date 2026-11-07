/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A TextBox and a Label start from the skin's text colour
 *  (Colors.Label.Default, which a theme's textColors.LabelDefault sets), and
 *  a colour the caller sets with setTextColor is kept through the focus,
 *  hover, redraw() and typing that re-run updateColors().
 */
#include "headlessFixture.h"

#include <gweni/controls/label.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/property/text.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

// Not white (StubSkin's default) and not any hard-coded text colour.
const Color skinText(12, 34, 56, 255);
const Color callerText(255, 0, 255, 255);

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        // Set before any control exists, as TexturedSkin::init loads the
        // theme before the canvas is built.
        skin.Colors.Label.Default=skinText;
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        // The focus and the mouse state are process-global.
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        HoveredControl=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

} // namespace

TEST_CASE("a new TextBox and Label take the skin's Label.Default text colour")
{
    Scene scene;

    controls::TextBox *box=scene.canvas->newChild<controls::TextBox>();
    controls::TextBoxNumeric *numeric=scene.canvas->newChild<controls::TextBoxNumeric>();
    controls::TextBoxMultiline *multiline=scene.canvas->newChild<controls::TextBoxMultiline>();
    controls::Label *label=scene.canvas->newChild<controls::Label>();
    controls::property::Text *property=scene.canvas->newChild<controls::property::Text>();

    multiline->setText("one\ntwo");
    scene.frame();

    CHECK((box->getTextColor() == skinText));
    CHECK((numeric->getTextColor() == skinText));
    CHECK((multiline->getTextColor() == skinText));
    CHECK((label->getTextColor() == skinText));
    CHECK((property->m_textBox->getTextColor() == skinText));
}

TEST_CASE("a TextBox keeps a caller's text colour through focus, hover, redraw and typing")
{
    Scene scene;

    controls::TextBox *box=scene.canvas->newChild<controls::TextBox>();
    box->setPos(10, 10);
    box->setText("");
    box->setTextColor(callerText);
    controls::TextBox *other=scene.canvas->newChild<controls::TextBox>();
    other->setPos(10, 40);
    scene.frame();
    REQUIRE((box->getTextColor() == callerText));

    box->focus();
    scene.frame();
    REQUIRE(box->isFocussed());
    CHECK((box->getTextColor() == callerText));

    for(const char *c="typed"; *c; ++c)
        scene.canvas->inputCharacter(static_cast<UnicodeChar>(*c));
    scene.frame();
    CHECK(box->getText() == "typed");
    CHECK((box->getTextColor() == callerText));

    box->redraw();
    scene.frame();
    CHECK((box->getTextColor() == callerText));

    // Hover on and off it.
    input::onMouseMoved(scene.canvas, 20, 15, 0, 0);
    scene.frame();
    CHECK((box->getTextColor() == callerText));
    input::onMouseMoved(scene.canvas, 300, 250, 280, 235);
    scene.frame();
    CHECK((box->getTextColor() == callerText));

    // The focus moving to the other box.
    other->focus();
    scene.frame();
    REQUIRE_FALSE(box->isFocussed());
    CHECK((box->getTextColor() == callerText));
    CHECK((other->getTextColor() == skinText));
}
