/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeEditor's wrap, from text::wrapCells up. The wrap is counted in cells
 *  alone: a row takes the caret stops that fit (a wide glyph 2 cells, a mark
 *  0 and kept with its base), blanks hang past the edge on the row they end,
 *  a word that does not fit goes to the next row whole and one wider than the
 *  row is cut between stops, a stop wider than the row is a row of its own,
 *  and width 0 does not wrap. wrapRowCount() is the same walk.
 *
 *  setWrap(true) puts the lines on the variable-height list with no
 *  horizontal bar and the rows the viewport's width: every line is an item
 *  its visual rows times the line height high, each shown row a Text per
 *  visual row holding exactly that row's bytes, one line height below the
 *  other; an estimate before a line is measured is its cells over the row's.
 *  An edit measures again only the line it changed: one that keeps its row
 *  count keeps every other height, one that adds a row grows that line's
 *  height alone. A narrower editor wraps the rows on screen again in the same
 *  frame, and holds the controls and texts an editor built at that width
 *  holds. setWrap(false) gives back the fixed rows and the bar. Every frame
 *  lays out in one pass with no request.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>
#include <gweni/text/cellWrap.h>

#include <doctest/doctest.h>

#include <iterator>
#include <map>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

std::vector<size_t> wrap(const String &line, size_t width)
{
    std::vector<size_t> starts;

    text::wrapCells(line, width, starts);
    CHECK(text::wrapRowCount(line, width) == starts.size());
    return starts;
}

//! The rows a wrap gives, as strings.
std::vector<String> rows(const String &line, size_t width)
{
    const std::vector<size_t> starts=wrap(line, width);
    std::vector<String> out;

    for(size_t i=0; i < starts.size(); ++i)
    {
        const size_t end=(i+1 < starts.size())?starts[i+1]:line.size();

        out.push_back(line.substr(starts[i], end-starts[i]));
    }
    return out;
}

using Rows=std::vector<String>;

//! A document of prose-like lines, line i's text depending on i alone:
//! every third line long enough to wrap at the scene's width.
String document(size_t lines)
{
    String text;

    for(size_t i=0; i < lines; ++i)
    {
        if(i > 0)
            text+='\n';
        text+="line "+std::to_string(i)+":";
        if(i%3 == 0)
        {
            for(size_t w=0; w < 6+i%5; ++w)
                text+=" word"+std::to_string(w);
        }
    }
    return text;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CodeEditor *editor=nullptr;

    explicit Scene(size_t lines, int width=200, int height=200)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 600);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setPos(0, 0);
        editor->setSize(width, height);
        editor->setText(document(lines));
        editor->setWrap(true);
        frame();
    }

    //! Frames until the idle re-measure has measured every line.
    void settle()
    {
        for(int i=0; i < 1000 && editor->isRemeasuring(); ++i)
            frame();
        CHECK(!editor->isRemeasuring());
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
};

//! Every shown row shows its line wrapped at the editor's wrap cells: a Text
//! per visual row with that row's bytes, a line height apart, and the row
//! (and its item) as high as its rows.
void checkRows(Scene &s)
{
    controls::CodeEditor *editor=s.editor;
    const int lineHeight=editor->getLineHeight();
    const size_t cells=editor->getWrapCells();
    size_t shown=0;

    REQUIRE(cells > 0);
    for(controls::Base *row : editor->getRows())
    {
        if(row->hidden())
            continue;

        controls::CodeLine *line=gweni_cast<controls::CodeLine>(row);

        REQUIRE(line);
        ++shown;

        const String &text=editor->getDocument().line(line->getLine());
        const Rows expected=rows(text, cells);

        CAPTURE(line->getLine());
        CHECK(line->getText() == text);
        REQUIRE(line->getRowCount() == expected.size());
        CHECK(editor->getLineRows(line->getLine()) == expected.size());
        CHECK(line->getHeight() == lineHeight*static_cast<int>(expected.size()));
        CHECK(line->getPreferredSize().height == line->getHeight());
        CHECK(editor->getHeightIndex().height(line->getLine()) == line->getHeight());
        CHECK(line->getWidth() == editor->getInnerPanel()->getWidth());
        CHECK(editor->wrapCellsFor(line->getWidth()) == cells);

        for(size_t r=0; r < expected.size(); ++r)
        {
            CAPTURE(r);
            CHECK(line->getRowText(r)->getText() == expected[r]);
            CHECK(line->getRowText(r)->getY() == static_cast<int>(r)*lineHeight);
            CHECK(line->getRowText(r)->getHeight() == lineHeight);
            CHECK(line->getRowText(r)->getX() == controls::CodeEditor::TextInset);
            CHECK(line->getRowText(r)->visible());
        }
    }
    CHECK(shown > 0);
}

