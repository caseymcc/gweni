/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeEditor's caret and editing keys, driven through the GLFW translator as
 *  a window's keyboard drives them, on the StubSkin. Every column is in
 *  cells: Up and Down keep the column a run of them started at, landing
 *  before a wide glyph (2 cells) that column falls inside and coming back to
 *  it on a line that has it; a combining mark moves with its base (no caret
 *  stop between them). Home goes to the first non-blank and then to column
 *  0, End to the line's end, Ctrl+Home/End to the document's ends; Up on the
 *  first line goes to its start, Down on the last to its end; Ctrl+Left and
 *  Ctrl+Right move over blanks and then a word or a run of punctuation, and
 *  across line ends; PageUp and PageDown move the caret and the view by the
 *  viewport's whole lines. Typing inserts UTF-8 (2, 3 and 4 byte codepoints),
 *  Backspace and Delete remove one codepoint or join lines, Return keeps the
 *  line's indent up to the caret, and a typed character or Return replaces a
 *  selection (made with Shift+arrows). Ctrl+C, Ctrl+X and Ctrl+V go through
 *  the platform clipboard. A press puts the caret at the nearest caret stop
 *  under the mouse (past a line's end: its end; below the last line: the
 *  document's end), Shift+press extends the selection. The caret's rect is
 *  at its column and line in the viewport, and empty once scrolled out; its
 *  blink animation runs only while the editor has the focus. A caret move
 *  keeps the caret in view (scrolling the line in by the next layout, the
 *  column at once). Return on a line in the middle of the screen binds the
 *  row of the line it split and the row the new line came into view on, and
 *  no other.
 *
 *  Also text::byteAtCell, byteNearestCell, nextCaretStop and prevCaretStop.
 */
#include "headlessFixture.h"

#include <gweni/anim.h>
#include <gweni/controls/codeEditor.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>
#include <gweni/text/cells.h>

#include <doctest/doctest.h>

#include <map>
#include <string>

using namespace gweni;

namespace
{

const String wideJa="\xe6\x97\xa5";     // 日, 2 cells, 3 bytes
const String eAcute="\xc3\xa9";         // é, 1 cell, 2 bytes
const String combAcute="\xcc\x81";      // U+0301, 0 cells, 2 bytes
const String grin="\xf0\x9f\x98\x80";   // U+1F600, 2 cells, 4 bytes

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CodeEditor *editor=nullptr;
    input::GLFW3 glfw;

    explicit Scene(const String &text, int height=200)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 600);
        glfw.initialize(canvas);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setPos(0, 0);
        editor->setSize(300, height);
        editor->setText(text);
        frame();
        editor->focus();
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the focus and the key state are process-global
        glfw.processFocus(false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
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

    //! A press and release at a point in the editor's coordinates.
    void click(int x, int y, int mods=0)
    {
        if(mods & GLFW_MOD_SHIFT)
            glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_PRESS, GLFW_MOD_SHIFT);

        const Point at=editor->localPosToCanvas(Point(x, y));

        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);

        if(mods & GLFW_MOD_SHIFT)
            glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE, 0);
        frame();
    }

    text::Position caret() const { return editor->getCaret(); }
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

TEST_CASE("Text cells: byteAtCell and byteNearestCell land on caret stops, before a wide glyph a column is inside")
{
    const String line="a"+wideJa+"b"+"e"+combAcute+"c";   // cells: a 1, 日 2, b 1, é 1, c 1

    CHECK(text::byteAtCell(line, 0) == 0);
    CHECK(text::byteAtCell(line, 1) == 1);
    // column 2 is inside 日 (cells 1..2): before it
    CHECK(text::byteAtCell(line, 2) == 1);
    CHECK(text::byteAtCell(line, 3) == 4);
    // the mark goes with its e
    CHECK(text::byteAtCell(line, 5) == 8);
    CHECK(text::byteAtCell(line, 99) == line.size());

    CHECK(text::byteNearestCell(line, 0.4) == 0);
    CHECK(text::byteNearestCell(line, 0.6) == 1);
    // inside 日: to its nearer edge, a tie to the left
    CHECK(text::byteNearestCell(line, 1.9) == 1);
    CHECK(text::byteNearestCell(line, 2.0) == 1);
    CHECK(text::byteNearestCell(line, 2.1) == 4);
    CHECK(text::byteNearestCell(line, 4.7) == 8);
    CHECK(text::byteNearestCell(line, -3.0) == 0);
    CHECK(text::byteNearestCell(line, 50.0) == line.size());

    CHECK(text::nextCaretStop(line, 0) == 1);
    CHECK(text::nextCaretStop(line, 1) == 4);
    // e and its mark are one stop
    CHECK(text::nextCaretStop(line, 5) == 8);
    CHECK(text::prevCaretStop(line, 8) == 5);
    CHECK(text::prevCaretStop(line, 4) == 1);
    CHECK(text::prevCaretStop(line, 0) == 0);
    CHECK(text::nextCaretStop(line, line.size()) == line.size());
}

