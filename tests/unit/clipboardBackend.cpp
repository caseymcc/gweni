/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The clipboard backend (platform::setClipboardBackend) and the deferred
 *  paste (platform::requestClipboardText), on the stub fixture with the keys
 *  through the GLFW translator. With no backend the clipboard is the
 *  in-process buffer and a request is answered inside the call, so Ctrl+V
 *  pastes in the keystroke, as before the backend existed. With a fake
 *  synchronous backend, a TextBox's and a CodeEditor's Ctrl+C, Ctrl+X and
 *  Ctrl+V go through it and not the buffer. With a fake deferred one (a
 *  request held and answered later, as the browser's paste event does),
 *  Ctrl+V changes nothing until the answer, which then pastes at the caret
 *  as one undo step; an answer for a TextBox or a CodeEditor deleted before
 *  it arrives does nothing (the reply looks the control up by id,
 *  controls::Base::getControlById(), which is null once it is deleted: a
 *  captured pointer would be a use after free, which the ASAN tree
 *  reports); and empty functions give the in-process clipboard back, which
 *  every case does on the way out, the platform state being process-global.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

const String eAcute="\xc3\xa9";         // é

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    input::GLFW3 glfw;
    float now=100.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return now; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        glfw.initialize(canvas);
    }

    ~Scene()
    {
        // The focus, the key state and the clipboard are process-global.
        glfw.processFocus(false);
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
        platform::setClipboardBackend(nullptr, nullptr, nullptr);
    }

    void frame()
    {
        now+=1.0f/60.0f;
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void glfwKey(int key, int action, int mods)
    {
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, action, mods});
    }

    //! Ctrl and a key, pressed and released as GLFW reports them, then a frame
    void ctrl(int key)
    {
        glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, GLFW_MOD_CONTROL);
        glfwKey(key, GLFW_PRESS, GLFW_MOD_CONTROL);
        glfwKey(key, GLFW_RELEASE, GLFW_MOD_CONTROL);
        glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE, 0);
        frame();
    }

    template<typename T>
    T *focused(const String &text)
    {
        T *control=canvas->newChild<T>("box");
        control->setSize(300, 200);
        control->setText(text);
        frame();
        control->focus();
        frame();
        return control;
    }
};

//! A system clipboard that answers at once
struct SyncClipboard
{
    String text;
    int gets=0;
    int sets=0;

    void install()
    {
        platform::setClipboardBackend(
            [this]() { ++gets; return text; },
            [this](const String &value) { ++sets; text=value; return true; });
    }
};

//! A system clipboard that answers a read later, when answer() is called
struct DeferredClipboard
{
    String text;
    std::vector<platform::ClipboardReply> pending;

    void install()
    {
        platform::setClipboardBackend(
            [this]() { return text; },
            [this](const String &value) { text=value; return true; },
            [this](platform::ClipboardReply reply) { pending.push_back(std::move(reply)); });
    }

    void answer()
    {
        std::vector<platform::ClipboardReply> replies;
        replies.swap(pending);

        for(platform::ClipboardReply &reply : replies)
            reply(text);
    }
};

}//namespace

TEST_CASE("Clipboard: with no backend it is the in-process buffer, a request answered inside the call")
{
    Scene scene;

    platform::setClipboardText("buffer"+eAcute);
    CHECK(platform::getClipboardText() == "buffer"+eAcute);

    String answered;
    bool called=false;
    platform::requestClipboardText([&](const String &text) { called=true; answered=text; });
    CHECK(called);
    CHECK(answered == "buffer"+eAcute);

    // Ctrl+V pastes in the keystroke, before any frame
    controls::TextBox *box=scene.focused<controls::TextBox>("ab");
    box->moveCaretToend();
    scene.glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, GLFW_MOD_CONTROL);
    scene.glfwKey(GLFW_KEY_V, GLFW_PRESS, GLFW_MOD_CONTROL);
    CHECK(box->getText() == "abbuffer"+eAcute);
    scene.glfwKey(GLFW_KEY_V, GLFW_RELEASE, GLFW_MOD_CONTROL);
    scene.glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE, 0);
}

