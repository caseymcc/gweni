/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas::setInputCapture, what tools::DepthView takes the input through, on a
 *  real Canvas with a StubRenderer: while a capture is set, every kind of event
 *  the canvas gets (a move, both buttons, both wheel axes, a key, a character, the
 *  cursor leaving) reaches the capture, in order and as it came, and nothing
 *  else: no hit test, the hovered control, the keyboard focus, the mouse focus,
 *  the mouse position and the keys held stay as they were, a Button under a
 *  click is not pressed, a focused TextBox gets no text, and the frame after
 *  draws nothing. Without one (before, and once it is cleared) the same events
 *  reach the controls as ever, the first move hit-testing from where the mouse
 *  is. A key or a button the canvas had down when the capture was set does not
 *  stay down there: a held Backspace does not repeat into the focused TextBox
 *  while the capture is set, its release goes up on the canvas as well as to
 *  the capture (so it does not repeat once the capture is gone), and a held
 *  Button's release ends its press where the canvas had the mouse, its mouse
 *  focus let go. A handler of that release that clears or replaces the capture
 *  (a button closing the depth view) is safe: neither capture gets it.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! Writes each event it gets down as a line
class Recorder: public controls::InputCapture
{
public:
    void captureMouseMoved(int x, int y, int deltaX, int deltaY) override
    {
        events.push_back("move "+std::to_string(x)+","+std::to_string(y)+" "
            +std::to_string(deltaX)+","+std::to_string(deltaY));
    }
    void captureMouseButton(int button, bool down) override
    {
        events.push_back("button "+std::to_string(button)+(down ? " down" : " up"));
    }
    void captureMouseWheel(int delta, Dim dim) override
    {
        events.push_back(std::string(dim == Dim::X?"wheel X ":"wheel Y ")+std::to_string(delta));
    }
    void captureKey(int key, bool down) override
    {
        events.push_back("key "+std::to_string(key)+(down ? " down" : " up"));
    }
    void captureCharacter(UnicodeChar chr) override
    {
        events.push_back("char "+std::to_string(chr));
    }
    void captureMouseLeave() override
    {
        events.push_back("leave");
    }

    std::vector<std::string> events;
};

struct Scene: public event::Handler
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Button *button=nullptr;
    controls::TextBox *textBox=nullptr;
    int presses=0;
    float clock=10.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(300, 200);

        button=canvas->newChild<controls::Button>();
        button->setText("Button");
        button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        button->setBounds(10, 10, 80, 20);
        button->onPressCaller.add(this, &Scene::pressed);

        textBox=canvas->newChild<controls::TextBox>();
        textBox->setText("abc");
        textBox->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        textBox->setBounds(10, 50, 120, 20);
        frame();
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        canvas->setInputCapture(nullptr);
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void pressed(event::Info) { ++presses; }

    void frame()
    {
        clock+=1.0f;
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void click(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }
};

}//namespace

