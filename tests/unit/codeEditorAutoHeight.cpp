/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeEditor's auto-height and plain mode, in a form: a panel of fields
 *  docked Top, the editor among them. With setAutoHeight(min, max) the
 *  editor is its visual rows tall, clamped to [min, max] rows, plus its
 *  padding: min rows empty, one row a wrapped or new line in between, max
 *  rows past it, where it scrolls (the vertical bar only then) with the
 *  caret's row in view and getContentRows() at max+1 (one long line too),
 *  and back down as the text shrinks. The fields below it move with it. A
 *  row more lays out in one pass that enters the editor's subtree and its
 *  ancestors and each field it moves once, none of their subtrees (20
 *  fields below cost 18 entries more than 2), and the next frame lays
 *  nothing out.
 *  setAutoHeight(1, 0) gives the requested height back. setPlain(true) is
 *  wrapped, has no gutter and no current-line look, and Tab moves the focus
 *  in and out of it as in a textarea; setPlain(false) captures Tab again.
 *  One paragraph taller than the field (a line of more visual rows than the
 *  viewport holds) keeps the caret's row in view, not the line's top: as
 *  typing grows it past the max, on idle frames after, when the field is
 *  made narrower, after a wrap toggle, and in a fixed-height editor.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>
#include <gweni/controls/label.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

struct Form
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Base *panel=nullptr;
    controls::TextBox *before=nullptr;
    controls::CodeEditor *editor=nullptr;
    std::vector<controls::Label *> below;
    controls::TextBox *after=nullptr;
    input::GLFW3 glfw;

    explicit Form(int minLines=2, int maxLines=5, size_t fieldsBelow=2)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 800);
        glfw.initialize(canvas);

        panel=canvas->newChild<controls::Base>();
        panel->setBounds(0, 0, 220, 780);

        before=panel->newChild<controls::TextBox>();
        before->setDock(DockPosition::Top);

        editor=panel->newChild<controls::CodeEditor>();
        editor->setDock(DockPosition::Top);
        editor->setPlain(true);
        editor->setAutoHeight(minLines, maxLines);

        for(size_t i=0; i < fieldsBelow; ++i)
        {
            controls::Label *label=panel->newChild<controls::Label>();

            label->setText("field "+std::to_string(i));
            label->setDock(DockPosition::Top);
            below.push_back(label);
        }

        after=panel->newChild<controls::TextBox>();
        after->setDock(DockPosition::Top);
        frame();
    }

    Form(const Form &)=delete;
    Form &operator=(const Form &)=delete;

    ~Form()
    {
        glfw.processFocus(false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    controls::LayoutStats last;

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        last=canvas->getLayoutStats();
        CHECK(last.requests == 0);
        CHECK(last.passes <= 1);
        CHECK(!last.renderLayout);
    }

    void glfwKey(int key, int action, int mods)
    {
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, action, mods});
    }

    void key(int key, int mods=0)
    {
        if(mods & GLFW_MOD_SHIFT)
            glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_PRESS, GLFW_MOD_SHIFT);
        glfwKey(key, GLFW_PRESS, mods);
        glfwKey(key, GLFW_RELEASE, mods);
        if(mods & GLFW_MOD_SHIFT)
            glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE, 0);
        frame();
    }

    void type(const String &text)
    {
        for(char c : text)
        {
            glfw.processCharEvent(static_cast<unsigned char>(c));
            frame();
        }
    }

    //! The editor's height for \p rows rows.
    int heightFor(size_t rows) const
    {
        const Padding &padding=editor->getPadding();

        return static_cast<int>(rows)*editor->getLineHeight()+padding.top+padding.bottom;
    }
};

}//namespace

