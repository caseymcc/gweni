/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  LogView's whole-row selection, on a real Canvas with the StubSkin, the
 *  mouse and keys through the GLFW translator and a stepped clock. The
 *  selection is ranges of (row id, character) ends, a whole row from 0 to its
 *  rowText()'s length; its text is each selected row's text in row order,
 *  joined by '\n', a collapsed block's every line included. It keeps the
 *  rows that remain when the cap drops some (a range all dropped is gone),
 *  and a replace clamps the characters on that row (a range's high end at the
 *  row's end stays at its end, its low end where it was, so a selected empty
 *  row selects its new text). A click selects a row, Shift+click extends to a row,
 *  Ctrl+click toggles one (splitting a range round it), Ctrl+A selects every
 *  row, Escape clears and a click under the last row clears; Ctrl+C and the
 *  context menu's Copy put the text on the clipboard, a right press on an
 *  unselected row selecting it first. A press-drag selects from the pressed
 *  character to the one under the mouse (here a row's start to a row's end;
 *  logViewCharSelection.cpp has the characters), and past the viewport's bottom or top the
 *  view scrolls on the clock and the selection follows (a scroll up
 *  detaching the view, but in a log whose rows all fit, which has nothing to
 *  scroll and keeps following, telling nothing). The rows on screen are marked selected exactly when
 *  their row is, through scrolling off screen and back and through appends;
 *  a selection alone does not detach the view. Every frame is one layout pass.
 */
#include "headlessFixture.h"

#include <gweni/anim.h>
#include <gweni/controls/logView.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

using RowId=controls::LogView::RowId;

std::vector<String> blockLines(size_t count)
{
    std::vector<String> lines;

    for(size_t i=0; i < count; ++i)
        lines.push_back("line "+std::to_string(i));
    return lines;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::LogView *log=nullptr;
    input::GLFW3 glfw;
    float clock=100.0f;
    int selectionEvents=0;

    explicit Scene(int rows=60)
    {
        platform::setTimeSource([this]() { return clock; });

        canvas=new controls::Canvas(&skin);
        canvas->setSize(500, 400);
        glfw.initialize(canvas);

        log=canvas->newChild<controls::LogView>();
        log->setPos(10, 10);
        log->setSize(300, 200);
        log->setRemeasureBudget(0);
        log->onSelectionChanged.addCallback(canvas, [this](event::Handler &, event::Info) { ++selectionEvents; });

        for(int i=0; i < rows; ++i)
            log->append(controls::LogRow("row "+std::to_string(i)));
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

        const controls::LayoutStats &layout=canvas->getLayoutStats();

        CHECK(layout.requests == 0);
        CHECK(layout.passes <= 1);
        CHECK(!layout.renderLayout);
    }

    void tick()
    {
        clock+=1.0f/60.0f;
        frame();
    }

    void glfwKey(int key, int action, int mods)
    {
        glfw.processKeyEvent(input::GLFW3::KeyEvent{key, 0, action, mods});
    }

    void holdModifiers(int mods, bool down)
    {
        if(mods & GLFW_MOD_CONTROL)
            glfwKey(GLFW_KEY_LEFT_CONTROL, down?GLFW_PRESS:GLFW_RELEASE, down?GLFW_MOD_CONTROL:0);
        if(mods & GLFW_MOD_SHIFT)
            glfwKey(GLFW_KEY_LEFT_SHIFT, down?GLFW_PRESS:GLFW_RELEASE, down?mods:0);
    }

    void key(int key, int mods=0)
    {
        holdModifiers(mods, true);
        glfwKey(key, GLFW_PRESS, mods);
        glfwKey(key, GLFW_RELEASE, mods);
        holdModifiers(mods, false);
        frame();
    }

    //! The canvas point left of the row's text, at its first line: its start.
    Point rowStart(RowId id) const
    {
        controls::Base *row=log->rowControl(id);

        REQUIRE(row != nullptr);
        return row->localPosToCanvas(Point(1, row->getHeight()/2));
    }

    //! The canvas point in the middle of the row on screen.
    Point rowPoint(RowId id) const
    {
        controls::Base *row=log->rowControl(id);

        REQUIRE(row != nullptr);
        return row->localPosToCanvas(Point(row->getWidth()/2, row->getHeight()/2));
    }

    void move(const Point &at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
    }

    void click(const Point &at, int mods=0, int button=0)
    {
        holdModifiers(mods, true);
        move(at);
        canvas->inputMouseButton(button, true);
        canvas->inputMouseButton(button, false);
        holdModifiers(mods, false);
        frame();
    }

    void clickRow(RowId id, int mods=0)
    {
        click(rowPoint(id), mods);
    }

    std::vector<RowId> selectedOnScreen() const
    {
        std::vector<RowId> rows;

        for(size_t i=0; i < log->getRowCount(); ++i)
        {
            controls::internal::LogViewRow *row=log->rowControl(log->rowIdAt(i));

            if(row && row->isSelected())
                rows.push_back(log->rowIdAt(i));
        }
        return rows;
    }

    //! Every row on screen is marked exactly when its row is selected.
    void checkMarks() const
    {
        for(size_t i=0; i < log->getRowCount(); ++i)
        {
            const RowId id=log->rowIdAt(i);

            if(controls::internal::LogViewRow *row=log->rowControl(id))
            {
                CAPTURE(id);
                CHECK(row->isSelected() == log->isRowSelected(id));
            }
        }
    }

    static String rows(int first, int last)
    {
        String text;

        for(int i=first; i <= last; ++i)
            text+=(i == first?"":"\n")+String("row ")+std::to_string(i);
        return text;
    }
};