TEST_CASE("Input capture: every event reaches the capture and nothing on the canvas")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    CHECK(canvas->inputCapture() == nullptr);

    // without a capture: focus the TextBox, then rest on the Button
    scene.click(20, 60);
    REQUIRE(KeyboardFocus == scene.textBox);
    canvas->inputMouseMoved(30, 20, 0, 0);
    scene.frame();
    REQUIRE(HoveredControl == scene.button);
    scene.frame();

    const Point mouse=input::getMousePosition();
    const size_t hitTests=input::hitTestVisits();

    Recorder recorder;

    canvas->setInputCapture(&recorder);
    CHECK(canvas->inputCapture() == &recorder);

    CHECK(canvas->inputMouseMoved(200, 150, 170, 130));
    CHECK(canvas->inputMouseButton(0, true));
    CHECK(canvas->inputMouseButton(0, false));
    CHECK(canvas->inputMouseButton(1, true));
    CHECK(canvas->inputMouseButton(1, false));
    CHECK(canvas->inputMouseWheel(-60));
    CHECK(canvas->inputMouseWheelH(90));
    CHECK(canvas->inputModifierKey(keys::Shift, true));
    CHECK(canvas->inputModifierKey(keys::Escape, true));
    CHECK(canvas->inputModifierKey(keys::Escape, false));
    CHECK(canvas->inputModifierKey(keys::Backspace, true));
    CHECK(canvas->inputCharacter('x'));
    CHECK(canvas->inputCharacter(0x1b));
    CHECK(canvas->inputMouseLeave());
    // back over the Button, and a click on it
    CHECK(canvas->inputMouseMoved(30, 20, -170, -130));
    CHECK(canvas->inputMouseButton(0, true));
    CHECK(canvas->inputMouseButton(0, false));

    const std::vector<std::string> expected={
        "move 200,150 170,130",
        "button 0 down", "button 0 up", "button 1 down", "button 1 up",
        "wheel Y -60", "wheel X 90",
        "key "+std::to_string(keys::Shift)+" down",
        "key "+std::to_string(keys::Escape)+" down",
        "key "+std::to_string(keys::Escape)+" up",
        "key "+std::to_string(keys::Backspace)+" down",
        "char 120", "char 27",
        "leave",
        "move 30,20 -170,-130",
        "button 0 down", "button 0 up" };

    REQUIRE(recorder.events.size() == expected.size());
    CHECK(recorder.events[5] == "wheel Y -60");
    CHECK(recorder.events[6] == "wheel X 90");
    CHECK(recorder.events == expected);

    // the update and render phases run, and nothing changed
    scene.frame();
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);

    CHECK(input::hitTestVisits() == hitTests);
    CHECK(input::getMousePosition().x == mouse.x);
    CHECK(input::getMousePosition().y == mouse.y);
    CHECK(HoveredControl == scene.button);
    CHECK(KeyboardFocus == scene.textBox);
    CHECK(MouseFocus == nullptr);
    CHECK_FALSE(input::isKeyDown(keys::Shift));
    CHECK_FALSE(input::isLeftMouseDown());
    CHECK(scene.presses == 0);
    CHECK_FALSE(scene.button->isDepressed());
    CHECK(scene.textBox->getText() == "abc");

    // cleared: the events reach the canvas again, from where the mouse was
    canvas->setInputCapture(nullptr);
    CHECK(canvas->inputCapture() == nullptr);

    canvas->inputMouseMoved(31, 20, 1, 0);
    scene.frame();
    CHECK(HoveredControl == scene.button);
    CHECK(input::hitTestVisits() > hitTests);
    scene.click(30, 20);
    CHECK(scene.presses == 1);
    CHECK(recorder.events.size() == expected.size());
}

TEST_CASE("Input capture: typed text reaches the focused TextBox only without a capture")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    Recorder recorder;

    scene.click(20, 60);
    REQUIRE(KeyboardFocus == scene.textBox);
    scene.textBox->moveCaretToend();

    canvas->setInputCapture(&recorder);
    canvas->inputCharacter('d');
    canvas->inputModifierKey(keys::Backspace, true);
    canvas->inputModifierKey(keys::Backspace, false);
    scene.frame();
    CHECK(scene.textBox->getText() == "abc");
    CHECK(recorder.events.size() == 3);

    canvas->setInputCapture(nullptr);
    canvas->inputCharacter('d');
    scene.frame();
    CHECK(scene.textBox->getText() == "abcd");
    CHECK(recorder.events.size() == 3);
}

