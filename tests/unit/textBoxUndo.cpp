/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  TextBox undo and redo, driven through the GLFW translator (input::GLFW3,
 *  as utf8Typing.cpp does) on the stub fixture. Seeded random sequences of
 *  typed characters (one- to three-byte codepoints), Backspace, Delete, the
 *  arrows, Home and End with and without Shift, Ctrl+A, Ctrl+X and Ctrl+V,
 *  and in a TextBoxMultiline Return, Up and Down, on a clock that sometimes
 *  pauses past the stack's interval, with a few Ctrl+Z and Ctrl+Y along the
 *  way, in a TextBox, a TextBoxMultiline and a PasswordTextBox: Ctrl+Z down
 *  to the bottom gives each step's text, caret and selection from before
 *  it, ending at the empty text, and Ctrl+Y and Ctrl+Shift+Z in turn up to
 *  the top give each step's text, caret and selection from after it, ending
 *  at the final text. Then by hand: typing joins one step and a caret move
 *  starts the next; a letter typed over a selection is one step with the
 *  letters after it, and its undo gives the selection back with one
 *  onTextChanged; a paste is a step of its own; setText() from the app
 *  clears the history, but not with the text the box already has; an
 *  onTextChanged handler that sets the text during an edit (a clamp, as
 *  HSVColorPicker's number boxes do) clears the history and the edit is not
 *  recorded, and one that edits the box during an undo leaves its edit and
 *  no history (that edit drops the step being undone, whose selection the
 *  undo has copied; the stale read it avoids is not one ASAN sees, the
 *  step's bytes staying in the vector's capacity); a step whose
 *  edits do not fit the text clears the history instead of being applied,
 *  for an undo and a redo; a box that is not editable neither edits nor
 *  undoes;
 *  and Base::acceleratorKey() puts an accelerator's modifiers in the order
 *  handleAccelerator builds them, so "Shift + Ctrl + Z" is Ctrl+Shift+Z.
 */
#include "headlessFixture.h"

#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

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
        // The focus and the key state are process-global.
        glfw.processFocus(false);
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
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

    //! A key pressed and released, with the modifiers in mods held around it
    //! the way GLFW reports them: their own presses first, released last.
    void key(int key, int mods=0)
    {
        int held=0;
        if(mods & GLFW_MOD_CONTROL)
            glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, held|=GLFW_MOD_CONTROL);
        if(mods & GLFW_MOD_SHIFT)
            glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_PRESS, held|=GLFW_MOD_SHIFT);

        glfwKey(key, GLFW_PRESS, mods);
        glfwKey(key, GLFW_RELEASE, mods);

        if(mods & GLFW_MOD_SHIFT)
            glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE, held&=~GLFW_MOD_SHIFT);
        if(mods & GLFW_MOD_CONTROL)
            glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE, held&=~GLFW_MOD_CONTROL);
        frame();
    }

    void type(char32_t c)
    {
        glfw.processCharEvent(static_cast<unsigned int>(c));
        frame();
    }

    void type(const std::u32string &text)
    {
        for(char32_t c : text)
            type(c);
    }

    void undo() { key(GLFW_KEY_Z, GLFW_MOD_CONTROL); }
    //! Ctrl+Y or, with \p shiftZ, Ctrl+Shift+Z
    void redo(bool shiftZ=false)
    {
        if(shiftZ)
            key(GLFW_KEY_Z, GLFW_MOD_CONTROL|GLFW_MOD_SHIFT);
        else
            key(GLFW_KEY_Y, GLFW_MOD_CONTROL);
    }

    template<typename T>
    T *focusedBox()
    {
        T *box=canvas->newChild<T>("box");
        box->setSize(300, 200);
        box->setText("");
        frame();
        box->focus();
        frame();
        return box;
    }
};

//! A box's text, caret and selection.
struct State
{
    String text;
    int caret=0;
    int anchor=0;

    bool operator==(const State &other) const
    {
        return text == other.text && caret == other.caret && anchor == other.anchor;
    }
};

State stateOf(controls::TextBox *box)
{
    return {box->getText(), box->getCursorPos(), box->getCursorEnd()};
}

doctest::String toString(const State &state)
{
    return ("\""+state.text+"\" caret "+std::to_string(state.caret)+" anchor "+std::to_string(state.anchor)).c_str();
}

//! One undo step as the random test saw it: the state before its first
//! action and after its last.
struct Step
{
    State before;
    State after;
};

const char32_t Typed[]={U'a', U'b', U'z', U' ', U'é', U'€', U'7'};

