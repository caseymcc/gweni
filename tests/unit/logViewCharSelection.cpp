/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  LogView's character selection, on a real Canvas with the StubSkin, the
 *  mouse and keys through the GLFW translator and a stepped clock. A range's
 *  text is each row's covered bytes of its rowText(), joined by '\n': inside
 *  one row, across rows, inside a collapsed block (whose shown count maps to
 *  its title's end, shownToRowCharacter(), and whose hidden lines are covered
 *  only by a range that runs past the block's end), the whole block's text for
 *  a range over all of it; a replace clamps the characters on its row, and
 *  the cap and a prepend keep what remains. A plain click selects the row
 *  whole; a press-drag selects from the pressed character to the one under
 *  the mouse (Text::getClosestCharacter, on any wrapped line of the row), the
 *  pressed row whole again back on its character, the row's end past the
 *  viewport's bottom; Shift+click extends the focus to the character clicked,
 *  the anchor kept whole (the clicked row, the pressed character, the
 *  double-clicked word); a double-click selects the word under it, and a drag
 *  from it extends with the word kept; Ctrl+C copies exactly the characters.
 *  Every frame is one layout pass.
 */
#include "headlessFixture.h"

#include <gweni/controls/logView.h>
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
using Position=controls::LogView::Position;

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
    //! A control in the canvas's corner whose clicks end any double-click.
    controls::Base *sink=nullptr;
    input::GLFW3 glfw;
    float clock=100.0f;

    explicit Scene(int rows=60)
    {
        platform::setTimeSource([this]() { return clock; });

        canvas=new controls::Canvas(&skin);
        canvas->setSize(500, 400);
        glfw.initialize(canvas);

        sink=canvas->newChild<controls::Base>();
        sink->setBounds(480, 380, 20, 20);
        sink->setMouseInputEnabled(true);

        log=canvas->newChild<controls::LogView>();
        log->setPos(10, 10);
        log->setSize(300, 200);
        log->setRemeasureBudget(0);

        for(int i=0; i < rows; ++i)
            log->append(controls::LogRow("row "+std::to_string(i)));
        frame();
        forgetClicks();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        forgetClicks();
        glfw.processFocus(false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    //! A click on the sink, so the next press anywhere else is no
    //! double-click of one made before (another test's included).
    void forgetClicks()
    {
        move(Point(490, 390));
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
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

    //! The canvas point of the caret before the shown character (a byte of
    //! the text the row's Label shows), in the middle of its wrapped line.
    Point charPoint(RowId id, size_t shown) const
    {
        controls::internal::LogViewRow *row=log->rowControl(id);

        REQUIRE(row != nullptr);

        const Rect caret=row->getCharacterPosition(static_cast<int>(shown));

        return row->localPosToCanvas(Point(caret.x, caret.y+caret.h/2));
    }

    void move(const Point &at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
    }

    void press(const Point &at, int mods=0)
    {
        holdModifiers(mods, true);
        move(at);
        canvas->inputMouseButton(0, true);
        holdModifiers(mods, false);
        frame();
    }

    void release()
    {
        canvas->inputMouseButton(0, false);
        frame();
    }

    void click(const Point &at, int mods=0)
    {
        press(at, mods);
        release();
    }

    void drag(const Point &from, const Point &to)
    {
        press(from);
        move(to);
        frame();
        release();
    }

    Position focus() const
    {
        REQUIRE(log->hasSelection());
        return log->getSelection().back().focus;
    }
};

}//namespace

TEST_CASE("LogView characters: the text of a range in a row and across rows")
{
    Scene scene(0);
    const RowId a=scene.log->append(controls::LogRow("alpha beta"));
    const RowId b=scene.log->append(controls::LogRow("gamma delta"));
    const RowId c=scene.log->append(controls::LogRow("epsilon"));

    scene.frame();

    scene.log->selectRange(Position{a, 2}, Position{a, 7});
    CHECK(scene.log->getSelectedText() == "pha b");
    // anchor after the focus: the same text
    scene.log->selectRange(Position{a, 7}, Position{a, 2});
    CHECK(scene.log->getSelectedText() == "pha b");

    scene.log->selectRange(Position{a, 6}, Position{b, 5});
    CHECK(scene.log->getSelectedText() == "beta\ngamma");
    scene.log->selectRange(Position{c, 3}, Position{a, 6});
    CHECK(scene.log->getSelectedText() == "beta\ngamma delta\neps");
    CHECK(scene.log->isRowSelected(b));

    // a low end at its row's end copies the '\n' after it alone
    scene.log->selectRange(Position{a, 10}, Position{b, 1});
    CHECK(scene.log->getSelectedText() == "\ng");

    // characters past the row's text clamp to its end
    scene.log->selectRange(Position{b, 6}, Position{b, 99});
    CHECK(scene.log->getSelectedText() == "delta");
    CHECK(scene.log->getSelection()[0].focus.character == 11);

    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == "delta");
}

TEST_CASE("LogView characters: a range inside a collapsed block and over all of it")
{
    Scene scene(0);
    const RowId before=scene.log->append(controls::LogRow("before"));
    const RowId block=scene.log->append(controls::LogRow::block("Bash ls", blockLines(10)));
    const RowId after=scene.log->append(controls::LogRow("after"));

    scene.frame();

    const controls::LogRow *row=scene.log->getRow(block);

    REQUIRE(row != nullptr);
    REQUIRE(row->collapsed);

    const String shown=controls::LogView::blockText(*row, scene.log->getBlockPreviewLines());
    const String full=scene.log->rowText(block);

    REQUIRE(shown == "Bash ls (4 of 10 lines)\nline 0\nline 1\nline 2\nline 3");

    // the title is itself, the count its end, the lines after it their own
    CHECK(scene.log->shownToRowCharacter(block, 4) == 4);
    CHECK(scene.log->shownToRowCharacter(block, 7) == 7);
    CHECK(scene.log->shownToRowCharacter(block, 12) == 7);
    CHECK(scene.log->shownToRowCharacter(block, shown.find("\nline 0")) == full.find("\nline 0"));
    CHECK(scene.log->shownToRowCharacter(block, shown.find("line 2")) == full.find("line 2"));
    CHECK(scene.log->shownToRowCharacter(block, shown.size()) == full.find("\nline 4"));
    CHECK(scene.log->shownToRowCharacter(before, 3) == 3);

    // inside it: the characters it covers
    const size_t line1=scene.log->shownToRowCharacter(block, shown.find("line 1"));
    const size_t line2End=scene.log->shownToRowCharacter(block, shown.find("\nline 3"));

    scene.log->selectRange(Position{block, line1}, Position{block, line2End});
    CHECK(scene.log->getSelectedText() == "line 1\nline 2");

    // from inside it to the next row: the rest of the block, its hidden
    // lines too, since the range runs past its end
    scene.log->selectRange(Position{block, full.find("line 8")}, Position{after, 3});
    CHECK(scene.log->getSelectedText() == "line 8\nline 9\naft");

    // from the row before into its title
    scene.log->selectRange(Position{before, 3}, Position{block, 4});
    CHECK(scene.log->getSelectedText() == "ore\nBash");

    // over all of it: the block's whole text, every line
    scene.log->selectRange(Position{before, 6}, Position{after, 0});
    CHECK(scene.log->getSelectedText() == "\n"+full+"\n");
    CHECK(scene.log->getRow(block)->collapsed);

    // and by the mouse: a press on a shown line, a drag onto the next row
    scene.drag(scene.charPoint(block, shown.find("ine 2")), scene.charPoint(after, 2));
    CHECK(scene.log->getSelectedText() == "ine 2\nline 3\nline 4\nline 5\nline 6\nline 7\nline 8\nline 9\naf");
    // and a drag ending on the last shown line copies up to there
    scene.drag(scene.charPoint(before, 4), scene.charPoint(block, shown.size()-2));
    CHECK(scene.log->getSelectedText() == "re\nBash ls\nline 0\nline 1\nline 2\nline");
}

TEST_CASE("LogView characters: a replace clamps the characters and the cap and a prepend keep them")
{
    Scene scene(0);
    const RowId a=scene.log->append(controls::LogRow("row four is long"));
    const RowId b=scene.log->append(controls::LogRow("second row"));

    scene.frame();

    scene.log->selectRange(Position{a, 2}, Position{a, 9});
    CHECK(scene.log->getSelectedText() == "w four ");
    CHECK(scene.log->replace(a, controls::LogRow("row4")));
    CHECK(scene.log->getSelectedText() == "w4");
    CHECK(scene.log->getSelection()[0].anchor.character == 2);
    CHECK(scene.log->getSelection()[0].focus.character == 4);

    // across rows: each end clamped on its own row
    scene.log->selectRange(Position{a, 3}, Position{b, 6});
    CHECK(scene.log->getSelectedText() == "4\nsecond");
    CHECK(scene.log->replace(b, controls::LogRow("2nd")));
    CHECK(scene.log->getSelectedText() == "4\n2nd");
    CHECK(scene.log->replace(a, controls::LogRow("r")));
    CHECK(scene.log->getSelectedText() == "\n2nd");
    scene.frame();

    // a Shift+click after a replace extends from the clamped anchor
    scene.log->selectRange(Position{b, 1}, Position{b, 2});
    CHECK(scene.log->replace(b, controls::LogRow("2")));
    scene.frame();
    scene.click(scene.charPoint(a, 0), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == "r\n2");

    // a clicked row made longer: a Shift+click above it still takes it whole
    scene.click(scene.charPoint(b, 0));
    CHECK(scene.log->getSelectedText() == "2");
    CHECK(scene.log->replace(b, controls::LogRow("2 longer")));
    scene.frame();
    scene.click(scene.charPoint(a, 0), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == "r\n2 longer");

    // a prepend leaves the range's text as it is
    scene.log->selectRange(Position{a, 0}, Position{b, 1});
    scene.log->prepend({controls::LogRow("old 0"), controls::LogRow("old 1")});
    scene.frame();
    CHECK(scene.log->getSelectedText() == "r\n2");

    // the cap drops the anchor's row: the range from the first row held
    for(int i=0; i < 3; ++i)
        scene.log->append(controls::LogRow("new "+std::to_string(i)));
    scene.log->selectRange(Position{a, 1}, Position{scene.log->lastRowId(), 3});
    scene.log->setMaxRows(3);
    scene.log->append(controls::LogRow("last"));
    CHECK(scene.log->firstRowId() > b);
    CHECK(scene.log->getSelection()[0].anchor == (Position{scene.log->firstRowId(), 0}));
    CHECK(scene.log->getSelectedText() == "new 1\nnew");
}

TEST_CASE("LogView characters: a drag selects from the pressed character to the one under the mouse")
{
    Scene scene;

    CHECK(scene.log->scrollToRow(20));
    scene.frame();
    scene.frame();

    // a plain click: the row whole
    scene.click(scene.charPoint(23, 2));
    CHECK(scene.log->getSelectedText() == "row 23");
    scene.forgetClicks();

    // a press, then a drag two rows down
    scene.press(scene.charPoint(23, 2));
    CHECK(scene.log->getSelectedText() == "row 23");
    scene.move(scene.charPoint(23, 4));
    scene.frame();
    CHECK(scene.log->getSelectedText() == "w ");
    scene.move(scene.charPoint(25, 5));
    scene.frame();
    CHECK(scene.log->getSelectedText() == "w 23\nrow 24\nrow 2");
    CHECK(scene.focus() == (Position{25, 5}));
    // above the press: from it back up
    scene.move(scene.charPoint(22, 1));
    scene.frame();
    CHECK(scene.log->getSelectedText() == "ow 22\nro");
    // back on the pressed character: the row whole again
    scene.move(scene.charPoint(23, 2));
    scene.frame();
    CHECK(scene.log->getSelectedText() == "row 23");
    scene.move(scene.charPoint(24, 3));
    scene.frame();
    scene.release();
    CHECK(scene.log->getSelectedText() == "w 23\nrow");

    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == "w 23\nrow");

    // the release ended the drag: a move selects nothing more
    scene.move(scene.charPoint(27, 1));
    scene.frame();
    CHECK(scene.log->getSelectedText() == "w 23\nrow");

    // past the viewport's bottom: the view scrolls, the focus the end of the
    // row at the bottom
    const Rect &viewport=scene.log->getViewport();

    scene.press(scene.charPoint(26, 3));
    scene.move(scene.log->localPosToCanvas(Point(viewport.x+40, viewport.y+viewport.h+20)));
    for(int i=0; i < 10; ++i)
        scene.tick();
    CHECK(scene.focus().row > 30);
    CHECK(scene.focus().character == scene.log->rowText(scene.focus().row).size());
    CHECK(scene.log->getSelection().back().anchor == (Position{26, 3}));
    scene.release();
}

