/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeEditor's selection, on the StubSkin with a stepped clock: it is kept
 *  in document positions, and every shown row draws the part of it on its
 *  line as one rect, from the begin cell's x to the end cell's, one cell past
 *  the last glyph where the line's break is selected too, with a line of wide
 *  glyphs counted in cells; the same rects at scale 1 and 2 (they are in
 *  units). Rows scrolled to other lines take those lines' parts, so a
 *  selection whose ends are off screen survives scrolling, and a row out of
 *  it draws none. getSelectedText() is the document's text of the range.
 *  Shift+arrows, Shift+Home/End and Shift+Ctrl+Home/End extend it from the
 *  anchor; a press and a drag select from the press to the mouse; a drag held
 *  below or right of the viewport scrolls the view on the canvas clock (a
 *  frozen clock scrolls nothing), the caret with it, and stops when the
 *  mouse comes back or the button is let go; Shift+press extends; a
 *  double-click selects the word, the punctuation run or the blank run under
 *  the mouse; Ctrl+A selects everything. A font of another cell width but
 *  the same line height (the rows keep their bounds) marks the rows holding
 *  the selection and the caret again, and the rects take the new cells.
 */
#include "headlessFixture.h"

#include <gweni/anim.h>
#include <gweni/controls/codeEditor.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

