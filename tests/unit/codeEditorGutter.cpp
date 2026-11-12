/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CodeEditor's line-number gutter, on the StubSkin. With setLineNumbers(true)
 *  every shown row has a number Label at its height in the gutter showing its
 *  line's number, one Label per pooled row (a spare row's hidden), so 1 000
 *  and 100 000 lines take the same controls and ids; a scroll gives the
 *  numbers the rows' new lines. The gutter is as wide as the line count's
 *  digits in cells plus GutterInset on each side, left of the viewport in the
 *  editor's padding, and stays where it is when the content scrolls
 *  sideways. A line count crossing 999 to 1000 widens it by a cell and moves
 *  the viewport with it, in a frame whose layout enters the editor's subtree
 *  and the canvas and nothing beside them (a panel of Labels next to the
 *  editor is not entered and keeps its bounds), in one pass. The caret's
 *  line's number takes Colors.CodeEditor.LineNumberCurrent, the others
 *  LineNumber, re-read by updateColors() (a theme changed live); read-only,
 *  none is current. Turning the numbers off hides the gutter and gives the
 *  viewport its room back.
 */
#include "headlessFixture.h"

#include <gweni/controls/codeEditor.h>
#include <gweni/controls/label.h>

#include <doctest/doctest.h>

#include <cmath>
#include <functional>
#include <string>

using namespace gweni;

namespace
{

String numbered(size_t lines)
{
    String text;

    for(size_t i=0; i < lines; ++i)
    {
        if(i > 0)
            text+='\n';
        text+="line "+std::to_string(i);
    }
    return text;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::CodeEditor *editor=nullptr;
    controls::Base *beside=nullptr;

    explicit Scene(size_t lines, int besideLabels=30)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);

        editor=canvas->newChild<controls::CodeEditor>();
        editor->setBounds(0, 0, 400, 200);
        editor->setLineNumbers(true);
        editor->setText(numbered(lines));

        // a panel of Labels beside it, which a layout of the editor alone
        // does not enter
        beside=canvas->newChild<controls::Base>();
        beside->setBounds(420, 0, 300, 500);
        for(int i=0; i < besideLabels; ++i)
        {
            controls::Label *label=beside->newChild<controls::Label>();

            label->setText("label "+std::to_string(i));
            label->setDock(DockPosition::Top);
        }
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

    int maxPasses=0;

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        maxPasses=std::max(maxPasses, canvas->getLayoutStats().passes);
    }
};

size_t subtreeSize(controls::Base *control)
{
    size_t size=1;

    for(controls::Base *child : control->getActualChildren())
        size+=subtreeSize(child);
    return size;
}

//! Every shown row's number shows its line at its height; every other
//! number is hidden.
void checkNumbers(Scene &s)
{
    controls::CodeEditor *editor=s.editor;
    const std::vector<controls::internal::CodeLineNumber *> &numbers=editor->getLineNumberLabels();
    const std::vector<controls::Base *> rows=editor->getRows();
    size_t shown=0;

    REQUIRE(numbers.size() == rows.size());

    for(controls::internal::CodeLineNumber *number : numbers)
    {
        if(number->hidden())
            continue;

        ++shown;
        REQUIRE(number->getLine() != controls::internal::CodeLineNumber::NoLine);

        controls::CodeLine *row=editor->rowForLine(number->getLine());

        REQUIRE(row);
        CHECK(!row->hidden());
        CHECK(number->getText() == std::to_string(number->getLine()+1));
        CHECK(number->getY() == row->getY());
        CHECK(number->getHeight() == row->getHeight());
    }

    size_t shownRows=0;

    for(controls::Base *row : rows)
        shownRows+=row->hidden()?0:1;
    CHECK(shown == shownRows);
}

int expectedWidth(const controls::CodeEditor *editor, int digits)
{
    return 2*controls::CodeEditor::GutterInset+static_cast<int>(std::ceil(digits*static_cast<double>(editor->getCellWidth())));
}

}//namespace

TEST_CASE("CodeEditor gutter: a number per shown row, bound with the rows, the same controls at 1 000 and 100 000 lines")
{
    auto take=[](size_t lines)
        {
            const test::IdBankCounts before=test::IdBankCounts::now();
            Scene s(lines);
            const test::IdBankCounts after=test::IdBankCounts::now();

            checkNumbers(s);
            CHECK(s.editor->getGutterWidth() == expectedWidth(s.editor, static_cast<int>(std::to_string(lines).size())));

            s.editor->setVerticalScrollOffset(s.editor->getMaxVerticalScrollOffset()/2+3);
            s.frame();
            checkNumbers(s);
            s.editor->setVerticalScrollOffset(s.editor->getMaxVerticalScrollOffset());
            s.frame();
            checkNumbers(s);
            CHECK(s.maxPasses <= 1);

            return std::make_pair(s.skin.liveControls, after.livePrimitives()-before.livePrimitives());
        };

    CHECK(take(1000) == take(100000));
}