TEST_CASE("LogView characters: a drag across the wrapped lines of a row")
{
    Scene scene(0);
    String text;

    for(int i=0; i < 40; ++i)
        text+=(i?" word":"word")+std::to_string(i);

    const RowId first=scene.log->append(controls::LogRow("first"));
    const RowId wrapped=scene.log->append(controls::LogRow(text));
    const RowId last=scene.log->append(controls::LogRow("last"));

    scene.frame();
    scene.frame();

    controls::internal::LogViewRow *row=scene.log->rowControl(wrapped);

    REQUIRE(row != nullptr);

    const size_t from=text.find("word2 ")+2;
    const size_t to=text.find("word33")+3;

    // the two characters on different wrapped lines, a line apart at least
    REQUIRE(row->getCharacterPosition(static_cast<int>(to)).y
        > row->getCharacterPosition(static_cast<int>(from)).y+row->getCharacterPosition(0).h);

    scene.drag(scene.charPoint(wrapped, from), scene.charPoint(wrapped, to));
    CHECK(scene.log->getSelectedText() == text.substr(from, to-from));

    // and from a row above it into one of its lines, and on to the next row
    scene.drag(scene.charPoint(first, 2), scene.charPoint(wrapped, to));
    CHECK(scene.log->getSelectedText() == "rst\n"+text.substr(0, to));
    scene.drag(scene.charPoint(wrapped, to), scene.charPoint(last, 2));
    CHECK(scene.log->getSelectedText() == text.substr(to)+"\nla");
}

