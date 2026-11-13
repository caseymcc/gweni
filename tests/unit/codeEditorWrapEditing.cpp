/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Editing a wrapped CodeEditor by visual row. The texts are made from the
 *  editor's wrap cells C, so a run of C letters with no blank is one visual
 *  row and the rows of a line start every C bytes. Up and Down move a
 *  visual row at a time, inside a line and across lines, keeping the column
 *  in cells on the row (past a short row's end too, for the next move), and
 *  landing past the end of a row that wraps they leave the caret drawn at
 *  that row's end. Home and End go to the visual row's start and end first,
 *  then to the line's (End's stop at a wrap point drawn on the upper row);
 *  the same byte is drawn at the end of the upper row or the start of the
 *  lower one as the move that reached it implies, and getCaretBounds() is
 *  there. A click lands on the visual row under it, past a wrapping row's
 *  end at that row's end. PageDown moves the caret by the viewport's rows
 *  and keeps it in view; Down past the viewport's bottom scrolls one row.
 *  Toggling wrap, and a resize, keep the caret's line on screen. Every frame
 *  lays out in one pass with no request.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/text/cellWrap.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CodeEditor *editor=nullptr;
    input::GLFW3 glfw;
    //! The wrap's cells a row.
    size_t C=0;

    explicit Scene(int width=200, int height=200)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 600);
        glfw.initialize(canvas);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setPos(0, 0);
        editor->setSize(width, height);
        editor->setWrap(true);
        frame();
        editor->focus();
        frame();
        C=editor->getWrapCells();
        REQUIRE(C > 4);
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
    }

    int requests=0;
    int maxPasses=0;
    bool renderLayout=false;

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();

        const controls::LayoutStats &layout=canvas->getLayoutStats();

        requests+=layout.requests;
        maxPasses=std::max(maxPasses, layout.passes);
        renderLayout=renderLayout || layout.renderLayout;
    }

    void checkOnePass() const
    {
        CHECK(requests == 0);
        CHECK(maxPasses <= 1);
        CHECK(!renderLayout);
    }

    void setText(const String &text)
    {
        editor->setText(text);
        frame();
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

    //! A press and release at a point in the editor's coordinates.
    void click(int x, int y)
    {
        const Point at=editor->localPosToCanvas(Point(x, y));

        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    //! The editor-coordinates point a little into cell \p column of visual
    //! row \p row of line \p line, which a row shows.
    Point cellPoint(size_t line, size_t row, size_t column) const
    {
        const controls::Base *shown=editor->rowForLine(line);

        REQUIRE(shown);

        const Rect &viewport=editor->getViewport();
        const double cell=editor->getCellWidth();

        return Point(viewport.x+controls::CodeEditor::TextInset+static_cast<int>(column*cell+cell*0.25),
            viewport.y+shown->getY()+static_cast<int>(row)*editor->getLineHeight()+editor->getLineHeight()/2);
    }

    //! Where getCaretBounds() should be for the caret on visual row \p row
    //! of its line at \p column cells into it.
    Rect expectedCaret(size_t row, size_t column) const
    {
        const controls::Base *shown=editor->rowForLine(editor->getCaret().line);

        REQUIRE(shown);

        const Rect &viewport=editor->getViewport();

        return Rect(viewport.x+editor->columnX(column),
            viewport.y+shown->getY()+static_cast<int>(row)*editor->getLineHeight(), 1, editor->getLineHeight());
    }

    text::Position caret() const { return editor->getCaret(); }

    //! The caret is on screen: its rect is not empty and inside the viewport.
    void checkCaretOnScreen() const
    {
        const Rect bounds=editor->getCaretBounds();
        const Rect &viewport=editor->getViewport();

        CHECK(bounds.w == 1);
        CHECK(bounds.h == editor->getLineHeight());
        CHECK(bounds.y >= viewport.y);
        CHECK(bounds.y+bounds.h <= viewport.y+viewport.h);
    }
};

text::Position at(size_t line, size_t byte) { return text::Position{line, byte}; }

bool sameRect(const Rect &a, const Rect &b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
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

TEST_CASE("CodeEditor wrapped: Up and Down move by visual row, keeping the column in cells")
{
    Scene s;
    const size_t C=s.C;
    controls::CodeEditor *editor=s.editor;

    // line 0: 3 rows [0, C) [C, 2C) [2C, 2C+3); line 1 one row; line 2 two
    s.setText(String(2*C+3, 'x')+"\nshort\n"+String(C+2, 'y'));
    REQUIRE(editor->getLineRows(0) == 3);
    REQUIRE(editor->getLineRows(2) == 2);

    editor->setCaret(at(0, 2));
    s.frame();
    CHECK(editor->getCaretRow() == 0);

    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(0, C+2));
    CHECK(editor->getCaretRow() == 1);
    CHECK(editor->getCaretRowColumn() == 2);
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(0, 2*C+2));
    CHECK(editor->getCaretRow() == 2);
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(1, 2));
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(2, 2));
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(2, C+2));
    CHECK(editor->getCaretRow() == 1);
    // down from the last row: the end
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(2, C+2));

    // and back up, row by row across the lines
    editor->setCaret(at(2, C+2));
    s.frame();
    s.key(GLFW_KEY_UP);
    CHECK(s.caret() == at(2, 2));
    s.key(GLFW_KEY_UP);
    CHECK(s.caret() == at(1, 2));
    s.key(GLFW_KEY_UP);
    CHECK(s.caret() == at(0, 2*C+2));
    CHECK(editor->getCaretRow() == 2);
    s.key(GLFW_KEY_UP);
    CHECK(s.caret() == at(0, C+2));
    s.key(GLFW_KEY_UP);
    CHECK(s.caret() == at(0, 2));
    s.key(GLFW_KEY_UP);
    CHECK(s.caret() == at(0, 0));

    // the goal column outlives a short row: column 4 through a row of 3
    editor->setCaret(at(0, C+4));
    s.frame();
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(0, 2*C+3));
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(1, 4));
    s.checkOnePass();
}

