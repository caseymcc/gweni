/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  What a canvas's input calls return, for an app drawing gweni over its own
 *  scene (#687): false is an event the UI did not take, which the app hands
 *  to its scene. On a real Canvas with a StubRenderer, a Button, a TextBox
 *  and a ScrollControl whose content overflows:
 *  - over the bare canvas every call returns false: a move, a press and its
 *    release, the wheel on both axes, the cursor leaving; and with no
 *    keyboard focus a key (one with a gweni code and one without) and a
 *    character; pointerOverControl() and holdsKeyboard() are false;
 *  - over a control a move, a press and its release return true, and the
 *    wheel over the ScrollControl (which scrolls) returns true;
 *  - with the TextBox focused a key and a character return true, even one
 *    the focus does not use (Up in a single-line box, a letter's key event);
 *  - a press on the bare canvas drops the keyboard focus the TextBox took
 *    from a click, and the keys after it return false;
 *  - with a modal up a move and a press on the bare canvas return true (the
 *    modal blocks the scene) and the press leaves the focus in the modal;
 *  - through input::GLFW3, a key's OS repeats are taken exactly when its
 *    press was: with the TextBox focused, not over the bare canvas;
 *  - a mouse release gets its press's answer wherever it lands: a press on
 *    the bare canvas released over the Button is false (and leaves the
 *    Button unpressed), its moves over the Button false too, and a press on
 *    the Button released over the bare canvas true, its moves there true
 *    (the right button on the ScrollControl, which pins no mouse focus, too);
 *  - through input::GLFW3, a key's release gets its press's answer: pressed
 *    with no focus and released after the TextBox took it, false; pressed
 *    in the TextBox and released after the focus went, true;
 *  - the middle button over a control is taken (true, the press focusing
 *    the TextBox), over the bare canvas not;
 *  - under a modal with no keyboard focus, a character is taken;
 *  - on a hidden canvas pointerOverControl() and holdsKeyboard() are false,
 *    with a stale hover and with a modal up, and a move returns false; with
 *    an input capture set they and the move are true;
 *  - a capture set during a press the scene got takes the moves (true, the
 *    capture hearing each), and the press's release is still the scene's;
 *  - a modal opened during a press the scene got leaves its moves and
 *    release the scene's (false), and the next press is the modal's (true);
 *  - a press on the bare canvas that closes an open menu or popup is taken,
 *    its release too, and the next one is the scene's.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Button *button=nullptr;
    controls::TextBox *text=nullptr;
    controls::ScrollControl *scroll=nullptr;
    double clock=10.0;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        button=canvas->newChild<controls::Button>();
        button->setText("Button");
        place(button, 10, 10, 80, 20);

        text=canvas->newChild<controls::TextBox>();
        text->setText("text");
        place(text, 10, 50, 120, 20);

        scroll=canvas->newChild<controls::ScrollControl>();
        place(scroll, 200, 10, 100, 60);
        scroll->setScroll(false, true);
        controls::Base *content=scroll->newChild<controls::Base>();
        place(content, 0, 0, 80, 400);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        canvas->inputMouseButton(0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    static void place(controls::Base *control, int x, int y, int w, int h)
    {
        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(x, y, w, h);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Moves there and presses, a second after the last press so no two
    //! are a double click; whether the press and the release were taken.
    bool click(int x, int y)
    {
        clock+=1.0;
        canvas->inputMouseMoved(x, y, 0, 0);
        const bool press=canvas->inputMouseButton(0, true);
        const bool release=canvas->inputMouseButton(0, false);
        frame();
        CHECK(press == release);
        return press;
    }
};

// the bare canvas: right of the Button, under the ScrollControl
constexpr int BareX=350, BareY=250;

//! Counts the moves it is given.
struct CountingCapture: controls::InputCapture
{
    int moves=0;

    void captureMouseMoved(int, int, int, int) override { ++moves; }
};

}//namespace

TEST_CASE("Input pass-through: every input call returns false over the bare canvas and true over a control")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    SUBCASE("the bare canvas")
    {
        CHECK_FALSE(canvas->inputMouseMoved(BareX, BareY, 0, 0));
        CHECK(HoveredControl == static_cast<controls::Base *>(canvas));
        CHECK_FALSE(canvas->pointerOverControl());
        CHECK_FALSE(canvas->inputMouseButton(0, true));
        CHECK_FALSE(canvas->inputMouseButton(0, false));
        CHECK_FALSE(canvas->inputMouseButton(1, true));
        CHECK_FALSE(canvas->inputMouseButton(1, false));
        CHECK_FALSE(canvas->inputMouseWheel(-60));
        CHECK_FALSE(canvas->inputMouseWheelH(-60));
        CHECK_FALSE(canvas->inputMouseLeave());
    }

    SUBCASE("a control")
    {
        CHECK(canvas->inputMouseMoved(20, 15, 0, 0));
        CHECK(HoveredControl == static_cast<controls::Base *>(scene.button));
        CHECK(canvas->pointerOverControl());
        CHECK(canvas->inputMouseButton(0, true));
        CHECK(canvas->inputMouseButton(0, false));

        CHECK(canvas->inputMouseMoved(240, 40, 0, 0));
        const int offset=scene.scroll->getVerticalScrollOffset();
        CHECK(canvas->inputMouseWheel(-60));
        scene.frame();
        CHECK(scene.scroll->getVerticalScrollOffset() != offset);
    }
}

TEST_CASE("Input pass-through: a key or character with no focused control returns false, with one true")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    REQUIRE(KeyboardFocus == nullptr);
    CHECK_FALSE(canvas->holdsKeyboard());
    CHECK_FALSE(canvas->inputModifierKey(keys::Up, true));
    CHECK_FALSE(canvas->inputModifierKey(keys::Up, false));
    CHECK_FALSE(canvas->inputModifierKey(keys::Invalid, true));
    CHECK_FALSE(canvas->inputCharacter('w'));

    scene.text->focus();
    REQUIRE(KeyboardFocus == static_cast<controls::Base *>(scene.text));
    CHECK(canvas->holdsKeyboard());
    // Up in a single-line box moves nothing, a letter's key has no gweni code
    CHECK(canvas->inputModifierKey(keys::Up, true));
    CHECK(canvas->inputModifierKey(keys::Up, false));
    CHECK(canvas->inputModifierKey(keys::Invalid, true));
    CHECK(canvas->inputCharacter('w'));
    CHECK(scene.text->getText() == "wtext");
}