TEST_CASE("CodeEditor auto-height: min rows empty, its rows in between, max rows past them where it scrolls")
{
    Form f(2, 5);
    controls::CodeEditor *editor=f.editor;
    const int lineHeight=editor->getLineHeight();

    REQUIRE(lineHeight > 0);
    REQUIRE(editor->hasAutoHeight());

    // empty: the min
    CHECK(editor->getContentRows() == 1);
    CHECK(editor->getHeight() == f.heightFor(2));
    CHECK(!editor->canScrollVertical());
    CHECK(f.below[0]->getY() == editor->bottom());

    // three lines: three rows
    editor->setText("one\ntwo\nthree");
    f.frame();
    CHECK(editor->getContentRows() == 3);
    CHECK(editor->getHeight() == f.heightFor(3));
    CHECK(f.below[0]->getY() == editor->bottom());

    // a line that wraps into 3 rows and one more: four
    const size_t C=editor->getWrapCells();

    REQUIRE(C > 4);
    editor->setText(String(2*C+1, 'w')+"\nend");
    f.frame();
    CHECK(editor->getLineRows(0) == 3);
    CHECK(editor->getContentRows() == 4);
    CHECK(editor->getHeight() == f.heightFor(4));

    // exactly the max: no bar
    editor->setText("1\n2\n3\n4\n5");
    f.frame();
    CHECK(editor->getHeight() == f.heightFor(5));
    CHECK(!editor->canScrollVertical());

    // past it: the max, scrolling, the caret's row in view
    editor->setText("1\n2\n3\n4\n5\n6\n7\n8");
    f.frame();
    editor->focus();
    editor->setCaret(text::Position{7, 1});
    f.frame();
    CHECK(editor->getContentRows() == 6);    // it stops counting past the max
    CHECK(editor->getHeight() == f.heightFor(5));
    CHECK(editor->canScrollVertical());
    CHECK(editor->getVerticalScrollOffset() > 0);
    CHECK(editor->getCaretBounds().h == lineHeight);
    CHECK(f.below[0]->getY() == editor->bottom());

    // one line of more rows than that (the bar narrows them) counts as
    // max+1 too
    editor->setText(String(8*C, 'w'));
    f.frame();
    CHECK(editor->getLineRows(0) >= 8);
    CHECK(editor->getContentRows() == 6);
    CHECK(editor->getHeight() == f.heightFor(5));

    // and down again
    editor->setText("short");
    f.frame();
    CHECK(editor->getHeight() == f.heightFor(2));
    CHECK(!editor->canScrollVertical());
    CHECK(editor->getVerticalScrollOffset() == 0);

    // off: the height it is given
    editor->setAutoHeight(1, 0);
    editor->setHeight(90);
    f.frame();
    CHECK(!editor->hasAutoHeight());
    CHECK(editor->getHeight() == 90);
    CHECK(editor->canScrollVertical());
}

TEST_CASE("CodeEditor auto-height: typing that adds a row grows it in one pass, the fields below only moved")
{
    size_t entries[2]={0, 0};
    int index=0;

    for(size_t fields : {2u, 20u})
    {
        CAPTURE(fields);
        Form f(1, 8, fields);
        controls::CodeEditor *editor=f.editor;
        const size_t C=editor->getWrapCells();

        REQUIRE(C > 4);
        editor->setText(String(C-1, 'a'));
        f.frame();
        editor->focus();
        editor->setCaret(text::Position{0, C-1});
        f.frame();
        f.frame();
        REQUIRE(editor->getHeight() == f.heightFor(1));

        // the row's last cell: still one row
        f.type("b");
        CHECK(editor->getHeight() == f.heightFor(1));

        // one more: a second row, the editor a row taller in that frame, the
        // fields below moved with it
        const int afterY=f.after->getY();

        f.type("c");
        CHECK(editor->getLineRows(0) == 2);
        CHECK(editor->getHeight() == f.heightFor(2));
        CHECK(f.last.passes == 1);
        CHECK(f.after->getY() == afterY+editor->getLineHeight());
        CHECK(f.below[0]->getY() == editor->bottom());
        entries[index++]=f.last.visited;

        // settled: nothing laid out
        f.frame();
        CHECK(f.last.passes == 0);

        // Backspace takes the row away again
        f.key(GLFW_KEY_BACKSPACE);
        CHECK(editor->getHeight() == f.heightFor(1));
        CHECK(f.after->getY() == afterY);
        CHECK(f.last.passes == 1);
    }

    // the fields below were moved, one entry each (their own arrange as the
    // dock moves them), none of their subtrees entered: a Label holds a Text,
    // so an entered subtree would cost more than one a field
    CHECK(entries[1]-entries[0] == 20-2);
}

