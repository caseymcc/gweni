/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeEditor's undo, Tab and read-only rules, driven through the GLFW
 *  translator on the StubSkin with a stepped clock.
 *
 *  Undo: after a random sequence of edits (typing ASCII and wide and 2-byte
 *  codepoints, Return, Backspace, Delete, Tab, Shift+Tab, paste, cut, typing
 *  over a selection, caret moves, pauses past the coalescing interval),
 *  every Ctrl+Z gives back a text the editor held at some point and the last
 *  one the first text; Ctrl+Y (and Ctrl+Shift+Z) then redo as many steps,
 *  up to the last text, every row on screen showing its line. Typing coalesces into one step until the
 *  caret moves, a line break is typed or a second passes; an undo restores
 *  the caret and the selection and scrolls the caret into view; the app's own
 *  edit of the document clears the history.
 *
 *  Tab: spaces to the next multiple of the tab width in cells (a wide glyph
 *  before the caret counting 2), in place of a selection inside one line;
 *  over several lines it indents each by the tab width (a last line the
 *  selection only reaches the start of left alone), Shift+Tab outdents the
 *  caret's line or the selected lines to the tab stop before their indent (a
 *  leading tab character is one stop), each one undo step, the selection
 *  moving with the text. Tab keeps the focus in the editor; Escape then Tab
 *  (or Shift+Tab) moves it on (back), and any other key between disarms
 *  that; setTabInsertsSpaces(false) gives Tab to the focus.
 *
 *  Read-only: no caret (empty bounds, no blink animation), no current line,
 *  and typing, the editing keys, Tab, paste, cut and undo change nothing,
 *  while the moves, Shift selection, Ctrl+A and Ctrl+C work and Tab moves the
 *  focus.
 */
#include "headlessFixture.h"

#include <gweni/anim.h>
#include <gweni/controls/codeEditor.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <random>
#include <set>
#include <string>

using namespace gweni;

namespace
{

const String wideJa="\xe6\x97\xa5";     // 日, 2 cells

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::TextBox *before=nullptr;
    controls::CodeEditor *editor=nullptr;
    controls::TextBox *after=nullptr;
    input::GLFW3 glfw;
    float clock=100.0f;

    explicit Scene(const String &text)
    {
        platform::setTimeSource([this]() { return clock; });

        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 600);
        glfw.initialize(canvas);

        // tabable controls on both sides of the editor
        before=canvas->newChild<controls::TextBox>();
        before->setBounds(0, 0, 100, 20);
        editor=canvas->newChild<controls::CodeEditor>();
        editor->setBounds(0, 30, 300, 200);
        editor->setText(text);
        after=canvas->newChild<controls::TextBox>();
        after->setBounds(0, 240, 100, 20);
        frame();
        editor->focus();
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        glfw.processFocus(false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void glfwKey(int key, int action, int mods)
    {
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, action, mods});
    }

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

    void type(const std::u32string &text)
    {
        for(char32_t c : text)
        {
            glfw.processCharEvent(static_cast<unsigned int>(c));
            frame();
        }
    }

    void undo() { key(GLFW_KEY_Z, GLFW_MOD_CONTROL); }
    void redo(bool shiftZ=false)
    {
        if(shiftZ)
            key(GLFW_KEY_Z, GLFW_MOD_CONTROL|GLFW_MOD_SHIFT);
        else
            key(GLFW_KEY_Y, GLFW_MOD_CONTROL);
    }

    text::Selection selection() const { return text::Selection{editor->getAnchor(), editor->getCaret()}; }
    String line(size_t i) const { return editor->getDocument().line(i); }
};

text::Position at(size_t line, size_t byte) { return text::Position{line, byte}; }

}//namespace

namespace doctest
{
template<> struct StringMaker<text::Position>
{
    static String convert(const text::Position &p)
    {
        return ("("+std::to_string(p.line)+", "+std::to_string(p.byte)+")").c_str();
    }
};
}