TEST_CASE("CodeEditor: Up and Down keep the column in cells across a line of wide glyphs")
{
    Scene s("abcdefgh\n"+wideJa+wideJa+wideJa+wideJa+"\nabcdefgh\nab");

    s.editor->setCaret(at(0, 3));
    CHECK(s.editor->getCaretColumn() == 3);

    // column 3 is inside the second 日: before it, column 2
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(1, 3));
    CHECK(s.editor->getCaretColumn() == 2);
    // and back to column 3 where the line has it
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(2, 3));
    // a short line: its end, the goal kept
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(3, 2));
    s.key(GLFW_KEY_UP);
    CHECK(s.caret() == at(2, 3));
    // Down on the last line goes to its end, Up on the first to its start
    s.editor->setCaret(at(3, 0));
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(3, 2));
    s.editor->setCaret(at(0, 5));
    s.key(GLFW_KEY_UP);
    CHECK(s.caret() == at(0, 0));

    // a horizontal move forgets the goal
    s.editor->setCaret(at(0, 7));
    s.key(GLFW_KEY_DOWN);
    CHECK(s.editor->getCaretColumn() == 6);
    s.key(GLFW_KEY_LEFT);
    CHECK(s.editor->getCaretColumn() == 4);
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(2, 4));
}

TEST_CASE("CodeEditor: Left and Right step over a wide glyph and a base with its mark, and across lines")
{
    Scene s("a"+wideJa+"e"+combAcute+"\nz");

    s.editor->setCaret(at(0, 0));
    s.key(GLFW_KEY_RIGHT);
    CHECK(s.caret() == at(0, 1));
    s.key(GLFW_KEY_RIGHT);
    CHECK(s.caret() == at(0, 4));
    CHECK(s.editor->getCaretColumn() == 3);
    s.key(GLFW_KEY_RIGHT);
    CHECK(s.caret() == at(0, 7));
    s.key(GLFW_KEY_RIGHT);
    CHECK(s.caret() == at(1, 0));
    s.key(GLFW_KEY_LEFT);
    CHECK(s.caret() == at(0, 7));
    s.key(GLFW_KEY_LEFT);
    CHECK(s.caret() == at(0, 4));

    // a position inside a codepoint or before a mark is snapped to its stop
    s.editor->setCaret(at(0, 2));
    CHECK(s.caret() == at(0, 1));
    s.editor->setCaret(at(0, 5));
    CHECK(s.caret() == at(0, 4));
}

TEST_CASE("CodeEditor: Home toggles between the first non-blank and column 0, and End and Ctrl+Home/End")
{
    Scene s("first\n    indented line\nlast");

    s.editor->setCaret(at(1, 10));
    s.key(GLFW_KEY_HOME);
    CHECK(s.caret() == at(1, 4));
    s.key(GLFW_KEY_HOME);
    CHECK(s.caret() == at(1, 0));
    s.key(GLFW_KEY_HOME);
    CHECK(s.caret() == at(1, 4));
    s.key(GLFW_KEY_END);
    CHECK(s.caret() == at(1, 17));
    s.key(GLFW_KEY_END, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(2, 4));
    s.key(GLFW_KEY_HOME, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(0, 0));
    // Shift extends
    s.key(GLFW_KEY_END, GLFW_MOD_SHIFT);
    CHECK(s.editor->getSelectedText() == "first");
}

