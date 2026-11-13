/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeView, the read-only code view, on the StubSkin with a stepped clock
 *  and an in-test clipboard (platform::setClipboardBackend). It is read-only
 *  from its construction: focused it draws no caret and no current line, and
 *  typing, Return, Backspace, Delete, Tab, Ctrl+V, Ctrl+X and Ctrl+Z leave
 *  its text as it was. A drag, Shift+click, a double-click and Ctrl+A
 *  select, and Ctrl+C puts exactly the selection on the clipboard. Tab and
 *  Shift+Tab move the focus through it as through any field. It sizes to its
 *  content: placed with no size, 1 and 10 lines make a viewport exactly
 *  their lines high and as wide as the editor's content (its longest line's
 *  cells, the one after them and TextInset on both sides), with no bar and
 *  nothing to scroll; 1000 lines docked Center to fill a 300x200 panel take the
 *  panel and scroll vertically, and in a VerticalLayout cell a long one is
 *  cut to the cell and scrolls there. Docked Top in a column narrower than
 *  its one line, it is that line high plus the horizontal bar, so the line
 *  shows above the bar with nothing to scroll vertically.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeView.h>
#include <gweni/controls/layout/layout.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <cmath>
#include <string>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    input::GLFW3 glfw;
    float clock=100.0f;
    String clipboard;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        platform::setClipboardBackend([this]() { return clipboard; },
            [this](const String &text) { clipboard=text; return true; });

        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        glfw.initialize(canvas);
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
        platform::setClipboardBackend(nullptr, nullptr, nullptr);
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

    void type(const String &text)
    {
        for(char c : text)
        {
            glfw.processCharEvent(static_cast<unsigned char>(c));
            frame();
        }
    }

    //! The mouse to a point in \p control's coordinates.
    void move(controls::Base *control, int x, int y)
    {
        const Point at=control->localPosToCanvas(Point(x, y));

        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    void button(bool down)
    {
        canvas->inputMouseButton(0, down);
        frame();
    }

    void click(controls::Base *control, int x, int y)
    {
        move(control, x, y);
        button(true);
        button(false);
    }

    void shiftClick(controls::Base *control, int x, int y)
    {
        move(control, x, y);
        glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_PRESS, GLFW_MOD_SHIFT);
        button(true);
        button(false);
        glfwKey(GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE, 0);
        frame();
    }
};

//! The point in \p view's coordinates of cell \p column's middle on line
//! \p line (no scroll).
Point cellPoint(controls::CodeView *view, size_t line, size_t column)
{
    const Rect &viewport=view->getViewport();
    const int x=viewport.x+view->columnX(column)+static_cast<int>(view->getCellWidth()/2);
    const int y=viewport.y+static_cast<int>(line)*view->getLineHeight()+view->getLineHeight()/2;

    return Point(x, y);
}

String numberedLines(size_t count)
{
    String text;

    for(size_t i=0; i < count; ++i)
    {
        if(i > 0)
            text+='\n';
        text+="line "+std::to_string(i);
    }
    return text;
}

//! Whether either of \p view's bars shows: the viewport is the view less its
//! padding and the bars that show (the editor's inner panel has no margin).
bool barShown(controls::CodeView *view)
{
    const Rect &viewport=view->getViewport();
    const int padding=2*controls::CodeEditor::FieldPadding;

    return viewport.w+padding < view->getWidth() || viewport.h+padding < view->getHeight();
}

}//namespace

TEST_CASE("CodeView is read-only: no caret, no current line, and no key or clipboard edit changes its text")
{
    Scene scene;
    controls::CodeView *view=scene.canvas->newChild<controls::CodeView>();
    const String text="first line\nsecond line\nthird";

    view->setPos(10, 10);
    view->setText(text);
    scene.frame();

    CHECK(view->isReadOnly());
    CHECK(!view->hasLineNumbers());

    view->focus();
    scene.frame();
    REQUIRE(KeyboardFocus == view);

    CHECK(!view->isCaretVisible());
    CHECK(view->getCaretBounds().w == 0);
    for(controls::Base *row : view->getRows())
    {
        if(!row->hidden())
            CHECK(!gweni_cast<controls::CodeLine>(row)->isCurrentLine());
    }

    scene.clipboard="pasted";
    scene.type("xyz");
    scene.key(GLFW_KEY_ENTER);
    scene.key(GLFW_KEY_BACKSPACE);
    scene.key(GLFW_KEY_DELETE);
    scene.key(GLFW_KEY_V, GLFW_MOD_CONTROL);
    scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
    scene.key(GLFW_KEY_X, GLFW_MOD_CONTROL);
    scene.key(GLFW_KEY_Z, GLFW_MOD_CONTROL);
    view->insertText("inserted");
    scene.frame();

    CHECK(view->getText() == text);
    CHECK(!view->undo());
    CHECK(!view->isCaretVisible());
}

