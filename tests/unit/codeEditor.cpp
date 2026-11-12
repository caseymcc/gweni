/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeEditor on a real Canvas with the StubSkin: only the lines on screen
 *  are controls. The pool is the viewport's lines plus one, whatever the line
 *  count, so a document of 1 000 and one of 100 000 lines take the same
 *  controls and ids; every shown row is bound to the line at its place, with
 *  that line's text. The cell width is the font's '0' advance and the line
 *  height its line box, read once; a column's x is TextInset plus the column
 *  times the cell width, so a line with wide glyphs (2 cells each) or
 *  combining marks (0) keeps the lines below it on the same columns. The
 *  content is as wide as the longest line in cells (kept as a count of lines
 *  per width, through edits that lengthen, shorten, add and remove lines).
 *  An edit inside one line binds only that line's row; a line break mid-
 *  screen moves the rows below it to their new lines without binding them
 *  again and binds the row the new line came into view on; a join does the
 *  reverse. Every frame lays out in one pass with no request, and deleting the
 *  editor gives back every id.
 *
 *  Also text::stringCells and text::utf8CodepointAt, which the columns are
 *  counted with.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>
#include <gweni/text/cells.h>

#include <doctest/doctest.h>

#include <map>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! A YAML-ish document of \p lines lines, line i's text depending on i alone.
String document(size_t lines)
{
    String text;

    for(size_t i=0; i < lines; ++i)
    {
        if(i > 0)
            text+='\n';
        if(i%10 == 0)
            text+="section_"+std::to_string(i/10)+":";
        else
            text+="  key_"+std::to_string(i)+": value "+std::to_string(i*7%100);
    }
    return text;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CodeEditor *editor=nullptr;

    explicit Scene(size_t lines, int height=200)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 600);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setPos(0, 0);
        editor->setSize(300, height);
        editor->setText(document(lines));
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
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

    //! The line each shown row shows.
    std::map<controls::CodeLine *, size_t> shownLines() const
    {
        std::map<controls::CodeLine *, size_t> shown;

        for(controls::Base *row : editor->getRows())
        {
            if(!row->hidden())
                shown[gweni_cast<controls::CodeLine>(row)]=editor->itemForRow(row);
        }
        return shown;
    }
};

//! Every line from the one at the top of the viewport to the last one in it
//! is shown by exactly one row, with its text, at its place.
void checkView(Scene &s)
{
    controls::CodeEditor *editor=s.editor;
    const int lineHeight=editor->getLineHeight();
    const int offset=editor->getVerticalScrollOffset();
    const int viewport=editor->getViewportHeight();
    const size_t first=static_cast<size_t>(offset/lineHeight);
    const size_t end=std::min(editor->getLineCount(),
        static_cast<size_t>((offset+viewport+lineHeight-1)/lineHeight));
    size_t shown=0;

    for(controls::Base *row : editor->getRows())
    {
        if(row->hidden())
            continue;

        controls::CodeLine *line=gweni_cast<controls::CodeLine>(row);

        REQUIRE(line);
        ++shown;
        INFO("line ", line->getLine());
        CHECK(line->getLine() >= first);
        CHECK(line->getLine() < end);
        CHECK(line->getLine() == editor->itemForRow(row));
        CHECK(line->getText() == editor->getDocument().line(line->getLine()));
        CHECK(row->getY() == static_cast<int>(line->getLine())*lineHeight-offset);
        CHECK(row->getHeight() == lineHeight);
    }
    CHECK(shown == end-first);
}

}//namespace