//! The controls under the editor, by type, and its rows' texts.
std::map<std::string, size_t> census(controls::Base *control)
{
    std::map<std::string, size_t> counts;
    std::vector<controls::Base *> stack{control};

    while(!stack.empty())
    {
        controls::Base *c=stack.back();

        stack.pop_back();
        ++counts[c->getTypeName()];
        for(controls::Base *child : c->getActualChildren())
            stack.push_back(child);
    }
    return counts;
}

}//namespace

TEST_CASE("Text cell wrap: a word that does not fit goes to the next row, a longer one is cut")
{
    CHECK(rows("", 10) == Rows{""});
    CHECK(rows("short", 10) == Rows{"short"});
    // exactly the width fits
    CHECK(rows("abcdefghij", 10) == Rows{"abcdefghij"});
    // the word moves whole, its blank staying on the row it ends
    CHECK(rows("one two three", 9) == Rows{"one two ", "three"});
    CHECK(rows("aaa bbb ccc ddd", 6) == Rows{"aaa ", "bbb ", "ccc ", "ddd"});
    CHECK(rows("aaa bbb ccc ddd", 7) == Rows{"aaa bbb ", "ccc ddd"});
    CHECK(rows("aaa bbb ccc ddd", 8) == Rows{"aaa bbb ", "ccc ddd"});
    // no blank: cut at the cell; a word wider than the row after a blank
    // goes to a row of its own first, then is cut there
    CHECK(rows("abcdefghijklmnopq", 5) == Rows{"abcde", "fghij", "klmno", "pq"});
    CHECK(rows("ab cdefghijkl", 5) == Rows{"ab ", "cdefg", "hijkl"});
    // punctuation is part of a word: only blanks separate
    CHECK(rows("a.b,c;d e", 4) == Rows{"a.b,", "c;d ", "e"});
    // width 0 does not wrap; width 1 is a stop a row
    CHECK(rows("abc def", 0) == Rows{"abc def"});
    CHECK(rows("abc", 1) == Rows{"a", "b", "c"});
    CHECK(wrap("one two three", 9) == std::vector<size_t>{0, 8});
}

TEST_CASE("Text cell wrap: blanks hang past the edge on the row they end")
{
    // trailing blanks never start a row, however many
    CHECK(rows("abcd      ", 4) == Rows{"abcd      "});
    CHECK(rows("abcd      e", 4) == Rows{"abcd      ", "e"});
    CHECK(rows("ab  cd", 3) == Rows{"ab  ", "cd"});
    // a tab is a blank of one cell
    CHECK(rows("ab\tcd", 3) == Rows{"ab\t", "cd"});
    // a line of blanks is one row
    CHECK(rows(String(40, ' '), 8) == Rows{String(40, ' ')});
    // leading blanks (an indent) are a word break too: the word after them
    // goes to the next row when it does not fit beside them
    CHECK(rows("    word", 6) == Rows{"    ", "word"});
    CHECK(rows("  ab cd", 5) == Rows{"  ab ", "cd"});
}

TEST_CASE("Text cell wrap: wide glyphs take 2 cells, marks stay with their base")
{
    const String ri="\xe6\x97\xa5";    // U+65E5, wide
    const String hon="\xe6\x9c\xac";   // U+672C, wide
    const String go="\xe8\xaa\x9e";    // U+8A9E, wide
    const String eAcute="e\xcc\x81";   // e and U+0301

    // 3 wide glyphs in 5 cells: two fit, the third (2 more) does not
    CHECK(rows(ri+hon+go, 5) == Rows{ri+hon, go});
    CHECK(rows(ri+hon+go, 6) == Rows{ri+hon+go});
    // a wide glyph never splits: at 1 cell each is a row of its own
    CHECK(rows(ri+hon, 1) == Rows{ri, hon});
    // after a narrow one, a wide glyph that would take the row past the
    // edge goes to the next row
    CHECK(rows("a"+ri+hon, 4) == Rows{"a"+ri, hon});
    // a mark is 0 cells and goes with its base, at a cut too
    CHECK(rows(eAcute+eAcute+eAcute, 2) == Rows{eAcute+eAcute, eAcute});
    CHECK(rows("ab"+eAcute, 2) == Rows{"ab", eAcute});
    // words of wide glyphs move whole like any other
    CHECK(rows(ri+hon+" "+go+ri, 4) == Rows{ri+hon+" ", go+ri});
}