const String wideJa="\xe6\x97\xa5";     // 日, 2 cells

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CodeEditor *editor=nullptr;
    input::GLFW3 glfw;
    float clock=100.0f;

    explicit Scene(const String &text, float scale=1.0f)
    {
        platform::setTimeSource([this]() { return clock; });

        canvas=new controls::Canvas(&skin);
        canvas->setScale(scale);
        canvas->setSize(600, 600);
        glfw.initialize(canvas);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setPos(0, 0);
        editor->setSize(300, 200);
        editor->setText(text);
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

    //! A frame a sixtieth of a second on.
    void tick()
    {
        clock+=1.0f/60.0f;
        frame();
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

    //! The mouse to a point in the editor's coordinates.
    void move(int x, int y)
    {
        const Point at=editor->localPosToCanvas(Point(x, y));

        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    void press(int x, int y)
    {
        move(x, y);
        canvas->inputMouseButton(0, true);
        frame();
    }

    void release()
    {
        canvas->inputMouseButton(0, false);
        frame();
    }

    //! A point at a cell column (its left edge plus a quarter cell) and in the
    //! middle of a line, as the last layout scrolled the view.
    Point pointAt(size_t line, double cells) const
    {
        const Rect &viewport=editor->getViewport();

        return Point(viewport.x+controls::CodeEditor::TextInset
            +static_cast<int>((cells+0.25)*editor->getCellWidth())-editor->getHorizontalScrollOffset(),
            viewport.y+static_cast<int>(line)*editor->getLineHeight()+editor->getLineHeight()/2
            -editor->getVerticalScrollOffset());
    }

    text::Position caret() const { return editor->getCaret(); }

    //! The selection rect of the row showing \p line (none if no row does).
    Rect rectOf(size_t line) const
    {
        controls::CodeLine *row=editor->rowForLine(line);

        return row?row->getSelectionRect():Rect(-1, -1, -1, -1);
    }

    //! The rect a selection over cells [begin, end) of a row draws.
    Rect cells(size_t begin, size_t end) const
    {
        const int x0=editor->columnX(begin);

        return Rect(x0, 0, editor->columnX(end)-x0, editor->getLineHeight());
    }
};

text::Position at(size_t line, size_t byte) { return text::Position{line, byte}; }

std::string str(const Rect &r)
{
    return "("+std::to_string(r.x)+", "+std::to_string(r.y)+", "+std::to_string(r.w)+", "+std::to_string(r.h)+")";
}

// A rect compared with the one expected, both printed when they differ.
#define CHECK_RECT(actual, expected) \
    do { const Rect a_=(actual), e_=(expected); INFO(#actual, " = ", str(a_), ", expected ", str(e_)); \
        CHECK((a_ == e_)); } while(false)

String numbered(int lines)
{
    String text;

    for(int i=0; i < lines; ++i)
    {
        if(i > 0)
            text+='\n';
        text+="line "+std::to_string(i);
    }
    return text;
}

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

TEST_CASE("CodeEditor selection: one rect per line, a selected line end one cell past its last glyph, at scale 1 and 2")
{
    for(float scale : {1.0f, 2.0f})
    {
        INFO("scale ", scale);
        // cells: line 1 is "a日b", 4 cells in 5 bytes
        Scene s("first line\na"+wideJa+"b\n\nlast line\nafter", scale);
        controls::CodeEditor *editor=s.editor;

        // from the middle of line 0 to the middle of line 3
        editor->setCaret(at(0, 6));
        editor->setCaret(at(3, 4), true);
        s.frame();

        CHECK(editor->getSelectedText() == "line\na"+wideJa+"b\n\nlast");
        CHECK_RECT(s.rectOf(0), s.cells(6, 11));     // "line" and its break
        CHECK_RECT(s.rectOf(1), s.cells(0, 5));      // 4 cells and the break
        CHECK_RECT(s.rectOf(2), s.cells(0, 1));      // an empty line: its break alone
        CHECK_RECT(s.rectOf(3), s.cells(0, 4));      // "last", its break not selected
        CHECK(s.rectOf(4).w == 0);

        // a selection ending inside the wide line, after 日 (byte 4, cell 3)
        editor->setCaret(at(1, 1));
        editor->setCaret(at(1, 4), true);
        s.frame();
        CHECK(editor->getSelectedText() == wideJa);
        CHECK_RECT(s.rectOf(1), s.cells(1, 3));
        CHECK(s.rectOf(0).w == 0);
        CHECK(s.rectOf(2).w == 0);

        // a selection made backwards draws the same
        editor->setCaret(at(1, 4));
        editor->setCaret(at(1, 1), true);
        s.frame();
        CHECK_RECT(s.rectOf(1), s.cells(1, 3));
        CHECK(editor->getAnchor() == at(1, 4));

        // collapsed: nothing
        editor->setCaret(at(2, 0));
        s.frame();
        for(size_t line=0; line < 5; ++line)
            CHECK(s.rectOf(line).w == 0);
    }
}

TEST_CASE("CodeEditor selection: a font of another cell width and the same line height redraws the selection and the caret")
{
    Scene s("first line\nsecond line\nthird", 1.0f);
    controls::CodeEditor *editor=s.editor;

    editor->setCaret(at(0, 6));
    editor->setCaret(at(1, 3), true);
    s.frame();

    const int lineHeight=editor->getLineHeight();
    const float cell=editor->getCellWidth();
    gweni::Font font=editor->getFont();

    // the stub measures a line as the size, truncated, and a cell as 0.4 of it
    font.size+=0.5f;
    editor->setFont(font);
    s.canvas->updateCanvas();

    REQUIRE(editor->getLineHeight() == lineHeight);
    REQUIRE(editor->getCellWidth() != cell);
    CHECK((editor->rowForLine(0)->getStateChange() & controls::StateChange_Background) != 0);
    CHECK((editor->rowForLine(1)->getStateChange() & controls::StateChange_Background) != 0);
    CHECK((editor->getStateChange() & controls::StateChange_Caret) != 0);
    s.canvas->renderCanvas();
    CHECK_RECT(s.rectOf(0), s.cells(6, 11));
    CHECK_RECT(s.rectOf(1), s.cells(0, 3));
}

TEST_CASE("CodeEditor selection: kept in document positions, it survives scrolling")
{
    Scene s(numbered(300));
    controls::CodeEditor *editor=s.editor;
    const int lineHeight=editor->getLineHeight();

    editor->setCaret(at(2, 3));
    editor->setCaret(at(150, 2), true);
    s.frame();

    editor->setVerticalScrollOffset(100*lineHeight);
    s.frame();
    // every row on screen is a whole selected line, its break included
    for(size_t line=100; line < 110; ++line)
    {
        INFO("line ", line);
        CHECK_RECT(s.rectOf(line), s.cells(0, editor->getLineCells(line)+1));
    }

    editor->setVerticalScrollOffset(145*lineHeight);
    s.frame();
    CHECK_RECT(s.rectOf(149), s.cells(0, editor->getLineCells(149)+1));
    CHECK_RECT(s.rectOf(150), s.cells(0, 2));
    CHECK(s.rectOf(151).w == 0);

    editor->setVerticalScrollOffset(200*lineHeight);
    s.frame();
    for(controls::Base *row : editor->getRows())
        CHECK(gweni_cast<controls::CodeLine>(row)->getSelectionRect().w == 0);

    // back up: the first line's part again
    editor->setVerticalScrollOffset(0);
    s.frame();
    CHECK_RECT(s.rectOf(2), s.cells(3, editor->getLineCells(2)+1));
    CHECK(s.rectOf(1).w == 0);
    CHECK(editor->getSelectedText().size() == editor->getDocument().textIn(editor->getSelection()).size());
}

TEST_CASE("CodeEditor selection: Shift with the arrows, Home, End and Ctrl+Home/End extends from the anchor")
{
    Scene s("alpha beta\n  gamma\nend");

    s.editor->setCaret(at(0, 6));
    s.key(GLFW_KEY_RIGHT, GLFW_MOD_SHIFT);
    CHECK(s.editor->getSelectedText() == "b");
    s.key(GLFW_KEY_DOWN, GLFW_MOD_SHIFT);
    CHECK(s.editor->getSelectedText() == "beta\n  gamma");
    s.key(GLFW_KEY_HOME, GLFW_MOD_SHIFT);
    CHECK(s.editor->getSelectedText() == "beta\n  ");
    s.key(GLFW_KEY_END, GLFW_MOD_SHIFT);
    CHECK(s.editor->getSelectedText() == "beta\n  gamma");
    s.key(GLFW_KEY_END, GLFW_MOD_SHIFT|GLFW_MOD_CONTROL);
    CHECK(s.editor->getSelectedText() == "beta\n  gamma\nend");
    s.key(GLFW_KEY_HOME, GLFW_MOD_SHIFT|GLFW_MOD_CONTROL);
    CHECK(s.editor->getSelectedText() == "alpha ");
    CHECK(s.editor->getAnchor() == at(0, 6));
    s.key(GLFW_KEY_RIGHT, GLFW_MOD_SHIFT|GLFW_MOD_CONTROL);
    CHECK(s.editor->getSelectedText() == " ");
    s.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
    CHECK(s.editor->getSelectedText() == s.editor->getText());
    CHECK_RECT(s.rectOf(0), s.cells(0, 11));
    CHECK_RECT(s.rectOf(1), s.cells(0, 8));
    CHECK_RECT(s.rectOf(2), s.cells(0, 3));
}

TEST_CASE("CodeEditor selection: a drag selects from the press to the mouse, Shift+press extends")
{
    Scene s("abcdef\nghijkl\nmnopqr");

    const Point from=s.pointAt(0, 2);
    const Point to=s.pointAt(1, 4);

    s.press(from.x, from.y);
    CHECK(s.caret() == at(0, 2));
    CHECK(!s.editor->hasSelection());
    s.move(to.x, to.y);
    CHECK(s.editor->getSelectedText() == "cdef\nghij");
    // the drag goes back over the anchor
    const Point back=s.pointAt(0, 1);
    s.move(back.x, back.y);
    CHECK(s.editor->getSelectedText() == "b");
    s.release();
    CHECK(MouseFocus == nullptr);
    // a move with the button up selects nothing more
    s.move(to.x, to.y);
    CHECK(s.editor->getSelectedText() == "b");

    // Shift+press extends from the anchor
    s.glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_PRESS, GLFW_MOD_SHIFT);
    const Point end=s.pointAt(2, 3);
    s.press(end.x, end.y);
    s.release();
    s.glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE, 0);
    CHECK(s.editor->getAnchor() == at(0, 2));
    CHECK(s.editor->getSelectedText() == "cdef\nghijkl\nmno");
}

TEST_CASE("CodeEditor selection: a drag held past the viewport scrolls on the canvas clock")
{
    Scene s(numbered(500));
    controls::CodeEditor *editor=s.editor;
    const Rect &viewport=editor->getViewport();

    const Point from=s.pointAt(1, 2);

    s.press(from.x, from.y);
    // 20 units below the viewport, with the clock frozen: no scroll
    s.move(from.x, viewport.y+viewport.h+20);
    CHECK(editor->isAutoScrolling());
    s.frame();
    s.frame();
    CHECK(editor->getVerticalScrollOffset() == 0);

    // each sixtieth of a second scrolls (60 + 10*21)/60 units
    for(int i=0; i < 60; ++i)
        s.tick();

    const int scrolled=editor->getVerticalScrollOffset();
    const int expected=static_cast<int>((controls::CodeEditor::AutoScrollBase
        +controls::CodeEditor::AutoScrollRate*21));

    CHECK(scrolled >= expected-2);
    CHECK(scrolled <= expected+1);
    // the caret is the point under the mouse in the scrolled view, the anchor kept
    CHECK(editor->getAnchor() == at(1, 2));
    CHECK(editor->getCaret().line > 1);
    CHECK(editor->getCaret() == editor->positionAt(Point(from.x, viewport.y+viewport.h+20)));

    // the mouse back inside: it stops
    s.move(from.x, viewport.y+viewport.h/2);
    s.tick();
    CHECK(!editor->isAutoScrolling());
    const int stopped=editor->getVerticalScrollOffset();
    s.tick();
    CHECK(editor->getVerticalScrollOffset() == stopped);

    // and the release stops it too, below the viewport
    s.move(from.x, viewport.y+viewport.h+5);
    s.tick();
    CHECK(editor->isAutoScrolling());
    s.release();
    s.tick();
    CHECK(!editor->isAutoScrolling());
    CHECK(anim::activeCount(editor) == 1);  // the caret's blink alone

    // up from the top it scrolls back
    editor->setVerticalScrollOffset(50*editor->getLineHeight());
    s.frame();
    const int before=editor->getVerticalScrollOffset();
    const Point mid=s.pointAt(55, 1);
    s.press(mid.x, mid.y);
    s.move(mid.x, viewport.y-10);
    for(int i=0; i < 10; ++i)
        s.tick();
    CHECK(editor->getVerticalScrollOffset() < before);
    s.release();
}

TEST_CASE("CodeEditor selection: a double-click selects the word, punctuation run or blanks under the mouse")
{
    Scene s("  key_1: [abc, d]");

    auto doubleClick=[&](double cells)
        {
            const Point p=s.pointAt(0, cells);

            s.move(p.x, p.y);
            s.canvas->inputMouseButton(0, true);
            s.canvas->inputMouseButton(0, false);
            s.canvas->inputMouseButton(0, true);
            s.canvas->inputMouseButton(0, false);
            s.frame();
        };

    doubleClick(4);
    CHECK(s.editor->getSelectedText() == "key_1");
    CHECK(s.caret() == at(0, 7));
    CHECK(s.editor->getAnchor() == at(0, 2));
    doubleClick(7);
    CHECK(s.editor->getSelectedText() == ":");
    doubleClick(11);
    CHECK(s.editor->getSelectedText() == "abc");
    doubleClick(0);
    CHECK(s.editor->getSelectedText() == "  ");
    // past the line's end: the run before it
    doubleClick(40);
    CHECK(s.editor->getSelectedText() == "]");
}