//! Random keys and characters into \p box, then every step undone and redone
//! through the keys.
void randomUndoRedo(Scene &scene, controls::TextBox *box, bool multiline, uint32_t seed)
{
    CAPTURE(seed);
    std::mt19937 random(seed);
    std::vector<Step> steps;
    const controls::TextBox *const focused=box;
    size_t started=0;
    size_t joined=0;

    for(int action=0; action < 250; ++action)
    {
        CAPTURE(action);
        REQUIRE(KeyboardFocus == focused);

        // now and then a pause longer than the stack's interval
        if(random()%10 == 0)
            scene.now+=1.5f;

        unsigned roll=random()%100;

        if(roll < 5)
        {
            // a few steps back and some forward again; the next edit drops
            // what is left to redo
            size_t back=1+random()%3;
            for(size_t i=0; i < back && box->getUndoStack().canUndo(); ++i)
            {
                size_t index=box->getUndoStack().undoCount()-1;
                scene.undo();
                REQUIRE(stateOf(box) == steps[index].before);
            }
            size_t forward=random()%(back+1);
            for(size_t i=0; i < forward && box->getUndoStack().canRedo(); ++i)
            {
                size_t index=box->getUndoStack().undoCount();
                scene.redo(i%2 == 1);
                REQUIRE(stateOf(box) == steps[index].after);
            }
            continue;
        }

        const State before=stateOf(box);
        const size_t undoBefore=box->getUndoStack().undoCount();
        const int shift=(random()%3 == 0)?GLFW_MOD_SHIFT:0;

        if(roll < 45)
            scene.type(Typed[random()%(sizeof(Typed)/sizeof(Typed[0]))]);
        else if(roll < 55)
            scene.key(GLFW_KEY_BACKSPACE);
        else if(roll < 62)
            scene.key(GLFW_KEY_DELETE);
        else if(roll < 70)
            scene.key(GLFW_KEY_LEFT, shift);
        else if(roll < 76)
            scene.key(GLFW_KEY_RIGHT, shift);
        else if(roll < 79)
            scene.key(GLFW_KEY_HOME, shift);
        else if(roll < 82)
            scene.key(GLFW_KEY_END, shift);
        else if(roll < 84)
            scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
        else if(roll < 87)
            scene.key(GLFW_KEY_X, GLFW_MOD_CONTROL);
        else if(roll < 91)
            scene.key(GLFW_KEY_V, GLFW_MOD_CONTROL);
        else if(!multiline)
            scene.type(U'q');
        else if(roll < 95)
            scene.key(GLFW_KEY_ENTER);
        else if(roll < 98)
            scene.key(GLFW_KEY_UP, shift);
        else
            scene.key(GLFW_KEY_DOWN, shift);

        const size_t undoAfter=box->getUndoStack().undoCount();
        REQUIRE(undoAfter >= undoBefore);
        REQUIRE(undoAfter <= undoBefore+1);

        if(undoAfter == undoBefore+1)
        {
            // a new step: what could be redone is gone
            REQUIRE(box->getUndoStack().redoCount() == 0);
            steps.resize(undoBefore);
            steps.push_back({before, stateOf(box)});
            ++started;
        }
        else if(stateOf(box).text != before.text)
        {
            // the edit joined the step on top
            REQUIRE(box->getUndoStack().redoCount() == 0);
            REQUIRE(steps.size() >= undoBefore);
            steps.resize(undoBefore);
            steps.back().after=stateOf(box);
            ++joined;
        }
        else if(box->getUndoStack().redoCount() == 0 && !steps.empty() && undoAfter == steps.size())
        {
            // no edit (a caret move, a copy, a delete at an end): the step on
            // top is as it was
            REQUIRE(stateOf(box).text == steps.back().after.text);
        }
    }

    // the sequence both started steps and joined edits into them
    CAPTURE(started);
    CAPTURE(joined);
    REQUIRE(started >= 20);
    REQUIRE(joined >= 20);

    // a sequence may end on an undo, with steps left to redo
    const auto &stack=box->getUndoStack();
    REQUIRE(stack.undoCount()+stack.redoCount() == steps.size());
    const String final=steps.empty()?String():steps.back().after.text;

    while(stack.canUndo())
    {
        size_t index=stack.undoCount()-1;
        scene.undo();
        REQUIRE(stateOf(box) == steps[index].before);
    }
    REQUIRE(box->getText().empty());
    scene.undo();
    REQUIRE(box->getText().empty());

    bool shiftZ=false;
    while(stack.canRedo())
    {
        size_t index=stack.undoCount();
        scene.redo(shiftZ);
        shiftZ=!shiftZ;
        REQUIRE(stateOf(box) == steps[index].after);
    }
    REQUIRE(box->getText() == final);
    REQUIRE(KeyboardFocus == focused);
}

}//namespace