TEST_CASE("LogView characters: Shift+click extends the focus to the character clicked")
{
    Scene scene;

    CHECK(scene.log->scrollToRow(20));
    scene.frame();
    scene.frame();

    // from a clicked row: kept whole either way
    scene.click(scene.charPoint(23, 3));
    scene.click(scene.charPoint(25, 3), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == "row 23\nrow 24\nrow");
    scene.click(scene.charPoint(21, 2), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == "w 21\nrow 22\nrow 23");

    // from a pressed character
    scene.drag(scene.charPoint(30, 2), scene.charPoint(30, 4));
    CHECK(scene.log->getSelectedText() == "w ");
    scene.click(scene.charPoint(31, 1), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == "w 30\nr");
    scene.click(scene.charPoint(29, 3), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == " 29\nro");
    CHECK(scene.log->getSelection().back().anchor == (Position{30, 2}));

    // a Shift+press drags on from the same anchor
    scene.press(scene.charPoint(32, 1), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == "w 30\nrow 31\nr");
    scene.move(scene.charPoint(31, 5));
    scene.frame();
    CHECK(scene.log->getSelectedText() == "w 30\nrow 3");
    scene.release();

    // with nothing selected, Shift+click selects the row
    scene.key(GLFW_KEY_ESCAPE);
    CHECK_FALSE(scene.log->hasSelection());
    scene.click(scene.charPoint(25, 2), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == "row 25");
}

