/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  TextBox::setPlaceholder: the hint is shown while the box is empty and does
 *  not have the keyboard focus, and hidden otherwise. Hiding and showing it
 *  only toggles its visibility (its skin Control keeps its ids, nothing is
 *  laid out again), it sits where the typed text will, it is not content
 *  (getText(), the cursor, the length limit and the box's size do not see
 *  it), and a PasswordTextBox shows it unmasked.
 *
 *  In detail: hidden by the focus, by typed text (after the focus has gone)
 *  and by setText, back when the box is cleared and blurred; kept hidden over
 *  text when its box is shown again, and back when a focused box is hidden
 *  (the canvas drops the focus) and shown; toggling it takes and gives back no
 *  id and does not move it; its origin is the typed text's, with a text
 *  padding; it does not widen a Shrink box and is not getValue() either; a
 *  TextBoxNumeric's is not its value, and a TextBoxMultiline's is unwrapped at
 *  its first line.
 */
#include "headlessFixture.h"

#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <string>

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
        // The focus and the key state are process-global.
        input::onKeyEvent(canvas, keys::Backspace, false);
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    template<typename T=controls::TextBox>
    T *box(const char *name, int y)
    {
        T *box=canvas->newChild<T>(name);
        box->setText("");
        box->setPos(10, y);
        return box;
    }

    void type(const char *text)
    {
        for(const char *c=text; *c; ++c)
            canvas->inputCharacter(static_cast<UnicodeChar>(*c));
        frame();
    }

    void backspace()
    {
        input::onKeyEvent(canvas, keys::Backspace, true);
        input::onKeyEvent(canvas, keys::Backspace, false);
        frame();
    }
};

} // namespace

TEST_CASE("a placeholder shows while the box is empty and unfocused")
{
    Scene scene;
    controls::TextBox *box=scene.box("box", 10);
    controls::TextBox *other=scene.box("other", 40);

    CHECK(box->getPlaceholder().empty());
    CHECK(box->getPlaceholderText() == nullptr);
    CHECK_FALSE(box->isPlaceholderShown());

    box->setPlaceholder("Search...");
    scene.frame();
    CHECK(box->getPlaceholder() == "Search...");
    CHECK(box->isPlaceholderShown());
    CHECK(box->getText().empty());

    // the focus hides it, even with nothing typed
    box->focus();
    scene.frame();
    CHECK_FALSE(box->isPlaceholderShown());

    // typed text keeps it hidden once the focus has moved on
    scene.type("a");
    CHECK(box->getText() == "a");
    other->focus();
    scene.frame();
    CHECK_FALSE(box->isPlaceholderShown());

    // cleared, it stays hidden while focused and comes back on the blur
    box->focus();
    scene.backspace();
    CHECK(box->getText().empty());
    CHECK_FALSE(box->isPlaceholderShown());
    box->unfocus();
    scene.frame();
    CHECK(box->isPlaceholderShown());
    CHECK(box->getText().empty());

    // setText from outside, unfocused
    box->setText("set");
    scene.frame();
    CHECK_FALSE(box->isPlaceholderShown());
    box->setText("");
    scene.frame();
    CHECK(box->isPlaceholderShown());

    // an empty hint is no hint
    box->setPlaceholder("");
    scene.frame();
    CHECK_FALSE(box->isPlaceholderShown());
    CHECK(box->getPlaceholder().empty());
}

TEST_CASE("a placeholder is hidden and shown with its box")
{
    Scene scene;
    controls::TextBox *box=scene.box("box", 10);
    box->setPlaceholder("hint");
    box->setText("typed");
    scene.frame();
    CHECK_FALSE(box->isPlaceholderShown());

    // showing the box gives the hint back its own state: hidden over text
    box->setHidden(true);
    scene.frame();
    box->setHidden(false);
    scene.frame();
    CHECK_FALSE(box->isPlaceholderShown());

    box->setText("");
    box->setHidden(true);
    scene.frame();
    CHECK_FALSE(box->isPlaceholderShown());
    box->setHidden(false);
    scene.frame();
    CHECK(box->isPlaceholderShown());

    // a focused box hidden loses the focus without onLostKeyboardFocus (the
    // canvas drops it): shown again, it has its hint back
    box->focus();
    scene.frame();
    CHECK_FALSE(box->isPlaceholderShown());
    box->setHidden(true);
    scene.frame();
    CHECK(KeyboardFocus == nullptr);
    box->setHidden(false);
    scene.frame();
    CHECK(box->isPlaceholderShown());
}