TEST_CASE("TextBox undo: random keys and characters undone to the empty text and redone to the final one")
{
    for(uint32_t seed=1; seed <= 12; ++seed)
    {
        Scene scene;
        platform::setClipboardText("paste");
        randomUndoRedo(scene, scene.focusedBox<controls::TextBox>(), false, seed);
    }
}

TEST_CASE("TextBoxMultiline undo: random keys, characters and Returns undone to the empty text and redone to the final one")
{
    for(uint32_t seed=1; seed <= 12; ++seed)
    {
        Scene scene;
        platform::setClipboardText("two\nlines");
        randomUndoRedo(scene, scene.focusedBox<controls::TextBoxMultiline>(), true, seed);
    }
}

TEST_CASE("PasswordTextBox undo: the password's edits undone and redone, the mask following")
{
    for(uint32_t seed=1; seed <= 6; ++seed)
    {
        Scene scene;
        platform::setClipboardText("secret");
        randomUndoRedo(scene, scene.focusedBox<controls::PasswordTextBox>(), false, seed);
    }
}

TEST_CASE("TextBox undo: typing is one step until the caret moves")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox<controls::TextBox>();

    scene.type(U"hello");
    CHECK(box->getUndoStack().undoCount() == 1);
    scene.key(GLFW_KEY_LEFT);
    scene.key(GLFW_KEY_LEFT);
    scene.type(U"XY");
    CHECK(box->getText() == "helXYlo");
    CHECK(box->getUndoStack().undoCount() == 2);

    scene.undo();
    CHECK((stateOf(box) == State{"hello", 3, 3}));
    scene.undo();
    CHECK((stateOf(box) == State{"", 0, 0}));
    scene.redo();
    CHECK((stateOf(box) == State{"hello", 5, 5}));
    scene.redo(true);
    CHECK((stateOf(box) == State{"helXYlo", 5, 5}));
}

TEST_CASE("TextBox undo: a letter typed over a selection is one step with the next letters, undone with one onTextChanged")
{
    struct Counter: public event::Handler
    {
        int count=0;
        void changed(event::Info) { ++count; }
    };

    Scene scene;
    controls::TextBox *box=scene.focusedBox<controls::TextBox>();
    Counter counter;
    box->onTextChangedCaller.add(&counter, &Counter::changed);

    scene.type(U"one two");
    scene.key(GLFW_KEY_LEFT, GLFW_MOD_SHIFT);
    scene.key(GLFW_KEY_LEFT, GLFW_MOD_SHIFT);
    scene.key(GLFW_KEY_LEFT, GLFW_MOD_SHIFT);
    CHECK(box->getSelection() == "two");
    scene.type(U"six");
    CHECK(box->getText() == "one six");
    CHECK(box->getUndoStack().undoCount() == 2);

    counter.count=0;
    scene.undo();
    CHECK(counter.count == 1);
    CHECK((stateOf(box) == State{"one two", 4, 7}));
    CHECK(box->getSelection() == "two");

    scene.redo();
    CHECK(counter.count == 2);
    CHECK((stateOf(box) == State{"one six", 7, 7}));
}

TEST_CASE("TextBox undo: a paste is a step of its own")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox<controls::TextBox>();

    platform::setClipboardText("CLIP");
    scene.type(U"ab");
    scene.key(GLFW_KEY_V, GLFW_MOD_CONTROL);
    scene.type(U"cd");
    CHECK(box->getText() == "abCLIPcd");
    CHECK(box->getUndoStack().undoCount() == 3);

    scene.undo();
    CHECK(box->getText() == "abCLIP");
    scene.undo();
    CHECK((stateOf(box) == State{"ab", 2, 2}));
}

TEST_CASE("TextBox undo: setText() from the app clears the history")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox<controls::TextBox>();

    scene.type(U"typed");
    REQUIRE(box->getUndoStack().canUndo());

    box->setText("from the app");
    scene.frame();
    CHECK_FALSE(box->getUndoStack().canUndo());
    scene.undo();
    CHECK(box->getText() == "from the app");

    // undone, the redo goes too
    scene.key(GLFW_KEY_END);
    scene.type(U"!");
    scene.undo();
    CHECK(box->getUndoStack().canRedo());
    box->setText("again");
    CHECK_FALSE(box->getUndoStack().canRedo());
    CHECK(box->redo() == false);
}

TEST_CASE("TextBox undo: setText() with the text the box has keeps the history")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox<controls::TextBox>();

    scene.type(U"same");
    box->setText("same");
    CHECK(box->getUndoStack().undoCount() == 1);
    scene.undo();
    CHECK(box->getText().empty());
}