TEST_CASE("Input capture: a key held when the capture is set does not repeat, and goes up")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    Recorder recorder;

    scene.click(20, 60);
    REQUIRE(KeyboardFocus == scene.textBox);
    scene.textBox->moveCaretToend();

    // Backspace held: its press deletes one character (a frame now would
    // repeat it, each frame being a second)
    canvas->inputModifierKey(keys::Backspace, true);
    REQUIRE(scene.textBox->getText() == "ab");
    REQUIRE(input::isKeyDown(keys::Backspace));

    // held on while the capture is set, frames well past the repeat delay
    canvas->setInputCapture(&recorder);

    for(int i=0; i < 5; ++i)
        scene.frame();

    CHECK(scene.textBox->getText() == "ab");
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);

    // its release reaches the capture and lets the key go on the canvas
    CHECK(canvas->inputModifierKey(keys::Backspace, false));
    CHECK_FALSE(input::isKeyDown(keys::Backspace));
    CHECK(recorder.events == std::vector<std::string>{
        "key "+std::to_string(keys::Backspace)+" up" });
    CHECK(KeyboardFocus == scene.textBox);

    canvas->setInputCapture(nullptr);

    for(int i=0; i < 5; ++i)
        scene.frame();

    CHECK(scene.textBox->getText() == "ab");

    // a key pressed while the capture was set was never down on the canvas
    canvas->setInputCapture(&recorder);
    canvas->inputModifierKey(keys::Delete, true);
    canvas->setInputCapture(nullptr);
    CHECK_FALSE(input::isKeyDown(keys::Delete));
    canvas->inputModifierKey(keys::Delete, false);
    scene.frame();
    CHECK(scene.textBox->getText() == "ab");
}

TEST_CASE("Input capture: a button held when the capture is set is let go by its release")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    Recorder recorder;

    canvas->inputMouseMoved(30, 20, 0, 0);
    canvas->inputMouseButton(0, true);
    scene.frame();
    REQUIRE(scene.button->isDepressed());
    REQUIRE(MouseFocus == scene.button);
    REQUIRE(input::isLeftMouseDown());

    canvas->setInputCapture(&recorder);

    // the capture's moves leave the press and the mouse where they were
    canvas->inputMouseMoved(200, 150, 170, 130);
    scene.frame();
    CHECK(scene.button->isDepressed());
    CHECK(MouseFocus == scene.button);

    // the release ends the press where the canvas has the mouse, on the Button
    CHECK(canvas->inputMouseButton(0, false));
    CHECK_FALSE(input::isLeftMouseDown());
    CHECK(MouseFocus == nullptr);
    CHECK_FALSE(scene.button->isDepressed());
    CHECK(scene.presses == 1);
    CHECK(recorder.events == std::vector<std::string>{
        "move 200,150 170,130", "button 0 up" });

    // a press and release while the capture is set reach the capture alone
    CHECK(canvas->inputMouseButton(0, true));
    CHECK(canvas->inputMouseButton(0, false));
    CHECK_FALSE(scene.button->isDepressed());
    CHECK(scene.presses == 1);
    CHECK(recorder.events.size() == 4);

    canvas->setInputCapture(nullptr);
    scene.frame();
    CHECK(HoveredControl == scene.button);
    scene.click(30, 20);
    CHECK(scene.presses == 2);
}

TEST_CASE("Input capture: a held release whose handler clears the capture gives it nothing")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    Recorder recorder;
    Recorder replacement;

    struct Closer: public event::Handler
    {
        controls::Canvas *canvas=nullptr;
        controls::InputCapture *next=nullptr;

        void pressed(event::Info) { canvas->setInputCapture(next); }
    } closer;

    closer.canvas=canvas;
    scene.button->onPressCaller.add(&closer, &Closer::pressed);

    // the press is held when the capture is set; its release clicks the
    // Button, whose handler clears the capture (as a button closing the
    // depth view deletes it)
    canvas->inputMouseMoved(30, 20, 0, 0);
    canvas->inputMouseButton(0, true);
    scene.frame();
    canvas->setInputCapture(&recorder);

    CHECK(canvas->inputMouseButton(0, false));
    CHECK(scene.presses == 1);
    CHECK(canvas->inputCapture() == nullptr);
    CHECK(recorder.events.empty());
    CHECK(MouseFocus == nullptr);

    // replaced instead: neither the old capture nor the new one gets the
    // release of a press it never saw
    closer.next=&replacement;
    canvas->inputMouseButton(0, true);
    scene.frame();
    canvas->setInputCapture(&recorder);

    CHECK(canvas->inputMouseButton(0, false));
    CHECK(scene.presses == 2);
    CHECK(canvas->inputCapture() == &replacement);
    CHECK(recorder.events.empty());
    CHECK(replacement.events.empty());
}
