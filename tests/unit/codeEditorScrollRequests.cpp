/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The latest scroll request in a frame decides a CodeEditor's view, wrapped
 *  or not (#661). VirtualList::scrollRequestSerial() counts scrollToItem(),
 *  scrollByRows(), a setVerticalScrollOffset() that moves the offset or
 *  drops a pending item (one that does neither is none) and a wheel notch
 *  that moved, and no edit or layout. Two caret moves in one frame, the
 *  first far (Ctrl+End, a setCaret()) and the second back inside the view
 *  (Ctrl+Home, a setCaret()), end with the second's line on screen; an app's
 *  scrollToItem() followed by a caret move inside the view leaves the view
 *  where it was. After a far caret move in one frame, an app's
 *  setVerticalScrollOffset(), a wheel notch and a move of the bar (as its
 *  arrows and track make it) each decide the view; a caret move after the
 *  bar was moved in one frame puts its line on screen. Wrapped, a resize
 *  frame keeps an app's scrollToItem(), scrollByRows() or
 *  setVerticalScrollOffset(), or the bar moved, in it over the resize's own
 *  keeping of the caret's line on screen (as unwrapped, which has none), and
 *  a caret move by offset to a line no row showed, in a resize frame that
 *  wraps it into more rows, ends with its line on screen. An app's edit
 *  merging the caret's line into the one above, after a caret move in the
 *  same frame, puts the line the caret is on now in view, not the line after
 *  it that the list's request was moved to.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>
#include <gweni/controls/verticalScrollBar.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>

#include <doctest/doctest.h>

#include <string>

using namespace gweni;

namespace
{

//! Lines of a few characters, none wrapping at the scenes' widths.
String shortLines(size_t lines)
{
    String text;

    for(size_t i=0; i < lines; ++i)
        text+=(i > 0?"\n":"")+std::to_string(i);
    return text;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CodeEditor *editor=nullptr;
    input::GLFW3 glfw;

    explicit Scene(bool wrap, size_t lines=5000): Scene(wrap, shortLines(lines), 200) {}

    Scene(bool wrap, const String &text, int width)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 600);
        glfw.initialize(canvas);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setPos(0, 0);
        editor->setSize(width, 200);
        editor->setText(text);
        editor->setWrap(wrap);
        frame();
        editor->focus();
        frame();
        REQUIRE(editor->rowForLine(0));
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

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void glfwKey(int key, int action, int mods)
    {
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, action, mods});
    }

    //! Ctrl+key, with no frame after it.
    void ctrlKey(int key)
    {
        glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, GLFW_MOD_CONTROL);
        glfwKey(key, GLFW_PRESS, GLFW_MOD_CONTROL);
        glfwKey(key, GLFW_RELEASE, GLFW_MOD_CONTROL);
        glfwKey(GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE, 0);
    }

    //! The editor's vertical bar, which a test moves as its arrows, its
    //! track and its thumb do (setScrolledAmount()).
    controls::BaseScrollBar *verticalBar() const
    {
        for(controls::Base *child : editor->getActualChildren())
        {
            if(auto *bar=gweni_cast<controls::VerticalScrollBar>(child))
                return bar;
        }
        return nullptr;
    }

    //! The caret's rect is inside the viewport.
    void checkCaretOnScreen() const
    {
        const Rect bounds=editor->getCaretBounds();
        const Rect &viewport=editor->getViewport();

        CHECK(bounds.h == editor->getLineHeight());
        CHECK(bounds.y >= viewport.y);
        CHECK(bounds.y+bounds.h <= viewport.y+viewport.h);
    }
};

text::Position at(size_t line, size_t byte) { return text::Position{line, byte}; }

}//namespace