TEST_CASE("LogView characters: a double-click selects a word and a drag extends it by characters")
{
    Scene scene(0);
    const RowId a=scene.log->append(controls::LogRow("error: open /usr/lib/libfoo.so failed"));
    const RowId b=scene.log->append(controls::LogRow("next  line"));

    scene.frame();

    const String text=scene.log->rowText(a);
    const size_t libfoo=text.find("libfoo");
    const Point inWord=scene.charPoint(a, libfoo+2);

    // the first click selects the row, the second press the word
    scene.click(inWord);
    CHECK(scene.log->getSelectedText() == text);
    scene.press(inWord);
    CHECK(scene.log->getSelectedText() == "libfoo");
    // a drag from it keeps it whole: to the right from its start
    scene.move(scene.charPoint(a, text.find("failed")+3));
    scene.frame();
    CHECK(scene.log->getSelectedText() == "libfoo.so fai");
    // to the left from its end
    scene.move(scene.charPoint(a, text.find("usr")+1));
    scene.frame();
    CHECK(scene.log->getSelectedText() == "sr/lib/libfoo");
    // inside it, the word
    scene.move(scene.charPoint(a, libfoo+4));
    scene.frame();
    CHECK(scene.log->getSelectedText() == "libfoo");
    scene.release();

    scene.key(GLFW_KEY_C, GLFW_MOD_CONTROL);
    CHECK(platform::getClipboardText() == "libfoo");

    // punctuation and blanks make runs of their own; at a row's end the run
    // before it
    scene.log->selectWordAt(Position{a, text.find("/usr")});
    CHECK(scene.log->getSelectedText() == "/");
    scene.log->selectWordAt(Position{b, 5});
    CHECK(scene.log->getSelectedText() == "  ");
    scene.log->selectWordAt(Position{b, 10});
    CHECK(scene.log->getSelectedText() == "line");

    // Shift+click after a word extends from it, the word whole
    scene.tick();
    scene.forgetClicks();
    scene.click(scene.charPoint(a, 2));
    scene.press(scene.charPoint(a, 2));
    scene.release();
    CHECK(scene.log->getSelectedText() == "error");
    scene.click(scene.charPoint(b, 2), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == text+"\nne");
    scene.click(scene.charPoint(a, 3), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == "error");
}

