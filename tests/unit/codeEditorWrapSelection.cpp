/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A wrapped CodeEditor's selection: it stays in document positions, and a
 *  row draws one rect per visual row of its line the selection covers, a
 *  line high at that row's place, between the selection's cells on the row
 *  counted from the row's start. A row the selection goes on past is covered
 *  to its last cell (at most the wrap cells, so blanks hanging past the edge
 *  are cut there), the line's last row one cell past its last glyph when the
 *  line break is selected; a selection ending at a wrap point covers nothing
 *  of the row below, one starting there nothing of the row above; wide glyphs
 *  count 2 cells. The rects are in units, the same at scale 1 and 2, and
 *  getSelectedText() is the document's text of the range. A narrower editor
 *  wraps again and the rects follow the new rows, the selection kept.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

const String wideJa="\xe6\x97\xa5";     // 日, 2 cells, 3 bytes

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CodeEditor *editor=nullptr;
    size_t C=0;

    explicit Scene(float scale, int width=200)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setScale(scale);
        canvas->setSize(600, 600);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setPos(0, 0);
        editor->setSize(width, 300);
        editor->setWrap(true);
        frame();
        C=editor->getWrapCells();
        REQUIRE(C > 6);
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

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void select(const text::Position &from, const text::Position &to)
    {
        editor->setCaret(from);
        editor->setCaret(to, true);
        frame();
    }

    std::vector<Rect> rectsOf(size_t line) const
    {
        std::vector<Rect> rects;
        const controls::CodeLine *row=editor->rowForLine(line);

        REQUIRE(row);
        row->getSelectionRects(rects);
        return rects;
    }

    //! The rect of cells [begin, end) on visual row \p row.
    Rect cells(size_t row, size_t begin, size_t end) const
    {
        const int x0=editor->columnX(begin);
        const int x1=editor->columnX(end);
        const int lineHeight=editor->getLineHeight();

        return Rect(x0, static_cast<int>(row)*lineHeight, x1-x0, lineHeight);
    }
};

text::Position at(size_t line, size_t byte) { return text::Position{line, byte}; }

String show(const std::vector<Rect> &rects)
{
    String out;

    for(const Rect &r : rects)
        out+="("+std::to_string(r.x)+","+std::to_string(r.y)+" "+std::to_string(r.w)+"x"+std::to_string(r.h)+")";
    return out;
}

void checkRects(const std::vector<Rect> &actual, const std::vector<Rect> &expected)
{
    CAPTURE(show(actual));
    CAPTURE(show(expected));
    REQUIRE(actual.size() == expected.size());
    for(size_t i=0; i < actual.size(); ++i)
    {
        CHECK(actual[i].x == expected[i].x);
        CHECK(actual[i].y == expected[i].y);
        CHECK(actual[i].w == expected[i].w);
        CHECK(actual[i].h == expected[i].h);
    }
}

}//namespace

TEST_CASE("CodeEditor wrapped selection: one rect per visual row it covers, at scale 1 and 2")
{
    for(float scale : {1.0f, 2.0f})
    {
        INFO("scale ", scale);
        Scene s(scale);
        const size_t C=s.C;
        controls::CodeEditor *editor=s.editor;

        // line 0: rows of x [0, C) [C, 2C) [2C, 2C+3); line 1: "ab " and a
        // C-letter word; line 2 unselected
        editor->setText(String(2*C+3, 'x')+"\nab "+String(C, 'c')+"\nend");
        s.frame();
        REQUIRE(editor->getLineRows(0) == 3);
        REQUIRE(editor->getLineRows(1) == 2);

        // from inside line 0's first row to inside line 1's second
        s.select(at(0, 2), at(1, 5));
        CHECK(editor->getSelectedText() == String(2*C+1, 'x')+"\nab cc");
        CHECK(editor->getAnchor() == at(0, 2));
        checkRects(s.rectsOf(0), {s.cells(0, 2, C), s.cells(1, 0, C), s.cells(2, 0, 3+1)});
        checkRects(s.rectsOf(1), {s.cells(0, 0, 3), s.cells(1, 0, 2)});
        checkRects(s.rectsOf(2), {});
        // getSelectionRect() is the first of them
        CHECK(editor->rowForLine(0)->getSelectionRect().x == s.cells(0, 2, C).x);

        // ending at a wrap point: nothing of the row below
        s.select(at(0, 1), at(0, C));
        CHECK(editor->getSelectedText() == String(C-1, 'x'));
        checkRects(s.rectsOf(0), {s.cells(0, 1, C)});

        // starting at one: nothing of the row above
        s.select(at(0, C), at(0, C+2));
        checkRects(s.rectsOf(0), {s.cells(1, 0, 2)});

        // made backwards, the same rects
        s.select(at(1, 5), at(0, 2));
        checkRects(s.rectsOf(0), {s.cells(0, 2, C), s.cells(1, 0, C), s.cells(2, 0, 3+1)});
        checkRects(s.rectsOf(1), {s.cells(0, 0, 3), s.cells(1, 0, 2)});

        // collapsed: none
        s.select(at(1, 1), at(1, 1));
        for(size_t line=0; line < 3; ++line)
            checkRects(s.rectsOf(line), {});
    }
}