TEST_CASE("Text cells: stringCells counts wide glyphs as 2 and combining marks as 0")
{
    CHECK(text::stringCells("") == 0);
    CHECK(text::stringCells("abc") == 3);
    // 日本 are wide, é is one cell either way, a combining acute takes none
    CHECK(text::stringCells("\xe6\x97\xa5\xe6\x9c\xac") == 4);
    CHECK(text::stringCells("\xc3\xa9") == 1);
    CHECK(text::stringCells("e\xcc\x81") == 1);
    CHECK(text::stringCells("a\xe6\x97\xa5" "b") == 4);
    // a range of the bytes
    CHECK(text::stringCells("a\xe6\x97\xa5" "b", 1, 4) == 2);
    CHECK(text::stringCells("a\xe6\x97\xa5" "b", 4) == 1);

    size_t next=0;

    CHECK(text::utf8CodepointAt("\xe6\x97\xa5", 0, &next) == 0x65e5);
    CHECK(next == 3);
    // a cut sequence is U+FFFD, one byte long
    CHECK(text::utf8CodepointAt("\xe6\x97", 0, &next) == 0xfffd);
    CHECK(next == 1);
    CHECK(text::utf8CodepointAt("ab", 2, &next) == 0);
    CHECK(next == 2);
}

TEST_CASE("CodeEditor: the pool is the viewport's lines plus one")
{
    for(int height : {100, 200, 333})
    {
        Scene s(1000, height);
        const int lineHeight=s.editor->getLineHeight();

        INFO("height ", height);
        REQUIRE(lineHeight > 0);
        const int viewport=s.editor->getViewportHeight();

        CHECK(s.editor->getRows().size() == static_cast<size_t>((viewport+lineHeight-1)/lineHeight+1));
        checkView(s);
        s.checkOnePass();
    }
}

TEST_CASE("CodeEditor: 1 000 and 100 000 lines take the same controls and ids")
{
    struct Taken
    {
        size_t controls;
        size_t primitives;
        size_t texts;
        size_t rows;
    };

    auto take=[](size_t lines)
    {
        const test::IdBankCounts before=test::IdBankCounts::now();
        Taken taken{};

        {
            Scene s(lines);
            const test::IdBankCounts after=test::IdBankCounts::now();

            checkView(s);
            s.checkOnePass();
            CHECK(s.editor->getLineCount() == lines);
            CHECK(s.editor->getVirtualContentHeight() == static_cast<int>(lines)*s.editor->getLineHeight());

            // scrolled to the middle and to the end, the same again
            s.editor->setVerticalScrollOffset(s.editor->getMaxVerticalScrollOffset()/2);
            s.frame();
            checkView(s);
            s.editor->setVerticalScrollOffset(s.editor->getMaxVerticalScrollOffset());
            s.frame();
            checkView(s);
            s.checkOnePass();

            const test::IdBankCounts scrolled=test::IdBankCounts::now();

            CHECK(scrolled.livePrimitives() == after.livePrimitives());
            CHECK(scrolled.liveTexts() == after.liveTexts());

            taken=Taken{s.skin.liveControls, after.livePrimitives()-before.livePrimitives(),
                after.liveTexts()-before.liveTexts(), s.editor->getRows().size()};
        }

        // the editor and its canvas give every id back
        const test::IdBankCounts gone=test::IdBankCounts::now();

        CHECK(gone.livePrimitives() == before.livePrimitives());
        CHECK(gone.liveTexts() == before.liveTexts());
        return taken;
    };

    const Taken small=take(1000);
    const Taken large=take(100000);

    CHECK(small.controls == large.controls);
    CHECK(small.primitives == large.primitives);
    CHECK(small.texts == large.texts);
    CHECK(small.rows == large.rows);
}

TEST_CASE("CodeEditor: columns are cells of the font's '0' advance, the same on every line")
{
    Scene s(10);
    controls::CodeEditor *editor=s.editor;
    const float expected=static_cast<float>(s.renderer.measureText(editor->getFont(),
        String(controls::CodeEditor::CellSample, '0')).x)/controls::CodeEditor::CellSample;

    CHECK((editor->getFont() == s.skin.getDefaultMonoFont()));
    CHECK(editor->getCellWidth() == doctest::Approx(expected));
    CHECK(editor->getLineHeight() == s.renderer.lineHeight(editor->getFont()));

    for(size_t column : {0u, 1u, 7u, 40u})
        CHECK(editor->columnX(column) == controls::CodeEditor::TextInset
            +static_cast<int>(std::floor(column*static_cast<double>(expected))));

    // a line of wide glyphs and one with a combining mark: their widths in
    // cells, and the content as wide as the longest
    editor->setText("ab\n\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\ne\xcc\x81" "e\xcc\x81");
    s.frame();
    CHECK(editor->getLineCells(0) == 2);
    CHECK(editor->getLineCells(1) == 6);
    CHECK(editor->getLineCells(2) == 2);
    CHECK(editor->getLongestLineCells() == 6);
    CHECK(editor->getLineCells(3) == 0);
    s.checkOnePass();
}