TEST_CASE("CodeEditor: Ctrl+Left and Ctrl+Right move by words, punctuation runs and line ends")
{
    Scene s("  key_1: [a, b]  \nnext");

    s.editor->setCaret(at(0, 0));
    s.key(GLFW_KEY_RIGHT, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(0, 7));      // over the blanks and key_1
    s.key(GLFW_KEY_RIGHT, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(0, 8));      // ':'
    s.key(GLFW_KEY_RIGHT, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(0, 10));     // ' ['
    s.key(GLFW_KEY_RIGHT, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(0, 11));     // a
    s.editor->setCaret(at(0, 15));
    s.key(GLFW_KEY_RIGHT, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(0, 17));     // trailing blanks to the end
    s.key(GLFW_KEY_RIGHT, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(1, 0));      // the line end
    s.key(GLFW_KEY_LEFT, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(0, 17));
    s.key(GLFW_KEY_LEFT, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(0, 14));     // back over the blanks and ']'
    s.editor->setCaret(at(0, 7));
    s.key(GLFW_KEY_LEFT, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(0, 2));
    s.key(GLFW_KEY_LEFT, GLFW_MOD_CONTROL);
    CHECK(s.caret() == at(0, 0));
}

TEST_CASE("CodeEditor: typing inserts UTF-8, Backspace and Delete remove a codepoint or join lines")
{
    Scene s("ab\ncd");

    s.editor->setCaret(at(0, 1));
    s.type(U"é€\U0001F600");
    CHECK(s.line(0) == "a"+eAcute+"\xe2\x82\xac"+grin+"b");
    CHECK(s.caret() == at(0, 10));
    CHECK(s.editor->getCaretColumn() == 5);
    CHECK(s.editor->getLineCells(0) == 6);

    s.key(GLFW_KEY_BACKSPACE);
    CHECK(s.line(0) == "a"+eAcute+"\xe2\x82\xac"+"b");
    CHECK(s.caret() == at(0, 6));
    s.key(GLFW_KEY_DELETE);
    CHECK(s.line(0) == "a"+eAcute+"\xe2\x82\xac");

    // Delete at a line's end joins the next line, Backspace at its start the line before
    s.key(GLFW_KEY_DELETE);
    CHECK(s.editor->getLineCount() == 1);
    CHECK(s.line(0) == "a"+eAcute+"\xe2\x82\xac"+"cd");
    CHECK(s.caret() == at(0, 6));
    s.key(GLFW_KEY_ENTER);
    CHECK(s.editor->getLineCount() == 2);
    CHECK(s.caret() == at(1, 0));
    s.key(GLFW_KEY_BACKSPACE);
    CHECK(s.editor->getLineCount() == 1);
    CHECK(s.caret() == at(0, 6));

    // control characters are not text
    s.type(U"\t\x01");
    CHECK(s.line(0) == "a"+eAcute+"\xe2\x82\xac"+"cd");
    // at the very start and end nothing happens
    s.key(GLFW_KEY_HOME, GLFW_MOD_CONTROL);
    s.key(GLFW_KEY_BACKSPACE);
    s.key(GLFW_KEY_END, GLFW_MOD_CONTROL);
    s.key(GLFW_KEY_DELETE);
    CHECK(s.editor->getText() == "a"+eAcute+"\xe2\x82\xac"+"cd");
}

TEST_CASE("CodeEditor: Return keeps the line's indent up to the caret")
{
    Scene s("    key: value\n\tx");

    s.editor->setCaret(at(0, 14));
    s.key(GLFW_KEY_ENTER);
    CHECK(s.line(1) == "    ");
    CHECK(s.caret() == at(1, 4));
    s.type(U"next");
    CHECK(s.line(1) == "    next");

    // inside the indent, only the blanks before the caret
    s.editor->setCaret(at(0, 2));
    s.key(GLFW_KEY_ENTER);
    CHECK(s.line(0) == "  ");
    CHECK(s.line(1) == "    key: value");
    CHECK(s.caret() == at(1, 2));

    // a tab indent is kept as it is
    REQUIRE(s.line(3) == "\tx");
    s.editor->setCaret(at(3, 2));
    s.key(GLFW_KEY_ENTER);
    CHECK(s.line(4) == "\t");
}