TEST_CASE("CodeEditor gutter: 999 to 1000 lines widens it by a cell and lays out the editor alone")
{
    Scene s(999);
    controls::CodeEditor *editor=s.editor;
    const int width999=editor->getGutterWidth();

    CHECK(width999 == expectedWidth(editor, 3));
    CHECK(editor->getViewport().x == controls::CodeEditor::FieldPadding+width999);

    std::vector<Rect> besideBounds;
    for(controls::Base *child : s.beside->getChildren())
        besideBounds.push_back(child->getBounds());
    const Rect editorBounds=editor->getBounds();

    // the 1000th line
    editor->getDocument().insert(editor->getDocument().endPosition(), "\nlast");
    s.frame();

    const controls::LayoutStats &layout=s.canvas->getLayoutStats();

    CHECK(editor->getLineCount() == 1000);
    CHECK(editor->getGutterWidth() == expectedWidth(editor, 4));
    CHECK(editor->getGutterWidth() > width999);
    CHECK(editor->getViewport().x == controls::CodeEditor::FieldPadding+editor->getGutterWidth());
    CHECK(layout.passes == 1);
    // the editor's subtree and the canvas, 4 entries each at most
    CHECK(layout.visited <= 4*(subtreeSize(editor)+1));
    // and the same entries with no Labels beside it: none of theirs
    {
        Scene alone(999, 0);

        alone.editor->getDocument().insert(alone.editor->getDocument().endPosition(), "\nlast");
        alone.frame();
        CHECK(alone.canvas->getLayoutStats().visited == layout.visited);
    }
    CHECK((editor->getBounds() == editorBounds));
    size_t i=0;
    for(controls::Base *child : s.beside->getChildren())
        CHECK((child->getBounds() == besideBounds[i++]));
    checkNumbers(s);

    // and back
    editor->getDocument().erase({{998, editor->getDocument().line(998).size()}, editor->getDocument().endPosition()});
    s.frame();
    CHECK(editor->getGutterWidth() == width999);
    CHECK(s.canvas->getLayoutStats().visited <= 4*(subtreeSize(editor)+1));
}

TEST_CASE("CodeEditor gutter: it does not scroll sideways, and the numbers can be turned off")
{
    Scene s(50);
    controls::CodeEditor *editor=s.editor;

    editor->getDocument().insert({3, 0}, String(300, 'x'));
    s.frame();

    controls::Base *gutter=editor->getGutter();
    const Rect gutterBounds=gutter->getBounds();
    const Rect numberBounds=editor->getLineNumberLabels()[0]->getBounds();

    CHECK(gutterBounds.x == controls::CodeEditor::FieldPadding);
    CHECK(gutterBounds.w == editor->getGutterWidth());
    // the gutter is the editor's own child, not the scrolled content's
    CHECK(gutter->getParent() == editor);
    CHECK(gutter->getActualParent() == editor);

    editor->setHorizontalScrollOffset(100);
    s.frame();
    CHECK(editor->getHorizontalScrollOffset() == 100);
    CHECK((gutter->getBounds() == gutterBounds));
    CHECK((editor->getLineNumberLabels()[0]->getBounds() == numberBounds));

    editor->setLineNumbers(false);
    s.frame();
    CHECK(gutter->hidden());
    CHECK(editor->getGutterWidth() == 0);
    CHECK(editor->getViewport().x == controls::CodeEditor::FieldPadding);
    CHECK(s.maxPasses <= 1);
}

TEST_CASE("CodeEditor gutter: the caret's line's number takes the current colour")
{
    Scene s(100);
    controls::CodeEditor *editor=s.editor;
    auto numberOf=[&](size_t line) -> controls::internal::CodeLineNumber *
        {
            for(controls::internal::CodeLineNumber *number : editor->getLineNumberLabels())
            {
                if(!number->hidden() && number->getLine() == line)
                    return number;
            }
            return nullptr;
        };
    auto same=[](const Color &a, const Color &b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; };

    s.skin.Colors.CodeEditor.LineNumber=Color(10, 20, 30);
    s.skin.Colors.CodeEditor.LineNumberCurrent=Color(200, 210, 220);
    for(controls::internal::CodeLineNumber *number : editor->getLineNumberLabels())
        number->updateColors();

    editor->setCaret({4, 0});
    s.frame();
    REQUIRE(numberOf(4));
    CHECK(numberOf(4)->isCurrentLine());
    CHECK(same(numberOf(4)->getTextColor(), Color(200, 210, 220)));
    CHECK(!numberOf(3)->isCurrentLine());
    CHECK(same(numberOf(3)->getTextColor(), Color(10, 20, 30)));

    editor->setCaret({5, 0});
    s.frame();
    CHECK(!numberOf(4)->isCurrentLine());
    CHECK(same(numberOf(4)->getTextColor(), Color(10, 20, 30)));
    CHECK(numberOf(5)->isCurrentLine());

    // a theme changed live: updateColors() reads the skin again
    s.skin.Colors.CodeEditor.LineNumber=Color(1, 2, 3);
    numberOf(4)->updateColors();
    CHECK(same(numberOf(4)->getTextColor(), Color(1, 2, 3)));

    editor->setReadOnly(true);
    s.frame();
    for(controls::internal::CodeLineNumber *number : editor->getLineNumberLabels())
        CHECK(!number->isCurrentLine());
}