TEST_CASE("CodeEditor wrapped: Up or Down past the end of a row that wraps leaves the caret drawn at its end")
{
    Scene s;
    const size_t C=s.C;
    controls::CodeEditor *editor=s.editor;

    // "ab " then a word of C letters, which goes to the next row: rows
    // "ab " [0, 3) and the word [3, C+3)
    s.setText("ab "+String(C, 'c'));
    REQUIRE(editor->getLineRows(0) == 2);

    editor->setCaret(at(0, 3+5));
    s.frame();
    CHECK(editor->getCaretRow() == 1);
    CHECK(!editor->isCaretAtRowEnd());

    s.key(GLFW_KEY_UP);
    // byte 3 is the second row's start too: drawn at the first row's end
    CHECK(s.caret() == at(0, 3));
    CHECK(editor->isCaretAtRowEnd());
    CHECK(editor->getCaretRow() == 0);
    CHECK(editor->getCaretRowColumn() == 3);
    CHECK(sameRect(editor->getCaretBounds(), s.expectedCaret(0, 3)));

    // the goal kept: Down again lands at column 5 of the word
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(0, 3+5));
    CHECK(!editor->isCaretAtRowEnd());
    s.checkOnePass();
}

TEST_CASE("CodeEditor wrapped: Home and End go to the visual row's start and end, then to the line's")
{
    Scene s;
    const size_t C=s.C;
    controls::CodeEditor *editor=s.editor;

    // rows "  a " [0, 4), then a word of 2C letters, cut: [4, 4+C) [4+C, 4+2C)
    s.setText("  a "+String(2*C, 'x'));
    REQUIRE(editor->getLineRows(0) == 3);

    editor->setCaret(at(0, 4+3));
    s.frame();
    REQUIRE(editor->getCaretRow() == 1);

    // End: the row's end, at the wrap point, drawn on that row
    s.key(GLFW_KEY_END);
    CHECK(s.caret() == at(0, 4+C));
    CHECK(editor->isCaretAtRowEnd());
    CHECK(editor->getCaretRow() == 1);
    CHECK(sameRect(editor->getCaretBounds(), s.expectedCaret(1, C)));
    // End again: the line's end
    s.key(GLFW_KEY_END);
    CHECK(s.caret() == at(0, 4+2*C));
    CHECK(editor->getCaretRow() == 2);
    CHECK(!editor->isCaretAtRowEnd());

    // Home: the last row's start (the same byte, now drawn on the row below)
    s.key(GLFW_KEY_HOME);
    CHECK(s.caret() == at(0, 4+C));
    CHECK(!editor->isCaretAtRowEnd());
    CHECK(editor->getCaretRow() == 2);
    CHECK(sameRect(editor->getCaretBounds(), s.expectedCaret(2, 0)));
    // Home again: the line's, its first non-blank, then column 0
    s.key(GLFW_KEY_HOME);
    CHECK(s.caret() == at(0, 2));
    s.key(GLFW_KEY_HOME);
    CHECK(s.caret() == at(0, 0));

    // End on the first row, then Home: that row's end, then the line's Home
    s.key(GLFW_KEY_END);
    CHECK(s.caret() == at(0, 4));
    CHECK(editor->isCaretAtRowEnd());
    CHECK(editor->getCaretRow() == 0);
    s.key(GLFW_KEY_HOME);
    CHECK(s.caret() == at(0, 2));

    // Shift+End selects to the row's end
    editor->setCaret(at(0, 4+1));
    s.frame();
    s.key(GLFW_KEY_END, GLFW_MOD_SHIFT);
    CHECK(editor->getAnchor() == at(0, 4+1));
    CHECK(s.caret() == at(0, 4+C));
    CHECK(editor->getSelectedText() == String(C-1, 'x'));
    s.checkOnePass();
}