TEST_CASE("CodeEditor: the longest line is kept through edits")
{
    Scene s(5);
    controls::CodeEditor *editor=s.editor;
    text::Document &document=editor->getDocument();

    editor->setText("aaaa\nbb\naaaa\nc");
    CHECK(editor->getLongestLineCells() == 4);

    // one of the two longest shortened: still 4
    document.erase({{0, 1}, {0, 4}});
    CHECK(editor->getLongestLineCells() == 4);
    // the other: 2
    document.erase({{2, 0}, {2, 3}});
    CHECK(editor->getLongestLineCells() == 2);
    // a long line typed in
    document.insert({3, 1}, "1234567");
    CHECK(editor->getLongestLineCells() == 8);
    // split: the two halves
    document.insert({3, 4}, "\n");
    CHECK(editor->getLineCount() == 5);
    CHECK(editor->getLineCells(3) == 4);
    CHECK(editor->getLineCells(4) == 4);
    CHECK(editor->getLongestLineCells() == 4);
    // joined over three lines
    document.erase({{1, 2}, {4, 0}});
    CHECK(editor->getLineCount() == 2);
    CHECK(editor->getLineCells(1) == 6);
    CHECK(editor->getLongestLineCells() == 6);
    // every line removed
    document.setText("");
    CHECK(editor->getLineCount() == 1);
    CHECK(editor->getLongestLineCells() == 0);

    // the content's width follows it
    document.setText(String(100, 'x'));
    s.frame();

    const int width=2*controls::CodeEditor::TextInset
        +static_cast<int>(std::ceil(101*static_cast<double>(editor->getCellWidth())));

    CHECK(editor->getMaxHorizontalScrollOffset() == width-editor->getViewport().w);
    s.checkOnePass();
}

TEST_CASE("CodeEditor: an edit binds the rows of the lines it changed, and moves the rows below")
{
    Scene s(1000);
    controls::CodeEditor *editor=s.editor;
    text::Document &document=editor->getDocument();

    editor->setVerticalScrollOffset(100*editor->getLineHeight());
    s.frame();
    checkView(s);

    const size_t top=100;
    const size_t middle=top+5;
    controls::CodeLine *middleRow=editor->rowForLine(middle);

    REQUIRE(middleRow);

    std::map<controls::CodeLine *, size_t> before=s.shownLines();
    const size_t measuresBefore=s.renderer.measures;

    // typed into one line: only its row's text changes
    document.insert({middle, 2}, "x");
    s.frame();
    checkView(s);
    CHECK(s.shownLines() == before);
    // one Text measured again: its new string
    CHECK(s.renderer.measures-measuresBefore == 1);

    // a line break mid-screen: the rows below show the line they showed, one
    // further down, and the row that was the spare shows the new line
    document.insert({middle, 3}, "\n");
    s.frame();
    checkView(s);

    const std::map<controls::CodeLine *, size_t> broken=s.shownLines();

    for(const auto &[row, line] : before)
    {
        if(line < middle)
            CHECK(broken.at(row) == line);
        else if(line > middle && editor->rowForLine(line+1))
            CHECK(editor->rowForLine(line+1) == row);
    }
    CHECK(broken.at(middleRow) == middle);
    CHECK(editor->rowForLine(middle+1));
    CHECK(editor->rowForLine(middle+1)->getText() == document.line(middle+1));

    // joined back: the reverse
    document.erase({{middle, document.line(middle).size()}, {middle+1, 0}});
    s.frame();
    checkView(s);
    for(const auto &[row, line] : s.shownLines())
        CHECK(row->getText() == document.line(line));
    s.checkOnePass();
}