TEST_CASE("CodeEditor: the list counts scroll requests, not edits or layouts")
{
    for(bool wrap : {false, true})
    {
        CAPTURE(wrap);
        Scene s(wrap, 400);
        controls::CodeEditor *editor=s.editor;

        editor->scrollToItem(300);
        s.frame();
        REQUIRE(!editor->rowForLine(0));

        uint64_t serial=editor->scrollRequestSerial();
        const int offset=editor->getVerticalScrollOffset();

        // an edit above the view, which moves the offset by its lines, and a
        // layout are not requests
        editor->getDocument().insert(at(0, 0), "x\ny\n");
        CHECK(editor->getVerticalScrollOffset() == offset+2*editor->getLineHeight());
        s.frame();
        editor->getDocument().erase(text::Range{at(0, 0), at(2, 0)});
        s.frame();
        CHECK(editor->getVerticalScrollOffset() == offset);
        CHECK(editor->scrollRequestSerial() == serial);

        editor->scrollToItem(200);
        CHECK(editor->scrollRequestSerial() == ++serial);
        editor->scrollToItem(200);
        CHECK(editor->scrollRequestSerial() == ++serial);
        s.frame();
        CHECK(editor->scrollRequestSerial() == serial);

        editor->scrollByRows(-3);
        CHECK(editor->scrollRequestSerial() == ++serial);
        s.frame();
        // an offset that moves nothing and replaces no pending item is none;
        // one replacing a pending item is one, and drops it
        editor->setVerticalScrollOffset(editor->getVerticalScrollOffset());
        CHECK(editor->scrollRequestSerial() == serial);
        editor->scrollToItem(10);
        editor->setVerticalScrollOffset(editor->getVerticalScrollOffset());
        serial+=2;
        CHECK(editor->scrollRequestSerial() == serial);
        CHECK(editor->getPendingItem() == controls::VirtualList::NoItem);
        s.frame();

        // a wheel notch the list takes, and none for one at the top
        CHECK(editor->onMouseWheeled(-60));
        CHECK(editor->scrollRequestSerial() == ++serial);
        s.frame();
        editor->setVerticalScrollOffset(0);
        ++serial;
        s.frame();
        editor->onMouseWheeled(60);
        CHECK(editor->scrollRequestSerial() == serial);
    }
}

TEST_CASE("CodeEditor: of two caret moves in one frame the later one decides the view")
{
    for(bool wrap : {false, true})
    {
        CAPTURE(wrap);
        Scene s(wrap);
        controls::CodeEditor *editor=s.editor;

        // far and back in one frame, by the keys and by the app
        s.ctrlKey(GLFW_KEY_END);
        s.ctrlKey(GLFW_KEY_HOME);
        s.frame();
        s.frame();
        CHECK(editor->getCaret() == at(0, 0));
        CHECK(editor->rowForLine(0));
        CHECK(!editor->rowForLine(4999));
        s.checkCaretOnScreen();

        editor->setCaret(at(4999, 0));
        editor->setCaret(at(3, 0));
        s.frame();
        s.frame();
        CHECK(editor->rowForLine(3));
        CHECK(!editor->rowForLine(4999));
        s.checkCaretOnScreen();

        // back and far: the far one
        s.ctrlKey(GLFW_KEY_HOME);
        s.ctrlKey(GLFW_KEY_END);
        s.frame();
        s.frame();
        CHECK(editor->rowForLine(4999));
        s.checkCaretOnScreen();

        // and from the end, a move up far and one back inside the view
        editor->setCaret(at(10, 0));
        editor->setCaret(at(4995, 0));
        s.frame();
        s.frame();
        CHECK(editor->rowForLine(4995));
        CHECK(!editor->rowForLine(10));
        s.checkCaretOnScreen();

        // an app's scrollToItem(), then a caret move inside the view: the
        // caret's, the later, keeps the view
        editor->scrollToItem(2000);
        editor->setCaret(at(4996, 0));
        s.frame();
        s.frame();
        CHECK(editor->rowForLine(4996));
        CHECK(!editor->rowForLine(2000));
        s.checkCaretOnScreen();
    }
}

TEST_CASE("CodeEditor: a resize frame keeps a scroll the app asked for in it")
{
    for(bool wrap : {false, true})
    {
        CAPTURE(wrap);
        Scene s(wrap);
        controls::CodeEditor *editor=s.editor;

        // the caret's line on screen, as a resize keeps it
        editor->setCaret(at(5, 0));
        s.frame();
        REQUIRE(editor->rowForLine(5));

        editor->setSize(160, 180);
        editor->scrollToItem(3000);
        s.frame();
        s.frame();
        CHECK(editor->rowForLine(3000));
        CHECK(!editor->rowForLine(5));

        // with a scrollByRows()
        editor->setCaret(at(3000, 0));
        s.frame();
        REQUIRE(editor->rowForLine(3000));
        editor->setSize(200, 200);
        editor->scrollByRows(-500);
        s.frame();
        s.frame();
        CHECK(editor->firstVisibleItem() < 3000-400);
        CHECK(!editor->rowForLine(3000));

        // with an offset
        editor->setCaret(at(editor->firstVisibleItem()+2, 0));
        s.frame();
        editor->setSize(160, 180);
        editor->setVerticalScrollOffset(100*editor->getLineHeight());
        s.frame();
        s.frame();
        CHECK(editor->firstVisibleItem() == 100);

        // with a move of the bar by the user (its arrows, its track)
        editor->setCaret(at(102, 0));
        s.frame();
        REQUIRE(editor->rowForLine(102));
        controls::BaseScrollBar *bar=s.verticalBar();
        REQUIRE(bar);
        editor->setSize(200, 200);
        bar->setScrolledAmount(0.5f, true);
        s.frame();
        s.frame();
        CHECK(editor->firstVisibleItem() > 2400);
        CHECK(editor->firstVisibleItem() < 2600);
        CHECK(!editor->rowForLine(102));
        editor->setVerticalScrollOffset(100*editor->getLineHeight());
        s.frame();

        // with none, the caret's line stays on screen
        editor->setCaret(at(104, 0));
        s.frame();
        editor->setSize(200, 200);
        s.frame();
        CHECK(editor->rowForLine(104));
        s.checkCaretOnScreen();
    }
}