TEST_CASE("CodeEditor wrapped: a byte at a wrap point is drawn where the move that reached it implies")
{
    Scene s;
    const size_t C=s.C;
    controls::CodeEditor *editor=s.editor;

    s.setText(String(2*C, 'x'));
    REQUIRE(editor->getLineRows(0) == 2);

    editor->setCaret(at(0, 1));
    s.frame();
    s.key(GLFW_KEY_END);
    REQUIRE(s.caret() == at(0, C));
    CHECK(sameRect(editor->getCaretBounds(), s.expectedCaret(0, C)));

    // Right goes on past it, Left back to it: the lower row's start
    s.key(GLFW_KEY_RIGHT);
    CHECK(s.caret() == at(0, C+1));
    CHECK(sameRect(editor->getCaretBounds(), s.expectedCaret(1, 1)));
    s.key(GLFW_KEY_LEFT);
    CHECK(s.caret() == at(0, C));
    CHECK(!editor->isCaretAtRowEnd());
    CHECK(sameRect(editor->getCaretBounds(), s.expectedCaret(1, 0)));

    // setCaret() is the lower row's start too
    editor->setCaret(at(0, C));
    s.frame();
    CHECK(!editor->isCaretAtRowEnd());
    CHECK(editor->getCaretRow() == 1);
    s.checkOnePass();
}

TEST_CASE("CodeEditor wrapped: a click lands on the visual row under it")
{
    Scene s;
    const size_t C=s.C;
    controls::CodeEditor *editor=s.editor;

    // line 0: 3 rows of x; line 1: "ab " and a C-letter word
    s.setText(String(2*C+3, 'x')+"\nab "+String(C, 'c')+"\nlast");

    s.click(s.cellPoint(0, 1, 3).x, s.cellPoint(0, 1, 3).y);
    CHECK(s.caret() == at(0, C+3));
    CHECK(editor->getCaretRow() == 1);
    s.click(s.cellPoint(0, 2, 1).x, s.cellPoint(0, 2, 1).y);
    CHECK(s.caret() == at(0, 2*C+1));
    // past the last row's end: the line's end
    s.click(s.cellPoint(0, 2, C-1).x, s.cellPoint(0, 2, C-1).y);
    CHECK(s.caret() == at(0, 2*C+3));
    CHECK(!editor->isCaretAtRowEnd());

    // past the end of "ab ", a row that wraps: drawn at its end
    s.click(s.cellPoint(1, 0, 7).x, s.cellPoint(1, 0, 7).y);
    CHECK(s.caret() == at(1, 3));
    CHECK(editor->isCaretAtRowEnd());
    CHECK(sameRect(editor->getCaretBounds(), s.expectedCaret(0, 3)));
    // the word's row
    s.click(s.cellPoint(1, 1, 2).x, s.cellPoint(1, 1, 2).y);
    CHECK(s.caret() == at(1, 5));
    CHECK(editor->getCaretRow() == 1);

    // a double-click selects the word on the row under it
    const Point word=s.cellPoint(1, 1, 2);
    const Point canvasWord=editor->localPosToCanvas(word);

    s.canvas->inputMouseMoved(canvasWord.x, canvasWord.y, 0, 0);
    editor->onMouseDoubleClickLeft(canvasWord.x, canvasWord.y);
    s.frame();
    CHECK(editor->getSelectedText() == String(C, 'c'));

    // the gutter side (left of the viewport): the row's start
    s.click(0, s.cellPoint(0, 1, 0).y);
    CHECK(s.caret() == at(0, C));
    s.checkOnePass();
}