TEST_CASE("Clipboard: a TextBox's copy, cut and paste go through an installed backend")
{
    Scene scene;
    SyncClipboard system;
    platform::setClipboardText("in-process");
    system.text="system"+eAcute;
    system.install();

    controls::TextBox *box=scene.focused<controls::TextBox>("hello");
    box->moveCaretToend();
    scene.ctrl(GLFW_KEY_V);
    CHECK(box->getText() == "hellosystem"+eAcute);
    CHECK(system.gets == 1);

    scene.ctrl(GLFW_KEY_A);
    scene.ctrl(GLFW_KEY_C);
    CHECK(system.text == "hellosystem"+eAcute);
    CHECK(system.sets == 1);

    box->setText("cut me");
    scene.ctrl(GLFW_KEY_A);
    scene.ctrl(GLFW_KEY_X);
    CHECK(system.text == "cut me");
    CHECK(system.sets == 2);
    CHECK(box->getText() == "");

    // the buffer was never read or written
    platform::setClipboardBackend(nullptr, nullptr, nullptr);
    CHECK(platform::getClipboardText() == "in-process");
}

TEST_CASE("Clipboard: a CodeEditor's copy, cut and paste go through an installed backend")
{
    Scene scene;
    SyncClipboard system;
    platform::setClipboardText("in-process");
    system.text="sys\n"+eAcute;
    system.install();

    controls::CodeEditor *editor=scene.focused<controls::CodeEditor>("one");
    editor->setCaret(text::Position{0, 3});
    scene.ctrl(GLFW_KEY_V);
    CHECK(editor->getText() == "onesys\n"+eAcute);

    scene.ctrl(GLFW_KEY_A);
    scene.ctrl(GLFW_KEY_C);
    CHECK(system.text == "onesys\n"+eAcute);
    scene.ctrl(GLFW_KEY_X);
    CHECK(system.sets == 2);
    CHECK(editor->getText() == "");

    platform::setClipboardBackend(nullptr, nullptr, nullptr);
    CHECK(platform::getClipboardText() == "in-process");
}

TEST_CASE("Clipboard: a deferred paste changes nothing until it is answered, then pastes as one step")
{
    Scene scene;
    DeferredClipboard system;
    system.install();

    controls::TextBox *box=scene.focused<controls::TextBox>("");
    box->setText("ab");
    box->moveCaretToend();
    scene.frame();
    const size_t steps=box->getUndoStack().undoCount();

    scene.ctrl(GLFW_KEY_V);
    CHECK(system.pending.size() == 1);
    CHECK(box->getText() == "ab");

    // the clipboard changed between the key and the answer: the answer's text
    system.text="later"+eAcute;
    system.answer();
    CHECK(box->getText() == "ablater"+eAcute);
    CHECK(box->getUndoStack().undoCount() == steps+1);

    scene.frame();
    scene.ctrl(GLFW_KEY_Z);
    CHECK(box->getText() == "ab");
}

TEST_CASE("Clipboard: a deferred paste answered after its TextBox was deleted does nothing")
{
    Scene scene;
    DeferredClipboard system;
    system.text="late";
    system.install();

    controls::TextBox *box=scene.focused<controls::TextBox>("gone");
    const size_t id=box->getControlId();
    CHECK(controls::Base::getControlById(id) == box);

    scene.ctrl(GLFW_KEY_V);
    REQUIRE(system.pending.size() == 1);

    delete box;
    CHECK(controls::Base::getControlById(id) == nullptr);

    // a new box (a new id: ids are never reused) gets nothing either
    controls::TextBox *other=scene.focused<controls::TextBox>("other");
    CHECK(other->getControlId() != id);
    system.answer();
    CHECK(other->getText() == "other");
}

TEST_CASE("Clipboard: a deferred paste answered after its CodeEditor was deleted does nothing")
{
    Scene scene;
    DeferredClipboard system;
    system.text="late";
    system.install();

    controls::CodeEditor *editor=scene.focused<controls::CodeEditor>("gone");
    scene.ctrl(GLFW_KEY_V);
    REQUIRE(system.pending.size() == 1);

    delete editor;
    controls::CodeEditor *other=scene.focused<controls::CodeEditor>("other");
    system.answer();
    CHECK(other->getText() == "other");
}

TEST_CASE("Clipboard: empty functions give the in-process clipboard back")
{
    Scene scene;
    platform::setClipboardText("kept");

    SyncClipboard system;
    system.text="system";
    system.install();
    CHECK(platform::getClipboardText() == "system");
    CHECK(platform::setClipboardText("written"));
    CHECK(system.text == "written");

    platform::setClipboardBackend(nullptr, nullptr, nullptr);
    CHECK(platform::getClipboardText() == "kept");

    String answered;
    platform::requestClipboardText([&](const String &text) { answered=text; });
    CHECK(answered == "kept");
}