TEST_CASE("CodeEditor: an app's edit merging the caret's line into the one above keeps its line in view")
{
    for(bool wrap : {false, true})
    {
        CAPTURE(wrap);
        Scene s(wrap);
        controls::CodeEditor *editor=s.editor;
        text::Document &document=editor->getDocument();

        // the view at the end, a caret move up to a line out of view, then
        // the app joins that line to the one above in the same frame
        editor->setCaret(at(4999, 0));
        s.frame();
        s.frame();
        REQUIRE(editor->rowForLine(4999));

        editor->setCaret(at(3000, 0));
        document.erase(text::Range{at(2999, document.line(2999).size()), at(3000, 0)});
        CHECK(editor->getCaret().line == 2999);
        s.frame();
        s.frame();
        CHECK(editor->rowForLine(2999));
        s.checkCaretOnScreen();
    }
}

TEST_CASE("CodeEditor: a scroll the app or the user makes after a caret move in one frame decides the view")
{
    for(bool wrap : {false, true})
    {
        CAPTURE(wrap);
        Scene s(wrap);
        controls::CodeEditor *editor=s.editor;
        const int lineHeight=editor->getLineHeight();

        // the app's offset after a far move
        editor->setCaret(at(4999, 0));
        editor->setVerticalScrollOffset(100*lineHeight);
        s.frame();
        s.frame();
        CHECK(editor->firstVisibleItem() == 100);
        CHECK(!editor->rowForLine(4999));

        // a wheel notch up after a far move, from the caret at line 2000
        editor->setCaret(at(2000, 0));
        s.frame();
        s.frame();
        REQUIRE(editor->rowForLine(2000));
        const size_t first=editor->firstVisibleItem();

        editor->setCaret(at(4999, 0));
        CHECK(editor->onMouseWheeled(60));
        s.frame();
        s.frame();
        CHECK(editor->firstVisibleItem() == first-3);
        CHECK(!editor->rowForLine(4999));

        // the bar moved by the user after a far move
        controls::BaseScrollBar *bar=s.verticalBar();
        REQUIRE(bar);
        editor->setCaret(at(4999, 0));
        bar->setScrolledAmount(0.25f, true);
        s.frame();
        s.frame();
        CHECK(editor->firstVisibleItem() > 1150);
        CHECK(editor->firstVisibleItem() < 1350);
        CHECK(!editor->rowForLine(4999));
    }
}

TEST_CASE("CodeEditor: a caret move after the user moved the bar in one frame decides the view")
{
    for(bool wrap : {false, true})
    {
        CAPTURE(wrap);
        Scene s(wrap);
        controls::CodeEditor *editor=s.editor;
        controls::BaseScrollBar *bar=s.verticalBar();
        REQUIRE(bar);

        // the bar moved half way down, then the caret moved to a line the
        // view showed before the bar moved
        bar->setScrolledAmount(0.5f, true);
        editor->setCaret(at(5, 0));
        s.frame();
        s.frame();
        CHECK(editor->rowForLine(5));
        s.checkCaretOnScreen();
    }
}

TEST_CASE("CodeEditor wrapped: a caret move by offset in a resize frame puts its row in view at the new width")
{
    // lines of a third of the viewport's width less a little at 600 units
    // wide, wrapping into two or three rows at 200
    Scene probe(true, shortLines(1), 600);
    const float cell=probe.editor->getCellWidth();
    REQUIRE(cell > 0.f);
    const size_t cells=static_cast<size_t>(300/cell);
    String text;

    for(size_t i=0; i < 200; ++i)
    {
        String line=std::to_string(i);

        while(line.size()+5 < cells)
            line+=" abcd";
        text+=(i > 0?"\n":"")+line;
    }

    Scene s(true, text, 600);
    controls::CodeEditor *editor=s.editor;

    // every line measured (the idle re-measure), so a move goes by offset
    for(int i=0; i < 10; ++i)
        s.frame();
    REQUIRE(editor->rowForLine(0));
    REQUIRE(editor->rowForLine(0)->getRowCount() == 1);

    // the line after the last one any row shows
    size_t line=0;

    while(editor->rowForLine(line))
        ++line;
    REQUIRE(line < 100);

    editor->setCaret(at(line, 0));
    editor->setSize(200, 200);
    s.frame();
    s.frame();
    REQUIRE(editor->rowForLine(line));
    CHECK(editor->rowForLine(line)->getRowCount() > 1);
    s.checkCaretOnScreen();
}