TEST_CASE("CodeEditor undo: a random edit sequence undoes to the first text and redoes to the last")
{
    for(unsigned seed : {1u, 2u, 3u, 4u, 5u})
    {
        INFO("seed ", seed);
        Scene s("alpha: 1\n  beta: two\n    gamma: [3, 4]\nend");
        controls::CodeEditor *editor=s.editor;
        std::mt19937 random(seed);
        auto roll=[&](unsigned n) { return static_cast<unsigned>(random()%n); };
        auto anywhere=[&]()
            {
                const size_t line=roll(static_cast<unsigned>(editor->getLineCount()));
                return at(line, roll(static_cast<unsigned>(s.line(line).size()+1)));
            };

        const String first=editor->getText();
        std::set<String> seen{first};

        platform::setClipboardText("pasted\ntext");

        for(int i=0; i < 120; ++i)
        {
            switch(roll(12))
            {
            case 0: case 1: case 2: s.type(U"xy"); break;
            case 3: s.type(U"é日"); break;
            case 4: s.key(GLFW_KEY_ENTER); break;
            case 5: s.key(GLFW_KEY_BACKSPACE); break;
            case 6: s.key(GLFW_KEY_DELETE); break;
            case 7: s.key(GLFW_KEY_TAB, roll(2)?GLFW_MOD_SHIFT:0); break;
            case 8: s.key(GLFW_KEY_V, GLFW_MOD_CONTROL); break;
            case 9: editor->setCaret(anywhere()); editor->setCaret(anywhere(), true); s.frame();
                if(roll(2)) s.key(GLFW_KEY_X, GLFW_MOD_CONTROL); else s.type(U"z");
                break;
            case 10: editor->setCaret(anywhere()); s.frame(); break;
            default: s.clock+=1.5f; break;
            }
            seen.insert(editor->getText());
        }

        const String last=editor->getText();
        size_t undone=0;

        while(editor->getUndoStack().canUndo())
        {
            s.undo();
            ++undone;
            CHECK(seen.count(editor->getText()) == 1);
            REQUIRE(undone < 1000);
        }
        CHECK(editor->getText() == first);
        // nothing more to undo
        s.undo();
        CHECK(editor->getText() == first);

        size_t redone=0;

        while(editor->getUndoStack().canRedo())
        {
            s.redo(redone%2 == 1);
            ++redone;
            CHECK(seen.count(editor->getText()) == 1);
        }
        CHECK(redone == undone);
        CHECK(editor->getText() == last);
        // every row on screen shows its line
        for(controls::Base *row : editor->getRows())
        {
            controls::CodeLine *line=gweni_cast<controls::CodeLine>(row);

            if(!line->hidden())
                CHECK(line->getText() == s.line(line->getLine()));
        }
    }
}

TEST_CASE("CodeEditor undo: typing coalesces until the caret moves, a line break or a pause")
{
    Scene s("");
    controls::CodeEditor *editor=s.editor;

    s.type(U"abc");
    s.type(U"de");
    CHECK(editor->getUndoStack().undoCount() == 1);
    // a line break starts a step, which the line typed after it joins
    s.key(GLFW_KEY_ENTER);
    s.type(U"fg");
    CHECK(editor->getUndoStack().undoCount() == 2);
    // a move starts a new step
    s.key(GLFW_KEY_LEFT);
    s.type(U"h");
    CHECK(editor->getUndoStack().undoCount() == 3);
    // a pause past a second too
    s.clock+=1.5f;
    s.type(U"i");
    CHECK(editor->getUndoStack().undoCount() == 4);
    // Backspace is a step of another kind
    s.key(GLFW_KEY_BACKSPACE);
    s.key(GLFW_KEY_BACKSPACE);
    CHECK(editor->getUndoStack().undoCount() == 5);
    CHECK(editor->getText() == "abcde\nfg");

    s.undo();
    CHECK(editor->getText() == "abcde\nfhig");
    CHECK(editor->getCaret() == at(1, 3));
    s.undo();
    s.undo();
    CHECK(editor->getText() == "abcde\nfg");
    s.undo();
    CHECK(editor->getText() == "abcde");
    s.undo();
    CHECK(editor->getText() == "");
    CHECK(!editor->getUndoStack().canUndo());
}