TEST_CASE("Input pass-through: a click on the bare canvas drops the keyboard focus")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    CHECK(scene.click(20, 60));
    REQUIRE(KeyboardFocus == static_cast<controls::Base *>(scene.text));

    CHECK_FALSE(scene.click(BareX, BareY));
    CHECK(KeyboardFocus == nullptr);
    CHECK_FALSE(canvas->holdsKeyboard());
    CHECK_FALSE(canvas->inputModifierKey(keys::Up, true));
    CHECK_FALSE(canvas->inputModifierKey(keys::Up, false));
    CHECK_FALSE(canvas->inputCharacter('w'));
    CHECK(scene.text->getText() == "text");
}

TEST_CASE("Input pass-through: under a modal the bare canvas is taken and the focus stays in the modal")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    controls::Base *panel=canvas->newChild<controls::Base>();

    Scene::place(panel, 100, 100, 150, 80);
    panel->setMouseInputEnabled(true);
    controls::TextBox *field=panel->newChild<controls::TextBox>();
    Scene::place(field, 10, 10, 100, 20);
    canvas->pushModal(panel);
    scene.frame();
    field->focus();
    REQUIRE(KeyboardFocus == static_cast<controls::Base *>(field));

    CHECK(canvas->inputMouseMoved(BareX, BareY, 0, 0));
    CHECK(canvas->pointerOverControl());
    CHECK(scene.click(BareX, BareY));
    CHECK(KeyboardFocus == static_cast<controls::Base *>(field));
    CHECK(canvas->holdsKeyboard());

    // no focus under the modal: its text is still not the scene's
    field->unfocus();
    REQUIRE(KeyboardFocus == nullptr);
    CHECK(canvas->inputCharacter('w'));
    CHECK(canvas->inputModifierKey(keys::Up, true));
    CHECK(canvas->inputModifierKey(keys::Up, false));

    canvas->popModal(panel);
    scene.frame();
}