TEST_CASE("CodeEditor wrapped selection: blanks hanging past the edge are cut at it, wide glyphs count 2 cells")
{
    Scene s(1.0f);
    const size_t C=s.C;
    controls::CodeEditor *editor=s.editor;

    // line 0: C-1 letters and 4 blanks hanging to C+3 cells, then "e"
    // line 1: wide glyphs, C/2 a row
    String wide;

    for(size_t i=0; i < C; ++i)
        wide+=wideJa;
    editor->setText(String(C-1, 'x')+"    e\n"+wide+"\nz");
    s.frame();
    REQUIRE(editor->getLineRows(0) == 2);

    // the whole of line 0 and its break: the first row cut at the edge
    s.select(at(0, 0), at(1, 0));
    CHECK(editor->getSelectedText() == String(C-1, 'x')+"    e\n");
    checkRects(s.rectsOf(0), {s.cells(0, 0, C), s.cells(1, 0, 1+1)});
    checkRects(s.rectsOf(1), {});

    // glyphs 1 to 3 of the wide line (bytes 3 to 9): cells 2 to 6
    s.select(at(1, 3), at(1, 9));
    CHECK(editor->getSelectedText() == wideJa+wideJa);
    checkRects(s.rectsOf(1), {s.cells(0, 2, 6)});

    // from the wide line's second row on: that row's cells from its start
    const size_t perRow=C/2;

    s.select(at(1, 3*perRow+3), at(2, 1));
    REQUIRE(editor->getLineRows(1) >= 2);
    CHECK(editor->rowForLine(1)->getRowStart(1) == 3*perRow);
    std::vector<Rect> rects=s.rectsOf(1);

    REQUIRE(rects.size() == editor->getLineRows(1)-1);
    CHECK(rects[0].x == s.cells(1, 2, 2*perRow).x);
    CHECK(rects[0].y == editor->getLineHeight());
    checkRects(s.rectsOf(2), {s.cells(0, 0, 1)});
}

TEST_CASE("CodeEditor wrapped selection: a narrower editor wraps again, the rects follow, the selection kept")
{
    Scene s(1.0f, 300);
    controls::CodeEditor *editor=s.editor;
    const size_t wide=s.C;

    editor->setText(String(2*wide, 'x')+"\nnext");
    s.frame();
    REQUIRE(editor->getLineRows(0) == 2);

    s.select(at(0, 1), at(1, 2));
    checkRects(s.rectsOf(0), {s.cells(0, 1, wide), s.cells(1, 0, wide+1)});

    editor->setSize(200, 300);
    s.frame();

    const size_t C=editor->getWrapCells();

    REQUIRE(C < wide);
    REQUIRE(editor->getLineRows(0) == (2*wide+C-1)/C);
    CHECK(editor->getAnchor() == at(0, 1));
    CHECK(editor->getCaret() == at(1, 2));
    CHECK(editor->getSelectedText() == String(2*wide-1, 'x')+"\nne");

    // every row of line 0 covered: the first from cell 1, the last to one
    // past its last glyph
    std::vector<Rect> expected;
    const size_t rows=editor->getLineRows(0);

    for(size_t row=0; row < rows; ++row)
    {
        const size_t begin=(row == 0)?1:0;
        const size_t end=(row+1 < rows)?C:(2*wide-row*C)+1;

        expected.push_back(s.cells(row, begin, end));
    }
    checkRects(s.rectsOf(0), expected);
    checkRects(s.rectsOf(1), {s.cells(0, 0, 2)});
}