TEST_CASE("CodeEditor wrap: every line is an item its visual rows high, each row's bytes in a Text of its own")
{
    Scene s(60);
    controls::CodeEditor *editor=s.editor;

    REQUIRE(editor->isWrapped());
    CHECK(editor->isVariableHeight());
    CHECK(!editor->canScrollHorizontal());
    CHECK(editor->getHorizontalScrollOffset() == 0);
    CHECK(editor->getHeightIndex().size() == editor->getLineCount());
    CHECK(editor->getWrapCells() == editor->wrapCellsFor(editor->getInnerPanel()->getWidth()));
    checkRows(s);

    // some shown line wraps: the scene is about wrapping
    size_t wrapped=0;

    for(controls::Base *row : editor->getRows())
    {
        if(!row->hidden() && gweni_cast<controls::CodeLine>(row)->getRowCount() > 1)
            ++wrapped;
    }
    CHECK(wrapped > 0);

    // the rows sit one below the other, each at its line's height
    std::map<size_t, controls::Base *> byLine;

    for(controls::Base *row : editor->getRows())
    {
        if(!row->hidden())
            byLine[editor->itemForRow(row)]=row;
    }
    for(auto it=byLine.begin(); std::next(it) != byLine.end(); ++it)
    {
        if(std::next(it)->first == it->first+1)
            CHECK(std::next(it)->second->getY() == it->second->bottom());
    }

    // a line not measured yet counts at its cells over the row's
    const size_t cells=editor->getWrapCells();
    const size_t line=editor->getLineCount()-3;    // a long line, below the view

    const size_t estimate=std::max<size_t>(1, (editor->getLineCells(line)+cells-1)/cells);

    REQUIRE(editor->getHeightIndex().generation(line) != editor->getMeasureGeneration());
    CHECK(estimate > 1);
    CHECK(editor->getHeightIndex().height(line) == static_cast<int>(estimate)*editor->getLineHeight());
    s.checkOnePass();
}

TEST_CASE("CodeEditor wrap: an edit measures its line alone, the rows it takes")
{
    Scene s(30, 200, 400);
    controls::CodeEditor *editor=s.editor;
    const int lineHeight=editor->getLineHeight();
    const size_t cells=editor->getWrapCells();
    std::vector<int> heights;

    s.settle();

    for(size_t i=0; i < editor->getLineCount(); ++i)
        heights.push_back(editor->getHeightIndex().height(i));

    // a short line, typed into without wrapping it
    REQUIRE(editor->getLineRows(1) == 1);
    editor->setCaret(text::Position{1, 0});
    editor->insertText("x");
    s.frame();
    CHECK(editor->getLineRows(1) == 1);
    for(size_t i=0; i < editor->getLineCount(); ++i)
        CHECK(editor->getHeightIndex().height(i) == heights[i]);

    // filled past the row: that line alone grows, by the rows it takes now
    const String fill(cells+2, 'y');

    editor->insertText(" "+fill);
    s.frame();
    CHECK(editor->getLineRows(1) == 3);
    CHECK(editor->getHeightIndex().height(1) == 3*lineHeight);
    for(size_t i=0; i < editor->getLineCount(); ++i)
    {
        if(i != 1)
            CHECK(editor->getHeightIndex().height(i) == heights[i]);
    }
    checkRows(s);
    s.checkOnePass();
}

TEST_CASE("CodeEditor wrap: a narrower editor wraps again in the frame, as one built at that width")
{
    Scene s(40, 260, 300);
    controls::CodeEditor *editor=s.editor;
    const size_t wide=editor->getWrapCells();

    editor->setSize(160, 300);
    s.frame();
    CHECK(editor->getWrapCells() < wide);
    checkRows(s);

    // the same controls an editor built at that width holds
    Scene fresh(40, 160, 300);

    CHECK(fresh.editor->getWrapCells() == editor->getWrapCells());
    CHECK(census(editor) == census(fresh.editor));

    // and back
    editor->setSize(260, 300);
    s.frame();
    CHECK(editor->getWrapCells() == wide);
    checkRows(s);
    s.checkOnePass();

    // once every line is measured, a frame lays nothing out
    s.settle();
    s.frame();
    CHECK(s.canvas->getLayoutStats().passes == 0);
    checkRows(s);
}

TEST_CASE("CodeEditor wrap: turning it off gives back the fixed rows and the horizontal bar")
{
    Scene s(40);
    controls::CodeEditor *editor=s.editor;

    editor->setWrap(false);
    s.frame();
    CHECK(!editor->isWrapped());
    CHECK(!editor->isVariableHeight());
    CHECK(editor->canScrollHorizontal());
    CHECK(editor->getWrapCells() == 0);

    for(controls::Base *row : editor->getRows())
    {
        if(row->hidden())
            continue;

        controls::CodeLine *line=gweni_cast<controls::CodeLine>(row);

        CHECK(line->getRowCount() == 1);
        CHECK(line->getHeight() == editor->getLineHeight());
        CHECK(line->getTextControl()->getText() == editor->getDocument().line(line->getLine()));
    }
    // the content is as wide as the longest line again
    CHECK(editor->getInnerPanel()->getWidth() > editor->getViewport().w);

    // and on
    editor->setWrap(true);
    s.frame();
    checkRows(s);
    s.checkOnePass();
}