TEST_CASE("Input pass-through: GLFW3 reports a key's repeats taken exactly when its press was")
{
    Scene scene;
    input::GLFW3 glfw;

    glfw.initialize(scene.canvas);

    // no focus: the press, its repeats and its release are the scene's
    CHECK_FALSE(glfw.processKeyEvent({GLFW_KEY_W, 0, GLFW_PRESS, 0}));
    CHECK_FALSE(glfw.processKeyEvent({GLFW_KEY_W, 0, GLFW_REPEAT, 0}));
    CHECK_FALSE(glfw.processKeyEvent({GLFW_KEY_W, 0, GLFW_RELEASE, 0}));
    CHECK_FALSE(glfw.processKeyEvent({GLFW_KEY_UP, 0, GLFW_PRESS, 0}));
    CHECK_FALSE(glfw.processKeyEvent({GLFW_KEY_UP, 0, GLFW_REPEAT, 0}));
    CHECK_FALSE(glfw.processKeyEvent({GLFW_KEY_UP, 0, GLFW_RELEASE, 0}));

    // the TextBox focused: all of them the UI's
    scene.text->focus();
    CHECK(glfw.processKeyEvent({GLFW_KEY_W, 0, GLFW_PRESS, 0}));
    CHECK(glfw.processKeyEvent({GLFW_KEY_W, 0, GLFW_REPEAT, 0}));
    CHECK(glfw.processKeyEvent({GLFW_KEY_W, 0, GLFW_RELEASE, 0}));
    CHECK(glfw.processKeyEvent({GLFW_KEY_UP, 0, GLFW_PRESS, 0}));
    CHECK(glfw.processKeyEvent({GLFW_KEY_UP, 0, GLFW_REPEAT, 0}));
    CHECK(glfw.processKeyEvent({GLFW_KEY_UP, 0, GLFW_RELEASE, 0}));
    // a repeat after the release is the scene's: the press it follows is gone
    CHECK_FALSE(glfw.processKeyEvent({GLFW_KEY_UP, 0, GLFW_REPEAT, 0}));
}

TEST_CASE("Input pass-through: a mouse release and a held press's moves get the press's answer")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    SUBCASE("pressed on the bare canvas, released over the Button")
    {
        CHECK_FALSE(canvas->inputMouseMoved(BareX, BareY, 0, 0));
        CHECK_FALSE(canvas->inputMouseButton(0, true));
        CHECK_FALSE(canvas->inputMouseMoved(20, 15, 0, 0));
        CHECK(HoveredControl == static_cast<controls::Base *>(scene.button));
        CHECK_FALSE(scene.button->isDepressed());
        CHECK_FALSE(canvas->inputMouseButton(0, false));
        CHECK_FALSE(scene.button->isDepressed());
        // the press is over: a move over the Button is the UI's again
        CHECK(canvas->inputMouseMoved(21, 15, 0, 0));
    }

    SUBCASE("pressed on the Button, released over the bare canvas")
    {
        CHECK(canvas->inputMouseMoved(20, 15, 0, 0));
        CHECK(canvas->inputMouseButton(0, true));
        CHECK(scene.button->isDepressed());
        CHECK(canvas->inputMouseMoved(BareX, BareY, 0, 0));
        CHECK(canvas->inputMouseButton(0, false));
        CHECK_FALSE(scene.button->isDepressed());
        CHECK_FALSE(canvas->inputMouseMoved(BareX+1, BareY, 0, 0));
    }

    SUBCASE("the right button pressed on the bare canvas, released over the Button")
    {
        CHECK_FALSE(canvas->inputMouseMoved(BareX, BareY, 0, 0));
        CHECK_FALSE(canvas->inputMouseButton(1, true));
        CHECK_FALSE(canvas->inputMouseMoved(20, 15, 0, 0));
        CHECK_FALSE(canvas->inputMouseButton(1, false));
    }

    SUBCASE("the right button pressed on the ScrollControl, released over the bare canvas")
    {
        // it pins no MouseFocus (a Button would), so the hover leaves it
        CHECK(canvas->inputMouseMoved(240, 40, 0, 0));
        CHECK(canvas->inputMouseButton(1, true));
        REQUIRE(MouseFocus == nullptr);
        CHECK(canvas->inputMouseMoved(BareX, BareY, 0, 0));
        CHECK(HoveredControl == static_cast<controls::Base *>(canvas));
        CHECK(canvas->inputMouseButton(1, false));
        CHECK_FALSE(canvas->inputMouseMoved(BareX+1, BareY, 0, 0));
    }
}

TEST_CASE("Input pass-through: GLFW3 gives a key's release its press's answer")
{
    Scene scene;
    input::GLFW3 glfw;

    glfw.initialize(scene.canvas);

    // pressed for the scene, the TextBox focused before the release
    CHECK_FALSE(glfw.processKeyEvent({GLFW_KEY_SPACE, 0, GLFW_PRESS, 0}));
    scene.text->focus();
    REQUIRE(scene.canvas->holdsKeyboard());
    CHECK_FALSE(glfw.processKeyEvent({GLFW_KEY_SPACE, 0, GLFW_RELEASE, 0}));

    // pressed in the TextBox, the focus gone before the release
    CHECK(glfw.processKeyEvent({GLFW_KEY_UP, 0, GLFW_PRESS, 0}));
    scene.text->unfocus();
    REQUIRE_FALSE(scene.canvas->holdsKeyboard());
    CHECK(glfw.processKeyEvent({GLFW_KEY_UP, 0, GLFW_RELEASE, 0}));
}