TEST_CASE("CodeEditor: a typed character and Return replace the selection, and Ctrl+C, Ctrl+X and Ctrl+V")
{
    Scene s("alpha beta\ngamma");

    s.editor->setCaret(at(0, 6));
    s.key(GLFW_KEY_RIGHT, GLFW_MOD_SHIFT);
    s.key(GLFW_KEY_RIGHT, GLFW_MOD_SHIFT);
    CHECK(s.editor->getSelectedText() == "be");
    s.type(U"B");
    CHECK(s.line(0) == "alpha Bta");
    CHECK(!s.editor->hasSelection());

    // a selection over the line end
    s.editor->setCaret(at(0, 7));
    s.editor->setCaret(at(1, 2), true);
    CHECK(s.editor->getSelectedText() == "ta\nga");
    s.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == "ta\nga");
    s.key(GLFW_KEY_X, GLFW_MOD_CONTROL);
    CHECK(s.editor->getText() == "alpha Bmma");
    CHECK(s.caret() == at(0, 7));
    s.key(GLFW_KEY_END, GLFW_MOD_CONTROL);
    s.key(GLFW_KEY_V, GLFW_MOD_CONTROL);
    CHECK(s.editor->getText() == "alpha Bmmata\nga");
    CHECK(s.caret() == at(1, 2));
    // "\r\n" in a paste is a line break
    platform::setClipboardText("1\r\n2");
    s.key(GLFW_KEY_V, GLFW_MOD_CONTROL);
    CHECK(s.editor->getLineCount() == 3);
    CHECK(s.line(2) == "2");

    // Return over a selection
    s.editor->setCaret(at(0, 0));
    s.editor->setCaret(at(0, 5), true);
    s.key(GLFW_KEY_ENTER);
    CHECK(s.line(0) == "");
    CHECK(s.line(1) == " Bmmata");
    // Ctrl+A
    s.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
    CHECK(s.editor->getSelectedText() == s.editor->getText());
    // without Shift a selection collapses to the end the arrow points at
    s.key(GLFW_KEY_LEFT);
    CHECK(s.caret() == at(0, 0));
    CHECK(!s.editor->hasSelection());
}

TEST_CASE("CodeEditor: a press puts the caret at the nearest stop under the mouse")
{
    Scene s("abcdef\n"+wideJa+wideJa+"\nxy");
    controls::CodeEditor *editor=s.editor;
    const Rect &viewport=editor->getViewport();
    const int lineHeight=editor->getLineHeight();
    const float cell=editor->getCellWidth();
    auto xOf=[&](double cells) { return viewport.x+controls::CodeEditor::TextInset+static_cast<int>(cells*cell); };

    s.click(xOf(2.2), viewport.y+lineHeight/2);
    CHECK(s.caret() == at(0, 2));
    s.click(xOf(2.8), viewport.y+lineHeight/2);
    CHECK(s.caret() == at(0, 3));
    // on the second line, inside the first 日's right half: after it
    s.click(xOf(1.6), viewport.y+lineHeight+lineHeight/2);
    CHECK(s.caret() == at(1, 3));
    // past the end of a line
    s.click(xOf(40), viewport.y+2*lineHeight+1);
    CHECK(s.caret() == at(2, 2));
    // below the last line: the document's end; Shift+press extends
    s.click(xOf(0.2), viewport.y+lineHeight/2);
    s.click(xOf(1), viewport.y+10*lineHeight, GLFW_MOD_SHIFT);
    CHECK(s.caret() == at(2, 2));
    CHECK(editor->getAnchor() == at(0, 0));
}