controls::MenuItem *findItem(controls::Base *control, const String &text)
{
    if(controls::MenuItem *item=gweni_cast<controls::MenuItem>(control))
    {
        if(item->getText() == text)
            return item;
    }
    for(controls::Base *child : control->getChildren())
    {
        if(controls::MenuItem *found=findItem(child, text))
            return found;
    }
    return nullptr;
}

}//namespace

TEST_CASE("LogView selection: the text of whole rows and of a collapsed block")
{
    Scene scene(0);
    const RowId a=scene.log->append(controls::LogRow("alpha"));
    const RowId b=scene.log->append(controls::LogRow("beta"));
    const RowId block=scene.log->append(controls::LogRow::block("Bash ls", blockLines(10)));
    const RowId d=scene.log->append(controls::LogRow("delta"));

    scene.frame();
    CHECK_FALSE(scene.log->hasSelection());
    CHECK(scene.log->getSelectedText().empty());

    scene.log->selectRow(b);
    CHECK(scene.log->getSelectedText() == "beta");
    REQUIRE(scene.log->getSelection().size() == 1);
    CHECK(scene.log->getSelection()[0].anchor.character == 0);
    CHECK(scene.log->getSelection()[0].focus.character == 4);

    // the block copies its title and every line though it shows four
    scene.log->extendSelectionTo(d);

    String expected="beta\nBash ls";

    for(int i=0; i < 10; ++i)
        expected+="\nline "+std::to_string(i);
    expected+="\ndelta";
    CHECK(scene.log->getSelectedText() == expected);
    CHECK(scene.log->getRow(block)->collapsed);
    CHECK(scene.log->rowText(block) == expected.substr(5, expected.size()-5-6));

    // extended the other way: both ends whole
    scene.log->selectRow(block);
    scene.log->extendSelectionTo(a);
    CHECK(scene.log->getSelectedText() == "alpha\nbeta\n"+scene.log->rowText(block));
    CHECK_FALSE(scene.log->isRowSelected(d));
    CHECK(scene.log->isRowSelected(a));
}

TEST_CASE("LogView selection: a selection that lost dropped rows keeps those that remain")
{
    Scene scene(10);

    scene.log->selectRow(2);
    scene.log->extendSelectionTo(6);
    CHECK(scene.log->getSelectedText() == Scene::rows(2, 6));

    const int events=scene.selectionEvents;

    // rows 0..4 dropped: 5 and 6 stay selected, the range from row 5's start
    scene.log->setMaxRows(6);
    scene.log->append(controls::LogRow("row 10"));
    CHECK(scene.log->firstRowId() == 5);
    CHECK(scene.log->getSelectedText() == Scene::rows(5, 6));
    REQUIRE(scene.log->getSelection().size() == 1);
    CHECK(scene.log->getSelection()[0].anchor.row == 5);
    CHECK(scene.log->getSelection()[0].anchor.character == 0);
    CHECK(scene.selectionEvents == events+1);
    scene.frame();
    scene.checkMarks();

    // every row of it dropped: no selection
    for(int i=0; i < 3; ++i)
        scene.log->append(controls::LogRow("more"));
    CHECK_FALSE(scene.log->hasSelection());
    CHECK(scene.log->getSelectedText().empty());
}