TEST_CASE("TextBox undo: an onTextChanged handler that sets the text during an edit clears the history, the edit unrecorded")
{
    // as HSVColorPicker's number boxes clamp what is typed into them
    struct Clamp: public event::Handler
    {
        controls::TextBox *box=nullptr;
        void changed(event::Info)
        {
            if(box->getText().size() > 3)
                box->setText("255");
        }
    };

    Scene scene;
    controls::TextBox *box=scene.focusedBox<controls::TextBox>();
    Clamp clamp;
    clamp.box=box;
    box->onTextChangedCaller.add(&clamp, &Clamp::changed);

    scene.type(U"255");
    CHECK(box->getUndoStack().undoCount() == 1);

    scene.type(U"5");
    CHECK(box->getText() == "255");
    CHECK_FALSE(box->getUndoStack().canUndo());

    scene.key(GLFW_KEY_HOME);
    scene.type(U"9");
    CHECK(box->getText() == "255");
    CHECK_FALSE(box->getUndoStack().canUndo());
    scene.undo();
    CHECK(box->getText() == "255");

    // a Backspace the handler lets through is recorded again, on the text
    // the handler left
    scene.key(GLFW_KEY_END);
    scene.key(GLFW_KEY_BACKSPACE);
    CHECK(box->getText() == "25");
    scene.undo();
    CHECK(box->getText() == "255");
    scene.redo();
    CHECK(box->getText() == "25");
}

TEST_CASE("TextBox undo: an onTextChanged handler that edits the box during an undo leaves its edit and no history")
{
    struct Editor: public event::Handler
    {
        controls::TextBox *box=nullptr;
        bool armed=false;
        void changed(event::Info)
        {
            if(!armed)
                return;
            armed=false;
            box->insertText("!");
        }
    };

    Scene scene;
    controls::TextBox *box=scene.focusedBox<controls::TextBox>();
    Editor edit;
    edit.box=box;
    box->onTextChangedCaller.add(&edit, &Editor::changed);

    scene.type(U"abc");
    scene.key(GLFW_KEY_LEFT);
    scene.type(U"d");
    REQUIRE(box->getUndoStack().undoCount() == 2);

    // the handler's edit drops the step being undone (the one that could be
    // redone) while the undo still has to set its selection
    edit.armed=true;
    scene.undo();
    CHECK(box->getText() == "abc!");
    CHECK_FALSE(box->getUndoStack().canUndo());
    CHECK_FALSE(box->getUndoStack().canRedo());
    CHECK(box->undo() == false);
    CHECK(box->redo() == false);
}

TEST_CASE("TextBox undo: a step that does not fit the text clears the history instead of applying")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox<controls::TextBox>();

    scene.type(U"abc");

    // an edit recorded by hand against a text the box does not have
    text::Edit stale;
    stale.range={{0, 5}, {0, 5}};
    stale.text="zz";
    stale.textEnd={0, 7};
    box->getUndoStack().seal();
    box->getUndoStack().record(stale, text::EditKind::Insert, {}, {});
    REQUIRE(box->getUndoStack().undoCount() == 2);

    CHECK(box->undo() == false);
    CHECK(box->getText() == "abc");
    CHECK_FALSE(box->getUndoStack().canUndo());
    CHECK_FALSE(box->getUndoStack().canRedo());

    // the same for a redo: "abc" undone, then the text changed under it
    scene.type(U"d");
    REQUIRE(box->undo());
    box->getUndoStack().record(stale, text::EditKind::Insert, {}, {});
    box->getUndoStack().undo();
    CHECK(box->redo() == false);
    CHECK_FALSE(box->getUndoStack().canUndo());
}

TEST_CASE("TextBox undo: a box that is not editable neither edits nor undoes")
{
    Scene scene;
    controls::TextBox *box=scene.focusedBox<controls::TextBox>();

    scene.type(U"abc");
    box->setEditable(false);
    scene.type(U"d");
    scene.undo();
    CHECK(box->getText() == "abc");
    CHECK(box->getUndoStack().undoCount() == 1);

    box->setEditable(true);
    scene.undo();
    CHECK(box->getText().empty());
}

TEST_CASE("Base::acceleratorKey: upper case, no spaces, Ctrl before Shift")
{
    using controls::Base;

    CHECK(Base::acceleratorKey("Ctrl + Z") == "CTRL+Z");
    CHECK(Base::acceleratorKey("ctrl+shift+z") == "CTRL+SHIFT+Z");
    CHECK(Base::acceleratorKey("Shift + Ctrl + Z") == "CTRL+SHIFT+Z");
    CHECK(Base::acceleratorKey("Control+Y") == "CTRL+Y");
    CHECK(Base::acceleratorKey("Ctrl + +") == "CTRL++");
    CHECK(Base::acceleratorKey("+") == "+");
    CHECK(Base::acceleratorKey("A") == "A");
    // a modifier handleAccelerator never builds is left as it was written
    CHECK(Base::acceleratorKey("Alt + Shift + F4") == "ALT+SHIFT+F4");
}