TEST_CASE("CodeEditor: the caret's rect is at its column and line, and empty scrolled out")
{
    String text;
    for(int i=0; i < 200; ++i)
        text+="line "+std::to_string(i)+"\n";
    Scene s(text);
    controls::CodeEditor *editor=s.editor;
    const Rect &viewport=editor->getViewport();
    const int lineHeight=editor->getLineHeight();

    editor->setCaret(at(3, 2));
    s.frame();

    Rect caret=editor->getCaretBounds();

    CHECK(caret.x == viewport.x+editor->columnX(2));
    CHECK(caret.y == viewport.y+3*lineHeight);
    CHECK(caret.w == 1);
    CHECK(caret.h == lineHeight);

    // scrolled away from it: empty, and the caret's line still the caret's
    editor->setVerticalScrollOffset(100*lineHeight);
    s.frame();
    CHECK(editor->getCaretBounds().w == 0);

    // a move puts it in view again: its line at the bottom of the viewport
    s.key(GLFW_KEY_DOWN);
    CHECK(s.caret() == at(4, 2));
    caret=editor->getCaretBounds();
    CHECK(caret.h == lineHeight);
    CHECK(editor->getVerticalScrollOffset() == 4*lineHeight);

    // PageDown moves the caret and the view by the viewport's whole lines
    const int page=editor->getViewportHeight()/lineHeight;
    const int offset=editor->getVerticalScrollOffset();

    s.key(GLFW_KEY_PAGE_DOWN);
    CHECK(s.caret() == at(4+page, 2));
    CHECK(editor->getVerticalScrollOffset() == offset+page*lineHeight);
    s.key(GLFW_KEY_PAGE_UP);
    CHECK(s.caret() == at(4, 2));
    CHECK(editor->getVerticalScrollOffset() == offset);

    // a long line's end: the view scrolls sideways to it
    editor->getDocument().insert(at(4, 0), String(200, 'x'));
    s.key(GLFW_KEY_END);
    CHECK(editor->getHorizontalScrollOffset() > 0);
    caret=editor->getCaretBounds();
    CHECK(caret.w == 1);
    CHECK(caret.x < viewport.x+viewport.w);
    s.key(GLFW_KEY_HOME);
    s.key(GLFW_KEY_HOME);
    CHECK(editor->getHorizontalScrollOffset() == 0);
}

TEST_CASE("CodeEditor: the caret blinks only while the editor has the focus")
{
    Scene s("text");

    CHECK(anim::activeCount(s.editor) == 1);
    CHECK(s.editor->isCaretVisible());

    s.editor->unfocus();
    s.frame();
    CHECK(anim::activeCount(s.editor) == 0);

    s.editor->focus();
    s.frame();
    CHECK(anim::activeCount(s.editor) == 1);
}

TEST_CASE("CodeEditor: Return mid-screen binds the row it split and the row the new line came into view on")
{
    String text;
    for(int i=0; i < 1000; ++i)
        text+="line "+std::to_string(i)+"\n";
    Scene s(text);
    controls::CodeEditor *editor=s.editor;

    editor->setVerticalScrollOffset(100*editor->getLineHeight());
    s.frame();

    std::map<controls::CodeLine *, String> before;
    std::map<controls::CodeLine *, size_t> lines;

    for(controls::Base *row : editor->getRows())
    {
        controls::CodeLine *line=gweni_cast<controls::CodeLine>(row);
        before[line]=line->getText();
        lines[line]=line->getLine();
    }

    const size_t middle=105;

    editor->setCaret(at(middle, 4));
    s.frame();

    const size_t measures=s.renderer.measures;

    s.key(GLFW_KEY_ENTER);

    // the split line's row, and the new line's (the spare, or the one whose
    // line went out at the bottom), took new strings: two measures
    size_t changed=0;
    for(controls::Base *row : editor->getRows())
    {
        controls::CodeLine *line=gweni_cast<controls::CodeLine>(row);

        if(line->getText() != before[line])
            ++changed;
        if(!line->hidden())
            CHECK(line->getText() == editor->getDocument().line(line->getLine()));
    }
    CHECK(changed == 2);
    CHECK(s.renderer.measures-measures == 2);
    CHECK(editor->rowForLine(middle)->getText() == "line");
    CHECK(editor->rowForLine(middle+1)->getText() == " 105");
    CHECK(editor->rowForLine(middle)->isCurrentLine() == false);
    CHECK(editor->rowForLine(middle+1)->isCurrentLine());
}