//! A rect as text, so a failed check shows it.
std::string str(const Rect &rect)
{
    return std::to_string(rect.x)+","+std::to_string(rect.y)+" "+std::to_string(rect.w)+"x"+std::to_string(rect.h);
}

TEST_CASE("LogView characters: the highlight is the row whole or a rect per wrapped line it covers")
{
    Scene scene(0);
    String text;

    for(int i=0; i < 40; ++i)
        text+=(i?" word":"word")+std::to_string(i);

    const RowId first=scene.log->append(controls::LogRow("first"));
    const RowId wrapped=scene.log->append(controls::LogRow(text));
    const RowId last=scene.log->append(controls::LogRow("last"));

    scene.frame();
    scene.frame();

    controls::internal::LogViewRow *row=scene.log->rowControl(wrapped);

    REQUIRE(row != nullptr);

    const int lineHeight=scene.log->getLineHeight();
    const auto caret=[&](size_t character) { return row->getCharacterPosition(static_cast<int>(character)); };
    const auto rects=[&](controls::internal::LogViewRow *of)
    {
        std::vector<Rect> found;

        of->getSelectionRects(found);
        return found;
    };
    const size_t from=text.find("word2 ")+2;
    const size_t to=text.find("word33")+3;
    const int lines=(caret(to).y-caret(from).y)/lineHeight+1;

    REQUIRE(lines >= 3);

    // a clicked row: its bounds
    scene.log->selectRow(wrapped);
    scene.frame();
    CHECK(row->getSelection().whole);
    REQUIRE(rects(row).size() == 1);
    CHECK(str(rects(row)[0]) == str(Rect(0, 0, row->getWidth(), row->getHeight())));
    CHECK(rects(scene.log->rowControl(first)).empty());

    // inside it: the first line from the caret to the right edge, the lines
    // between whole, the last to the caret
    scene.log->selectRange(Position{wrapped, from}, Position{wrapped, to});
    scene.frame();
    CHECK_FALSE(row->getSelection().whole);

    std::vector<Rect> found=rects(row);

    REQUIRE(found.size() == static_cast<size_t>(lines));
    CHECK(str(found.front()) == str(Rect(caret(from).x, caret(from).y, row->getWidth()-caret(from).x, lineHeight)));
    for(size_t i=1; i+1 < found.size(); ++i)
        CHECK(str(found[i]) == str(Rect(0, caret(from).y+static_cast<int>(i)*lineHeight, row->getWidth(), lineHeight)));
    CHECK(str(found.back()) == str(Rect(0, caret(to).y, caret(to).x, lineHeight)));

    // inside one line: from caret to caret
    scene.log->selectRange(Position{wrapped, from}, Position{wrapped, from+3});
    scene.frame();
    found=rects(row);
    REQUIRE(found.size() == 1);
    CHECK(str(found[0]) == str(Rect(caret(from).x, caret(from).y, caret(from+3).x-caret(from).x, lineHeight)));

    // from the row above: its first line from the left edge and top; on into
    // the row below: its last line to the right edge and bottom
    scene.log->selectRange(Position{first, 2}, Position{wrapped, to});
    scene.frame();
    found=rects(row);
    REQUIRE(found.size() == static_cast<size_t>(caret(to).y/lineHeight+1));
    CHECK(found.front().x == 0);
    CHECK(found.front().y == 0);
    CHECK(str(found.back()) == str(Rect(0, caret(to).y, caret(to).x, lineHeight)));

    scene.log->selectRange(Position{wrapped, to}, Position{last, 2});
    scene.frame();
    found=rects(row);
    REQUIRE(!found.empty());
    CHECK(found.front().x == caret(to).x);
    CHECK(found.back().x+found.back().w == row->getWidth());
    CHECK(found.back().y+found.back().h == row->getHeight());

    // from the first byte of a wrapped line: nothing on the line above it
    size_t lineStart=1;

    while(lineStart < text.size() && caret(lineStart).y == caret(0).y)
        ++lineStart;
    REQUIRE(lineStart < from+100);
    scene.log->selectRange(Position{wrapped, lineStart}, Position{wrapped, to});
    scene.frame();
    found=rects(row);
    REQUIRE(!found.empty());
    CHECK(str(found.front()) == str(Rect(caret(lineStart).x, caret(lineStart).y,
        row->getWidth()-caret(lineStart).x, lineHeight)));

    // a range over it whole: its bounds again; none: nothing
    scene.log->selectRange(Position{first, 2}, Position{last, 2});
    scene.frame();
    CHECK(rects(row).size() == 1);
    CHECK(str(rects(row)[0]) == str(Rect(0, 0, row->getWidth(), row->getHeight())));
    scene.log->clearSelection();
    scene.frame();
    CHECK(rects(row).empty());
    CHECK_FALSE(row->isSelected());
}

