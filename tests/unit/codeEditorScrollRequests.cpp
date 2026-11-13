/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The latest scroll request in a frame decides a CodeEditor's view, wrapped
 *  or not (#661). VirtualList::scrollRequestSerial() counts scrollToItem(),
 *  scrollByRows(), setVerticalScrollOffset() and a wheel notch that moved,
 *  and no edit or layout. Two caret moves in one frame, the first far
 *  (Ctrl+End, a setCaret()) and the second back inside the view (Ctrl+Home,
 *  a setCaret()), end with the second's line on screen; an app's
 *  scrollToItem() followed by a caret move inside the view leaves the view
 *  where it was. Wrapped, a resize frame keeps an app's scrollToItem(),
 *  scrollByRows() or setVerticalScrollOffset() made in it over the resize's
 *  own keeping of the caret's line on screen (as unwrapped, which has none).
 *  An app's edit merging the caret's line into the one above, after a caret
 *  move in the same frame, puts the line the caret is on now in view, not
 *  the line after it that the list's request was moved to.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>
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

    explicit Scene(bool wrap, size_t lines=5000)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 600);
        glfw.initialize(canvas);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setPos(0, 0);
        editor->setSize(200, 200);
        editor->setText(shortLines(lines));
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
        editor->setVerticalScrollOffset(editor->getVerticalScrollOffset());
        CHECK(editor->scrollRequestSerial() == ++serial);
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