TEST_CASE("Input pass-through: the middle button over a control is taken")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    CHECK_FALSE(canvas->inputMouseMoved(BareX, BareY, 0, 0));
    CHECK_FALSE(canvas->inputMouseButton(2, true));
    CHECK_FALSE(canvas->inputMouseButton(2, false));

    CHECK(canvas->inputMouseMoved(20, 60, 0, 0));
    REQUIRE(HoveredControl == static_cast<controls::Base *>(scene.text));
    CHECK(canvas->inputMouseButton(2, true));
    CHECK(KeyboardFocus == static_cast<controls::Base *>(scene.text));
    CHECK(canvas->inputMouseButton(2, false));
}

TEST_CASE("Input pass-through: a hidden canvas takes nothing but what an input capture gets")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    controls::Base *panel=canvas->newChild<controls::Base>();
    CountingCapture capture;

    Scene::place(panel, 100, 100, 150, 80);
    CHECK(canvas->inputMouseMoved(20, 15, 0, 0));
    REQUIRE(canvas->pointerOverControl());

    // the hover is stale: the canvas is hidden over the Button
    canvas->hide();
    scene.frame();
    CHECK_FALSE(canvas->inputMouseMoved(BareX, BareY, 0, 0));
    CHECK_FALSE(canvas->pointerOverControl());
    CHECK_FALSE(canvas->holdsKeyboard());

    canvas->pushModal(panel);
    CHECK_FALSE(canvas->pointerOverControl());
    CHECK_FALSE(canvas->holdsKeyboard());
    CHECK_FALSE(canvas->inputCharacter('w'));
    canvas->popModal(panel);

    canvas->setInputCapture(&capture);
    CHECK(canvas->inputMouseMoved(BareX+1, BareY, 1, 0));
    CHECK(capture.moves == 1);
    CHECK(canvas->pointerOverControl());
    CHECK(canvas->holdsKeyboard());
    canvas->setInputCapture(nullptr);

    canvas->show();
    scene.frame();
}

TEST_CASE("Input pass-through: a capture set during a scene press takes its moves, the release stays the scene's")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    CountingCapture capture;

    CHECK_FALSE(canvas->inputMouseMoved(BareX, BareY, 0, 0));
    CHECK_FALSE(canvas->inputMouseButton(0, true));
    canvas->setInputCapture(&capture);
    CHECK(canvas->inputMouseMoved(BareX+5, BareY, 5, 0));
    CHECK(canvas->inputMouseMoved(BareX+9, BareY, 4, 0));
    CHECK(capture.moves == 2);
    CHECK_FALSE(canvas->inputMouseButton(0, false));
    canvas->setInputCapture(nullptr);
}

TEST_CASE("Input pass-through: a modal opened during a scene press leaves its moves and release the scene's")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    controls::Base *panel=canvas->newChild<controls::Base>();

    Scene::place(panel, 100, 100, 150, 80);
    panel->setMouseInputEnabled(true);
    CHECK_FALSE(canvas->inputMouseMoved(BareX, BareY, 0, 0));
    CHECK_FALSE(canvas->inputMouseButton(0, true));
    canvas->pushModal(panel);
    scene.frame();
    CHECK_FALSE(canvas->inputMouseMoved(BareX-5, BareY, -5, 0));
    CHECK_FALSE(canvas->inputMouseMoved(150, 140, -195, -110));
    CHECK_FALSE(canvas->inputMouseButton(0, false));
    // new input is the modal's
    CHECK(scene.click(BareX, BareY));
    canvas->popModal(panel);
    scene.frame();
}

TEST_CASE("Input pass-through: a press on the bare canvas that closes a menu is taken")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    controls::Menu *menu=canvas->newChild<controls::Menu>();

    menu->setDeleteOnClose(false);
    menu->addItem("One");
    menu->addItem("Two");
    menu->openAt(Point(100, 120));
    scene.frame();
    REQUIRE_FALSE(menu->hidden());

    CHECK(scene.click(BareX, BareY));
    CHECK(menu->hidden());
    // nothing to close any more: the scene's
    CHECK_FALSE(scene.click(BareX, BareY));
}

TEST_CASE("Input pass-through: a press on the bare canvas that closes a popup is taken")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    controls::Popup *popup=scene.button->createPopup();

    Scene::place(popup->newChild<controls::Base>(), 0, 0, 60, 40);
    popup->open(scene.button);
    scene.frame();
    REQUIRE(canvas->shownPopup() == static_cast<controls::Base *>(popup));

    CHECK(scene.click(BareX, BareY));
    CHECK(canvas->shownPopup() == nullptr);
    CHECK_FALSE(scene.click(BareX, BareY));
}