TEST_CASE("LogView characters: a row covered from its start on into the next is the band")
{
    Scene scene(0);
    String text;

    for(int i=0; i < 40; ++i)
        text+=(i?" word":"word")+std::to_string(i);

    const RowId first=scene.log->append(controls::LogRow("first"));
    const RowId wrapped=scene.log->append(controls::LogRow(text));
    const RowId last=scene.log->append(controls::LogRow("last"));

    scene.frame();
    scene.frame();

    controls::internal::LogViewRow *row=scene.log->rowControl(wrapped);

    REQUIRE(row != nullptr);

    const auto rects=[&](controls::internal::LogViewRow *of)
    {
        std::vector<Rect> found;

        of->getSelectionRects(found);
        return found;
    };
    const String band=str(Rect(0, 0, row->getWidth(), row->getHeight()));

    // from its first character on into the row below: its bounds, the left
    // edge the rows below it start at
    scene.log->selectRange(Position{wrapped, 0}, Position{last, 2});
    scene.frame();
    CHECK(row->getSelection().whole);
    REQUIRE(rects(row).size() == 1);
    CHECK(str(rects(row)[0]) == band);

    // a click on it and a Shift+click below: the same
    scene.click(scene.charPoint(wrapped, 3));
    scene.click(scene.charPoint(last, 2), GLFW_MOD_SHIFT);
    CHECK(scene.log->getSelectedText() == text+"\nla");
    CHECK(row->getSelection().whole);
    REQUIRE(rects(row).size() == 1);
    CHECK(str(rects(row)[0]) == band);

    // from the row above to its end, no further: its last line stops at the
    // text's end
    scene.log->selectRange(Position{first, 2}, Position{wrapped, text.size()});
    scene.frame();
    CHECK_FALSE(row->getSelection().whole);

    const std::vector<Rect> found=rects(row);
    const Rect end=row->getCharacterPosition(static_cast<int>(text.size()));

    REQUIRE(found.size() > 1);
    CHECK(found.back().x+found.back().w == end.x);
}