TEST_CASE("a placeholder toggles its visibility only")
{
    Scene scene;
    controls::TextBox *box=scene.box("box", 10);
    controls::TextBox *other=scene.box("other", 40);
    box->setPlaceholder("hint");
    scene.frame();
    scene.frame();

    const controls::internal::Text *hint=box->getPlaceholderText();
    REQUIRE(hint != nullptr);
    const Rect bounds=hint->getBounds();

    scene.renderer.events.clear();
    for(int i=0; i < 3; ++i)
    {
        box->focus();
        scene.frame();
        CHECK_FALSE(box->isPlaceholderShown());
        other->focus();
        scene.frame();
        CHECK(box->isPlaceholderShown());
        CHECK((hint->getBounds() == bounds));
    }

    // no id was given back or taken: the hint was hidden, not freed
    CHECK(scene.renderer.events.empty());
    CHECK(box->getPlaceholderText() == hint);
}

TEST_CASE("a placeholder sits where the typed text will")
{
    Scene scene;
    controls::TextBox *box=scene.box("box", 10);
    box->setTextPadding(Padding(3, 1, 3, 1));
    box->setPlaceholder("Name");
    scene.frame();

    const controls::internal::Text *hint=box->getPlaceholderText();
    REQUIRE(hint != nullptr);
    const Point hintOrigin=hint->textOrigin();
    CHECK(hint->getPadding().left == 3);

    box->setText("Name");
    scene.frame();
    CHECK_FALSE(box->isPlaceholderShown());
    CHECK(box->textX()+box->getTextPadding().left == hintOrigin.x);
    CHECK(box->textY()+box->getTextPadding().top == hintOrigin.y);
}

TEST_CASE("a placeholder is not content")
{
    Scene scene;
    controls::TextBox *box=scene.box("box", 10);
    box->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    box->setPlaceholder("a hint far wider than anything typed into this box");
    scene.frame();
    const int emptyWidth=box->getWidth();

    controls::TextBox *plain=scene.box("plain", 40);
    plain->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    scene.frame();

    // the hint does not widen the box, nor is it its text or value
    CHECK(emptyWidth == plain->getWidth());
    CHECK(box->getText().empty());
    CHECK(box->getValue().empty());
    CHECK(box->textLength() == 0);

    // the cursor and the length limit see the typed text only
    box->setMaxTextLength(3);
    box->focus();
    scene.type("abcd");
    CHECK(box->getText() == "abc");
    scene.frame();
    CHECK(box->getCaretBounds().x == box->getCharacterPosition(3).x);
}

TEST_CASE("a password box's placeholder is not masked, a numeric box's is not its value")
{
    Scene scene;
    controls::PasswordTextBox *password=scene.box<controls::PasswordTextBox>("password", 10);
    password->setPlaceholder("Password");
    scene.frame();
    REQUIRE(password->getPlaceholderText() != nullptr);
    CHECK(password->isPlaceholderShown());
    CHECK(password->getPlaceholderText()->getText() == "Password");

    password->focus();
    scene.type("pw");
    CHECK(password->getText() == "pw");
    password->unfocus();
    scene.frame();
    CHECK_FALSE(password->isPlaceholderShown());
    password->setText("", false);
    scene.frame();
    CHECK(password->isPlaceholderShown());

    controls::TextBoxNumeric *numeric=scene.box<controls::TextBoxNumeric>("numeric", 40);
    numeric->setPlaceholder("1999");
    scene.frame();
    CHECK(numeric->isPlaceholderShown());
    CHECK(numeric->getText().empty());
    CHECK(numeric->GetFloatFromText() == 0.0f);
}

TEST_CASE("a multiline box's placeholder is on its first line")
{
    Scene scene;
    controls::TextBoxMultiline *box=scene.box<controls::TextBoxMultiline>("multi", 10);
    box->setSize(200, 100);
    box->setPlaceholder("Notes");
    scene.frame();
    const controls::internal::Text *hint=box->getPlaceholderText();
    REQUIRE(hint != nullptr);
    CHECK(box->isPlaceholderShown());
    CHECK_FALSE(const_cast<controls::internal::Text *>(hint)->wrap());
    CHECK(hint->getY() == box->getPadding().top);
    CHECK(hint->getX() == box->getPadding().left);
}