TEST_CASE("LogView selection: a replace clamps the characters on its row")
{
    Scene scene(10);

    scene.log->selectRow(4);
    CHECK(scene.log->replace(4, controls::LogRow("row 4 is longer now")));
    // an end at the row's end stays there
    CHECK(scene.log->getSelectedText() == "row 4 is longer now");
    CHECK(scene.log->replace(4, controls::LogRow("r4")));
    CHECK(scene.log->getSelectedText() == "r4");
    REQUIRE(scene.log->getSelection().size() == 1);
    CHECK(scene.log->getSelection()[0].focus.character == 2);

    // a replace of another row leaves it be
    CHECK(scene.log->replace(5, controls::LogRow("five")));
    CHECK(scene.log->getSelectedText() == "r4");

    // and clear() clears it
    scene.log->clear();
    CHECK_FALSE(scene.log->hasSelection());
}

TEST_CASE("LogView selection: a replace of a selected empty row selects its new text")
{
    Scene scene(0);
    const controls::LogView::RowId empty=scene.log->append(controls::LogRow(""));
    const controls::LogView::RowId after=scene.log->append(controls::LogRow("after"));

    scene.frame();

    // both ends at 0, the row's end: only the high one goes to the new end
    scene.log->selectRow(empty);
    CHECK(scene.log->replace(empty, controls::LogRow("hello")));
    CHECK(scene.log->getSelectedText() == "hello");
    REQUIRE(scene.log->getSelection().size() == 1);
    CHECK(scene.log->getSelection()[0].anchor.character == 0);
    CHECK(scene.log->getSelection()[0].focus.character == 5);

    // a range from it to the next row keeps its low end at 0
    CHECK(scene.log->replace(empty, controls::LogRow("")));
    scene.log->selectRow(empty);
    scene.log->extendSelectionTo(after);
    CHECK(scene.log->replace(empty, controls::LogRow("hello")));
    CHECK(scene.log->getSelectedText() == "hello\nafter");
}

TEST_CASE("LogView selection: click Shift+click Ctrl+click Ctrl+A Escape and Ctrl+C")
{
    Scene scene;

    CHECK(scene.log->scrollToRow(20));
    scene.frame();
    scene.frame();

    scene.clickRow(21);
    CHECK(scene.log->getSelectedText() == "row 21");
    CHECK(scene.selectedOnScreen() == std::vector<RowId>{21});
    CHECK(scene.log->isFocussed());

    scene.clickRow(24, GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == Scene::rows(21, 24));

    // Ctrl+click in it splits it; on a row outside it adds that row
    scene.clickRow(22, GLFW_MOD_CONTROL);
    CHECK(scene.log->getSelectedText() == "row 21\nrow 23\nrow 24");
    scene.clickRow(26, GLFW_MOD_CONTROL);
    CHECK(scene.log->getSelectedText() == "row 21\nrow 23\nrow 24\nrow 26");
    CHECK(scene.selectedOnScreen() == (std::vector<RowId>{21, 23, 24, 26}));
    scene.checkMarks();

    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == "row 21\nrow 23\nrow 24\nrow 26");

    // a plain click: that row alone
    scene.clickRow(23);
    CHECK(scene.log->getSelectedText() == "row 23");

    scene.key(GLFW_KEY_A, GLFW_MOD_CONTROL);
    CHECK(scene.log->getSelectedText() == Scene::rows(0, 59));
    scene.checkMarks();

    scene.key(GLFW_KEY_ESCAPE);
    CHECK_FALSE(scene.log->hasSelection());
    CHECK(scene.selectedOnScreen().empty());

    // a click under the last row clears too
    scene.clickRow(23);
    REQUIRE(scene.log->hasSelection());
    scene.log->setFollowing(true);
    scene.frame();

    Scene small(3);

    small.clickRow(1);
    REQUIRE(small.log->hasSelection());

    const Point below=small.log->localPosToCanvas(Point(50, small.log->getHeight()-10));

    small.click(below);
    CHECK_FALSE(small.log->hasSelection());
}

TEST_CASE("LogView selection: the context menu's Copy and a right press on a row")
{
    Scene scene;

    CHECK(scene.log->scrollToRow(30));
    scene.frame();
    scene.frame();

    scene.clickRow(31);
    scene.clickRow(33, GLFW_MOD_SHIFT);

    // a right press inside the selection keeps it
    scene.click(scene.rowPoint(32), 0, 1);

    controls::Menu *menu=scene.log->getContextMenu();

    REQUIRE(menu != nullptr);
    CHECK(scene.canvas->shownContextMenu() == menu);
    CHECK(scene.log->getSelectedText() == Scene::rows(31, 33));

    controls::MenuItem *copy=findItem(menu, "Copy");

    REQUIRE(copy != nullptr);
    CHECK_FALSE(copy->isDisabled());

    platform::setClipboardText("");
    scene.frame();
    scene.click(copy->localPosToCanvas(Point(copy->getWidth()/2, copy->getHeight()/2)));
    CHECK(platform::getClipboardText() == Scene::rows(31, 33));
    CHECK_FALSE(scene.canvas->shownContextMenu());

    // a right press on a row outside it selects that row
    scene.click(scene.rowPoint(36), 0, 1);
    CHECK(scene.log->getSelectedText() == "row 36");
    CHECK(scene.canvas->shownContextMenu() == menu);
}