TEST_CASE("CodeEditor wrapped: Down past the bottom scrolls a row, PageDown a page, the caret kept in view")
{
    Scene s(200, 120);
    const size_t C=s.C;
    controls::CodeEditor *editor=s.editor;
    String text;

    // 40 lines of 3 rows each
    for(int i=0; i < 40; ++i)
        text+=(i > 0?"\n":"")+String(2*C+1, static_cast<char>('a'+i%26));
    s.setText(text);

    const int lineHeight=editor->getLineHeight();
    const int viewportRows=editor->getViewportHeight()/lineHeight;

    REQUIRE(viewportRows >= 3);

    // down to the viewport's last whole row and one past it
    for(int i=0; i < viewportRows; ++i)
    {
        s.key(GLFW_KEY_DOWN);
        s.checkCaretOnScreen();
    }
    CHECK(editor->getVerticalScrollOffset() > 0);
    CHECK(editor->getVerticalScrollOffset() <= lineHeight);

    // PageDown: the caret a page of rows further down, still on screen
    const size_t rowsBefore=s.caret().line*3+editor->getCaretRow();

    s.key(GLFW_KEY_PAGE_DOWN);
    CHECK(s.caret().line*3+editor->getCaretRow() == rowsBefore+static_cast<size_t>(viewportRows));
    s.checkCaretOnScreen();
    s.key(GLFW_KEY_PAGE_UP);
    CHECK(s.caret().line*3+editor->getCaretRow() == rowsBefore);
    s.checkCaretOnScreen();

    // the view scrolled past the caret (its row above the top): a move on
    // its row scrolls the view back up to it
    const int offset=editor->getVerticalScrollOffset();

    editor->setVerticalScrollOffset(offset+(viewportRows+3)*lineHeight);
    s.frame();
    REQUIRE(editor->getCaretBounds().h == 0);
    s.key(GLFW_KEY_RIGHT);
    s.checkCaretOnScreen();
    CHECK(editor->getCaretBounds().y == editor->getViewport().y);
    s.checkOnePass();
}

TEST_CASE("CodeEditor wrapped: toggling wrap keeps the caret's line on screen")
{
    Scene s(200, 150);
    const size_t C=s.C;
    controls::CodeEditor *editor=s.editor;
    String text;

    // lines of 4 rows wrapped, so wrapping moves a line far down
    for(int i=0; i < 200; ++i)
        text+=(i > 0?"\n":"")+std::to_string(i)+" "+String(3*C, 'w');
    s.setText(text);

    editor->setWrap(false);
    s.frame();
    editor->setCaret(at(60, 2));
    s.frame();
    REQUIRE(editor->rowForLine(60));
    s.checkCaretOnScreen();

    editor->setWrap(true);
    s.frame();
    CHECK(editor->isWrapped());
    REQUIRE(editor->rowForLine(60));
    s.checkCaretOnScreen();
    CHECK(s.caret() == at(60, 2));

    // and the caret on a later row of a wrapped line, unwrapped again
    editor->setCaret(at(60, 3*C));
    s.frame();
    s.checkCaretOnScreen();
    editor->setWrap(false);
    s.frame();
    REQUIRE(editor->rowForLine(60));
    s.checkCaretOnScreen();
    s.checkOnePass();
}

TEST_CASE("CodeEditor wrapped: a resize keeps the caret's line on screen")
{
    Scene s(300, 200);
    const size_t C=s.C;
    controls::CodeEditor *editor=s.editor;
    String text;

    for(int i=0; i < 200; ++i)
        text+=(i > 0?"\n":"")+std::to_string(i)+" "+String(C-4, 'w')+" "+String(C/2, 'v');
    s.setText(text);

    // the caret on the view's bottom line: narrowing makes every line above
    // it taller, which would push it out
    editor->setCaret(at(30, 0));
    s.frame();
    for(int i=0; i < 6; ++i)
        s.key(GLFW_KEY_DOWN);
    s.checkCaretOnScreen();

    const size_t line=s.caret().line;

    editor->setSize(160, 200);
    s.frame();
    CHECK(editor->getWrapCells() < C);
    REQUIRE(editor->rowForLine(line));
    s.checkCaretOnScreen();

    // a shorter editor too
    editor->setSize(160, 80);
    s.frame();
    REQUIRE(editor->rowForLine(line));
    s.checkCaretOnScreen();

    // and back to the first size
    editor->setSize(300, 200);
    s.frame();
    REQUIRE(editor->rowForLine(line));
    s.checkCaretOnScreen();
    s.checkOnePass();
}