TEST_CASE("CodeEditor undo: it restores the selection and scrolls the caret into view, and an app edit clears it")
{
    String text;
    for(int i=0; i < 300; ++i)
        text+="line "+std::to_string(i)+"\n";
    Scene s(text);
    controls::CodeEditor *editor=s.editor;

    editor->setCaret(at(5, 0));
    editor->setCaret(at(7, 4), true);
    s.frame();
    s.type(U"Q");
    CHECK(s.line(5) == "Q 7");

    editor->setVerticalScrollOffset(200*editor->getLineHeight());
    s.frame();
    s.undo();
    CHECK(s.line(5) == "line 5");
    CHECK(editor->getAnchor() == at(5, 0));
    CHECK(editor->getCaret() == at(7, 4));
    CHECK(editor->getSelectedText() == "line 5\nline 6\nline");
    // the caret's line in view
    const int top=7*editor->getLineHeight();
    CHECK(editor->getVerticalScrollOffset() <= top);
    CHECK(editor->getVerticalScrollOffset()+editor->getViewportHeight() >= top+editor->getLineHeight());

    s.redo();
    CHECK(s.line(5) == "Q 7");
    CHECK(editor->getCaret() == at(5, 1));

    // the app edits the document: the history no longer fits it
    editor->getDocument().insert(at(0, 0), "app ");
    CHECK(!editor->getUndoStack().canUndo());
    CHECK(!editor->getUndoStack().canRedo());
    s.undo();
    CHECK(s.line(0) == "app line 0");
}

TEST_CASE("CodeEditor Tab: spaces to the next tab stop in cells, in place of a selection on one line")
{
    Scene s("ab\n"+wideJa+"x\nabcdef");
    controls::CodeEditor *editor=s.editor;

    editor->setCaret(at(0, 2));
    s.key(GLFW_KEY_TAB);
    CHECK(s.line(0) == "ab  ");
    s.key(GLFW_KEY_TAB);
    CHECK(s.line(0) == "ab      ");
    CHECK(KeyboardFocus == editor);

    // after 日x (3 cells): one space to column 4
    editor->setCaret(at(1, 4));
    s.key(GLFW_KEY_TAB);
    CHECK(s.line(1) == wideJa+"x ");
    CHECK(editor->getCaretColumn() == 4);

    // a selection inside one line: replaced, from its start's column (1)
    editor->setTabWidth(8);
    editor->setCaret(at(2, 1));
    editor->setCaret(at(2, 4), true);
    s.key(GLFW_KEY_TAB);
    CHECK(s.line(2) == "a       ef");
    CHECK(editor->getTabWidth() == 8);
}

TEST_CASE("CodeEditor Tab: over several lines it indents them, Shift+Tab outdents, each one undo step")
{
    Scene s("one\n  two\n\tthree\nfour\nfive");
    controls::CodeEditor *editor=s.editor;

    // from line 0 to the start of line 3: lines 0..2
    editor->setCaret(at(0, 1));
    editor->setCaret(at(3, 0), true);
    s.key(GLFW_KEY_TAB);
    CHECK(s.line(0) == "    one");
    CHECK(s.line(1) == "      two");
    CHECK(s.line(2) == "    \tthree");
    CHECK(s.line(3) == "four");
    // the selection moved with the text
    CHECK(editor->getAnchor() == at(0, 5));
    CHECK(editor->getCaret() == at(3, 0));
    CHECK(editor->getUndoStack().undoCount() == 1);

    // six spaces go to the stop before them (4), the four before the tab go
    s.key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    CHECK(editor->getText() == "one\n    two\n\tthree\nfour\nfive");
    CHECK(editor->getAnchor() == at(0, 1));
    // again: two's indent goes to 0, the tab goes
    s.key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    CHECK(editor->getText() == "one\ntwo\nthree\nfour\nfive");
    // nothing left to take: no edit, no step
    const size_t steps=editor->getUndoStack().undoCount();
    s.key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    CHECK(editor->getUndoStack().undoCount() == steps);

    s.undo();
    CHECK(editor->getText() == "one\n    two\n\tthree\nfour\nfive");
    s.undo();
    CHECK(editor->getText() == "    one\n      two\n    \tthree\nfour\nfive");
    s.undo();
    CHECK(editor->getText() == "one\n  two\n\tthree\nfour\nfive");

    // Shift+Tab without a selection: the caret's line, 6 spaces to 4
    editor->setText("      six");
    editor->setCaret(at(0, 8));
    s.key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    CHECK(s.line(0) == "    six");
    CHECK(editor->getCaret() == at(0, 6));
}