TEST_CASE("LogView selection: a drag selects to the row under the mouse and scrolls past the edges")
{
    Scene scene(200);

    CHECK(scene.log->scrollToRow(100));
    scene.frame();
    scene.frame();

    scene.move(scene.rowStart(101));
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    scene.move(scene.rowPoint(104));
    scene.frame();
    CHECK(scene.log->getSelectedText() == Scene::rows(101, 104));

    // below the viewport: it scrolls down on the clock, the selection with it
    const Rect &viewport=scene.log->getViewport();
    const Point below=scene.log->localPosToCanvas(Point(viewport.x+40, viewport.y+viewport.h+20));
    const int offset=scene.log->getVerticalScrollOffset();

    scene.move(below);
    for(int i=0; i < 20; ++i)
        scene.tick();
    CHECK(scene.log->getVerticalScrollOffset() > offset);

    const auto &range=scene.log->getSelection().back();

    CHECK(range.anchor.row == 101);
    CHECK(range.focus.row > 104);
    scene.checkMarks();

    // still selected rows are marked once scrolled back
    scene.canvas->inputMouseButton(0, false);
    scene.frame();

    const RowId focus=range.focus.row;

    for(int i=0; i < 5; ++i)
        scene.tick();
    CHECK(scene.log->getSelection().back().focus.row == focus);
    CHECK(scene.log->scrollToRow(150));
    scene.frame();
    scene.frame();
    CHECK(scene.selectedOnScreen().empty());
    CHECK(scene.log->scrollToRow(101));
    scene.frame();
    scene.frame();
    CHECK(!scene.selectedOnScreen().empty());
    scene.checkMarks();

    // following, a drag above the top scrolls up and detaches the view
    scene.log->setFollowing(true);
    scene.frame();
    scene.frame();

    const RowId last=scene.log->lastRowId();

    scene.move(scene.rowPoint(last));
    scene.canvas->inputMouseButton(0, true);
    scene.frame();

    const Point above=scene.log->localPosToCanvas(Point(viewport.x+40, viewport.y-15));

    scene.move(above);
    for(int i=0; i < 10; ++i)
        scene.tick();
    CHECK_FALSE(scene.log->isFollowing());
    CHECK(scene.log->getSelection().back().anchor.row == last);
    CHECK(scene.log->getSelection().back().focus.row < last-10);
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
}

// A drag above the top of a log whose rows all fit has nothing to scroll: it
// selects up to the first row and the view keeps following, telling nothing.
TEST_CASE("LogView selection: a drag above the top of a log that fits keeps following")
{
    Scene scene(3);
    int followEvents=0;

    scene.log->onFollowingChanged.addCallback(scene.canvas, [&](event::Handler &, event::Info) { ++followEvents; });
    REQUIRE(scene.log->getMaxVerticalScrollOffset() == 0);

    const RowId last=scene.log->lastRowId();
    const Rect &viewport=scene.log->getViewport();

    scene.move(scene.rowPoint(last));
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    scene.move(scene.log->localPosToCanvas(Point(viewport.x+40, viewport.y-15)));
    for(int i=0; i < 10; ++i)
        scene.tick();
    CHECK(scene.log->isFollowing());
    CHECK(followEvents == 0);
    CHECK(scene.log->getJumpButton() == nullptr);
    CHECK(scene.log->getSelection().back().anchor.row == last);
    CHECK(scene.log->getSelection().back().focus.row == scene.log->firstRowId());
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
}

TEST_CASE("LogView selection: a selection alone does not detach the view")
{
    Scene scene;

    scene.clickRow(scene.log->lastRowId());
    CHECK(scene.log->isFollowing());

    const RowId selected=scene.log->lastRowId();

    for(int i=0; i < 4; ++i)
        scene.log->append(controls::LogRow("new "+std::to_string(i)));
    scene.frame();
    CHECK(scene.log->isFollowing());
    CHECK(scene.log->isRowSelected(selected));
    CHECK(scene.log->getSelectedText() == "row 59");
    CHECK(scene.selectedOnScreen() == std::vector<RowId>{selected});
    scene.checkMarks();
}