TEST_CASE("CodeView selects by a drag, Shift+click, a double-click and Ctrl+A, and Ctrl+C copies the selection")
{
    Scene scene;
    controls::CodeView *view=scene.canvas->newChild<controls::CodeView>();

    view->setPos(10, 10);
    view->setText("alpha beta\ngamma delta\nepsilon");
    scene.frame();
    REQUIRE(view->getLineHeight() > 0);

    // a drag from line 0's "beta" to line 1's "gamma" (before its 4th cell)
    Point from=cellPoint(view, 0, 6);
    Point to=cellPoint(view, 1, 3);

    scene.move(view, from.x-static_cast<int>(view->getCellWidth()/2), from.y);
    scene.button(true);
    scene.move(view, to.x-static_cast<int>(view->getCellWidth()/2), to.y);
    scene.button(false);
    REQUIRE(KeyboardFocus == view);
    CHECK(view->getSelectedText() == "beta\ngam");

    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(scene.clipboard == "beta\ngam");

    // a click and a Shift+click on line 2
    Point start=cellPoint(view, 2, 0);
    Point end=cellPoint(view, 2, 4);

    scene.click(view, start.x-static_cast<int>(view->getCellWidth()/2), start.y);
    CHECK(!view->hasSelection());
    scene.shiftClick(view, end.x-static_cast<int>(view->getCellWidth()/2), end.y);
    CHECK(view->getSelectedText() == "epsi");
    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(scene.clipboard == "epsi");

    // a double-click on "delta"
    Point word=cellPoint(view, 1, 8);

    const Point wordAt=view->localPosToCanvas(word);

    scene.move(view, word.x, word.y);
    view->onMouseDoubleClickLeft(wordAt.x, wordAt.y);
    scene.frame();
    CHECK(view->getSelectedText() == "delta");

    scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(scene.clipboard == view->getText());
}

TEST_CASE("CodeView: Tab and Shift+Tab move the focus through it")
{
    Scene scene;
    controls::TextBox *before=scene.canvas->newChild<controls::TextBox>();
    controls::CodeView *view=scene.canvas->newChild<controls::CodeView>();
    controls::TextBox *after=scene.canvas->newChild<controls::TextBox>();

    before->setPos(10, 10);
    view->setPos(10, 40);
    view->setText("key: value\nother: 1");
    after->setPos(10, 200);
    scene.frame();

    before->focus();
    scene.frame();
    REQUIRE(KeyboardFocus == before);

    scene.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == view);
    scene.key(GLFW_KEY_TAB);
    CHECK(KeyboardFocus == after);
    scene.key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    CHECK(KeyboardFocus == view);
    scene.key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    CHECK(KeyboardFocus == before);
    CHECK(view->getText() == "key: value\nother: 1");
}

TEST_CASE("CodeView sizes to its content: 1 and 10 lines are a viewport exactly their size, with no bar")
{
    for(size_t lines : {size_t(1), size_t(10)})
    {
        CAPTURE(lines);
        Scene scene;
        controls::CodeView *view=scene.canvas->newChild<controls::CodeView>();

        view->setPos(10, 10);
        view->setText(numberedLines(lines));
        scene.frame();
        scene.frame();
        REQUIRE(view->getLineHeight() > 0);

        const Rect &viewport=view->getViewport();
        const int width=2*controls::CodeEditor::TextInset
            +static_cast<int>(std::ceil(static_cast<double>(view->getLongestLineCells()+1)*view->getCellWidth()));

        CHECK(viewport.h == static_cast<int>(lines)*view->getLineHeight());
        CHECK(viewport.w == width);
        CHECK(view->getHeight()-viewport.h == 2*controls::CodeEditor::FieldPadding);
        CHECK(view->getWidth()-viewport.w == 2*controls::CodeEditor::FieldPadding);
        CHECK(!barShown(view));
        CHECK(view->getMaxVerticalScrollOffset() == 0);
        CHECK(view->getMaxHorizontalScrollOffset() == 0);
    }
}

TEST_CASE("CodeView of 1000 lines scrolls in what a dock or a layout gives it")
{
    SUBCASE("1000 lines docked Center in a 300x200 panel")
    {
        Scene scene;
        controls::Base *panel=scene.canvas->newChild<controls::Base>();
        controls::CodeView *view=panel->newChild<controls::CodeView>();

        panel->setBounds(0, 0, 300, 200);
        panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        view->setDock(DockPosition::Center);
        view->setText(numberedLines(1000));
        scene.frame();
        scene.frame();

        CHECK(view->getWidth() == 300);
        CHECK(view->getHeight() == 200);
        CHECK(barShown(view));
        CHECK(view->getMaxVerticalScrollOffset() == 1000*view->getLineHeight()-view->getViewportHeight());
        CHECK(view->getRows().size() < 30);
    }

    SUBCASE("1000 lines in a VerticalLayout cell")
    {
        Scene scene;
        controls::Base *panel=scene.canvas->newChild<controls::Base>();
        controls::CodeView *view=panel->newChild<controls::CodeView>();

        panel->setBounds(0, 0, 300, 200);
        panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        panel->setLayout(new controls::VerticalLayout());
        view->setText(numberedLines(1000));
        scene.frame();
        scene.frame();

        CHECK(view->getHeight() <= 200);
        CHECK(view->getWidth() <= 300);
        CHECK(barShown(view));
        CHECK(view->getMaxVerticalScrollOffset() > 0);
    }
}

TEST_CASE("CodeView docked Top narrower than its one line is that line high plus the horizontal bar")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>();
    controls::CodeView *view=panel->newChild<controls::CodeView>();

    panel->setBounds(0, 0, 120, 200);
    panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    view->setDock(DockPosition::Top);
    view->setText("git clone https://github.com/caseymcc/gweni.git && cd gweni");
    scene.frame();
    scene.frame();
    REQUIRE(view->getLineHeight() > 0);

    CHECK(view->getWidth() == 120);
    CHECK(view->getMaxHorizontalScrollOffset() > 0);
    CHECK(view->getViewportHeight() == view->getLineHeight());
    CHECK(view->getMaxVerticalScrollOffset() == 0);
}