TEST_CASE("CodeEditor Tab: Escape then Tab moves the focus, any key between disarms it")
{
    Scene s("text");
    controls::CodeEditor *editor=s.editor;

    s.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == editor);
    CHECK(s.line(0) == "    text");

    s.key(GLFW_KEY_ESCAPE);
    s.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == s.after);
    CHECK(s.line(0) == "    text");

    // back, and Escape then Shift+Tab goes back again
    s.editor->focus();
    s.frame();
    s.key(GLFW_KEY_ESCAPE);
    s.key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    CHECK(KeyboardFocus == s.before);

    // another key between: Tab edits again
    s.editor->focus();
    s.frame();
    s.key(GLFW_KEY_ESCAPE);
    s.key(GLFW_KEY_LEFT);
    s.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == editor);
    // a typed character disarms it too
    s.key(GLFW_KEY_ESCAPE);
    s.type(U"k");
    s.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == editor);
    // Escape arms one Tab only
    s.key(GLFW_KEY_ESCAPE);
    s.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == s.after);
    s.editor->focus();
    s.frame();
    s.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == editor);

    // setTabInsertsSpaces(false): Tab is the focus's
    const String text=editor->getText();
    editor->setTabInsertsSpaces(false);
    s.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == s.after);
    CHECK(editor->getText() == text);
}

TEST_CASE("CodeEditor read-only: no caret, no edits, while selection, Ctrl+A and Ctrl+C work")
{
    Scene s("first\nsecond");
    controls::CodeEditor *editor=s.editor;

    s.type(U"x");
    editor->setReadOnly(true);
    s.frame();
    CHECK(editor->isReadOnly());
    CHECK(editor->getCaretBounds().w == 0);
    CHECK(anim::activeCount(editor) == 0);
    for(controls::Base *row : editor->getRows())
        CHECK(!gweni_cast<controls::CodeLine>(row)->isCurrentLine());

    const String text=editor->getText();

    platform::setClipboardText("clip");
    s.type(U"abc");
    s.key(GLFW_KEY_ENTER);
    s.key(GLFW_KEY_BACKSPACE);
    s.key(GLFW_KEY_DELETE);
    s.key(GLFW_KEY_V, GLFW_MOD_CONTROL);
    s.undo();
    CHECK(editor->getText() == text);
    CHECK(!editor->undo());

    // moves and the selection
    editor->setCaret(at(1, 0));
    s.key(GLFW_KEY_END, GLFW_MOD_SHIFT);
    CHECK(editor->getSelectedText() == "second");
    s.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == "second");
    s.key(GLFW_KEY_X, GLFW_MOD_CONTROL);
    CHECK(editor->getText() == text);
    s.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
    CHECK(editor->getSelectedText() == text);
    s.key(GLFW_KEY_TAB);
    CHECK(editor->getText() == text);
    CHECK(KeyboardFocus == s.after);

    // editable again: the caret, the blink and the history are back
    editor->setReadOnly(false);
    editor->focus();
    s.frame();
    CHECK(anim::activeCount(editor) == 1);
    editor->setCaret(at(0, 0));
    s.frame();
    CHECK(editor->getCaretBounds().w == 1);
    s.undo();
    CHECK(s.line(0) == "first");
}