TEST_CASE("CodeEditor plain: wrapped, no gutter or current line, and Tab moves the focus in and out")
{
    Form f(2, 5);
    controls::CodeEditor *editor=f.editor;

    CHECK(editor->isPlain());
    CHECK(editor->isWrapped());
    CHECK(!editor->hasLineNumbers());
    CHECK(!editor->getTabInsertsSpaces());

    editor->setText("first\nsecond");
    f.frame();

    // no current-line look on the caret's line
    f.before->focus();
    f.frame();
    REQUIRE(KeyboardFocus == f.before);

    // Tab from the field before: into the editor, Tab again: out of it,
    // the text untouched
    f.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == editor);
    for(controls::Base *row : editor->getRows())
    {
        if(!row->hidden())
            CHECK(!gweni_cast<controls::CodeLine>(row)->isCurrentLine());
    }
    f.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == f.after);
    CHECK(editor->getText() == "first\nsecond");

    // Shift+Tab back into it
    f.key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    CHECK(KeyboardFocus == editor);

    // the code look: Tab edits again, the current line shows
    editor->setPlain(false);
    f.frame();
    CHECK(editor->getTabInsertsSpaces());
    editor->setCaret(text::Position{0, 0});
    f.frame();
    f.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == editor);
    CHECK(editor->getDocument().line(0) == "    first");
    CHECK(editor->rowForLine(0)->isCurrentLine());
    // the wrap is left as it was
    CHECK(editor->isWrapped());
}

TEST_CASE("CodeEditor auto-height: in a line taller than the field the caret's row stays in view")
{
    Form f(2, 4);
    controls::CodeEditor *editor=f.editor;
    const int lineHeight=editor->getLineHeight();

    REQUIRE(lineHeight > 0);

    // the caret drawn whole inside the viewport
    auto caretShown=[editor, lineHeight]()
        {
            const Rect caret=editor->getCaretBounds();
            const Rect &viewport=editor->getViewport();

            return caret.h == lineHeight && caret.y >= viewport.y && caret.y+caret.h <= viewport.y+viewport.h;
        };

    editor->focus();
    f.frame();

    // one paragraph typed at its end until it is a row past the max
    const String word="word ";
    size_t words=0;

    while(editor->getLineRows(0) <= 4)
    {
        REQUIRE(words < 200);
        // every keystroke, the one that adds the fifth row included
        for(char c : word)
        {
            f.type(String(1, c));
            CAPTURE(editor->getLineRows(0));
            CHECK(caretShown());
        }
        ++words;
    }
    CHECK(editor->getHeight() == f.heightFor(4));
    CHECK(editor->canScrollVertical());
    CHECK(editor->getVerticalScrollOffset() > 0);
    // and on the idle frames after
    f.frame();
    f.frame();
    CHECK(caretShown());

    // a few more rows: still the caret's
    for(size_t i=0; i < 3*words; ++i)
        f.type(word);
    REQUIRE(editor->getLineRows(0) > 8);
    CHECK(caretShown());

    // a narrower field wraps it into more rows: the caret's in view
    f.panel->setWidth(200);
    f.frame();
    f.frame();
    CHECK(caretShown());

    // wrap off and on: the same
    editor->setWrap(false);
    f.frame();
    CHECK(caretShown());
    editor->setWrap(true);
    f.frame();
    f.frame();
    CHECK(editor->getLineRows(0) > 8);
    CHECK(caretShown());

    // the caret near the line's top, then at its end again
    editor->setCaret(text::Position{0, 0});
    f.frame();
    CHECK(editor->getVerticalScrollOffset() == 0);
    CHECK(caretShown());
    editor->setCaret(editor->getDocument().endPosition());
    f.frame();
    CHECK(caretShown());

    // a fixed-height editor three rows high: a resize and a toggle
    editor->setAutoHeight(1, 0);
    editor->setHeight(f.heightFor(3));
    f.frame();
    f.frame();
    CHECK(caretShown());
    f.panel->setWidth(180);
    f.frame();
    f.frame();
    CHECK(caretShown());
    editor->setWrap(false);
    f.frame();
    editor->setWrap(true);
    f.frame();
    f.frame();
    CHECK(caretShown());
}