TEST_CASE("LogView characters: a row of an app kind is selected whole or not at all")
{
    Scene scene(0);
    constexpr controls::LogRow::Kind Card=controls::LogRow::FirstAppKind;
    controls::LogView::RowKind card;

    // content that takes no mouse, so the clicks are the view's
    card.factory=[](controls::Base *row) -> controls::Base *
        {
            controls::Base *content=row->newChild<controls::Base>();

            content->setMouseInputEnabled(false);
            return content;
        };
    card.binder=[](controls::Base * /*content*/, const controls::LogRow & /*row*/) {};
    card.measurer=[](const controls::LogRow & /*row*/, int /*width*/) { return 30; };
    scene.log->setRowKind(Card, card);

    controls::LogRow cardRow("card one");

    cardRow.kind=Card;

    const RowId plain=scene.log->append(controls::LogRow("plain row"));
    const RowId cardId=scene.log->append(cardRow);

    scene.frame();
    scene.frame();

    controls::internal::LogViewRow *row=scene.log->rowControl(cardId);

    REQUIRE(row != nullptr);
    REQUIRE(row->getKindContent() != nullptr);

    const auto rects=[&]()
    {
        std::vector<Rect> found;

        row->getSelectionRects(found);
        return found;
    };

    // a double-click selects no word of its hidden text: the row, as drawn
    const Point at=row->localPosToCanvas(Point(row->getWidth()/4, row->getHeight()/2));

    scene.click(at);
    scene.press(at);
    scene.release();
    CHECK(scene.log->getSelectedText() == scene.log->rowText(cardId));
    REQUIRE(rects().size() == 1);
    CHECK(str(rects()[0]) == str(Rect(0, 0, row->getWidth(), row->getHeight())));

    // a range ending at its start covers none of it: nothing drawn
    scene.log->selectRange(Position{plain, 2}, Position{cardId, 0});
    scene.frame();
    CHECK(scene.log->getSelectedText() == "ain row\n");
    CHECK(rects().empty());
}
