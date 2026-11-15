/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A MarkdownDocument's code blocks, tables, images and raw HTML on the
 *  StubRenderer. It pins:
 *  - a code block is a MarkdownCodeBlock whose text is the code in
 *    codeFont() (the skin's mono face at the body size), Padding in;
 *    scrolled (the default) the text is in a MarkdownCodeScroll, one line
 *    per line of code however narrow the document, the block then taller by
 *    the skin's ScrollBarThickness and the scroller's horizontal range the
 *    overflow, a horizontal wheel moving it and a vertical one not;
 *    wrapped (setCodeWrap(true)) the same text control is the block's own,
 *    at the block's width less the padding, its lines wrapped;
 *  - the highlighter gets the fence's language and the code, and its spans
 *    are shown, Code bits dropped, unless they do not make up the code
 *    (then plain); a new highlighter shows every code block again;
 *  - a table is a MarkdownTable of MarkdownText cells, the header row's
 *    bold, each column its widest cell plus 2*CellPaddingX when they fit,
 *    the lines Border wide round them, a cell placed in its column by the
 *    column's alignment and as high as its row less 2*CellPaddingY; in a
 *    document too narrow the columns add up to its width less the lines and
 *    the long cell wraps; a cell's edit keeps every cell's control and
 *    measures the others' spans no more;
 *  - splitColumns: natural widths that fit, an even share kept by the
 *    narrow columns and the rest in proportion, adding up to the room;
 *  - an image is its alt text (or "image") on a Placeholder span in
 *    italics, and raw HTML its literal text.
 */
#include "headlessFixture.h"

#include <gweni/controls/markdownDocument.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using controls::MarkdownDocument;
using controls::RichSpan;
using controls::internal::MarkdownCodeBlock;
using controls::internal::MarkdownTable;
using controls::internal::MarkdownText;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    MarkdownDocument *document;

    explicit Scene(int width=400)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, 2000);
        document=canvas->newChild<MarkdownDocument>();
    }

    ~Scene()
    {
        delete canvas;
    }

    void settle()
    {
        for(int i=0; i < 10; ++i)
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
            if(canvas->getLayoutStats().passes == 0)
                break;
        }
    }

    void show(const std::string &markdown)
    {
        document->setMarkdown(markdown);
        settle();
    }

    template<typename T>
    T *block(size_t index)
    {
        T *control=dynamic_cast<T *>(document->getBlockControl(index));

        REQUIRE(control);
        return control;
    }
};

const std::string longLine="return printf(\"a long line of code that runs well past the edge\");";
const std::string code="int main()\n{\n    "+longLine+"\n}";

}//namespace

TEST_CASE("Markdown code: a block scrolled sideways, one line per line of code")
{
    Scene scene(200);

    scene.show("```cpp\n"+code+"\n```\n");

    MarkdownCodeBlock *block=scene.block<MarkdownCodeBlock>(0);
    MarkdownText *text=block->getText();

    CHECK(text->getText() == code);
    CHECK(text->getFont().facename == scene.skin.getDefaultMonoFont().facename);
    CHECK(text->getFont().size == doctest::Approx(scene.skin.getDefaultFont().size));
    CHECK(block->getLanguage() == "cpp");
    REQUIRE(block->getScroll());
    CHECK(!block->isWrapped());
    CHECK(text->getParent() == block->getScroll());
    CHECK(text->getLayout().lines().size() == 4);
    CHECK(text->getWidth() >= text->getLayout().size().x);

    const int padding=MarkdownCodeBlock::Padding;

    CHECK(block->getScroll()->getX() == padding);
    CHECK(block->getScroll()->getWidth() == 200-2*padding);
    CHECK(block->getHeight() == text->getHeight()+2*padding+scene.skin.Metrics.ScrollBarThickness);
    CHECK(block->getScroll()->getMaxHorizontalScrollOffset() == text->getWidth()-(200-2*padding));
    CHECK(block->getScroll()->getMaxHorizontalScrollOffset() > 0);

    // a vertical wheel is not the code's; a horizontal one scrolls it
    block->getScroll()->onMouseWheeled(-120);
    scene.settle();
    CHECK(block->getScroll()->getHorizontalScrollOffset() == 0);
    block->getScroll()->onMouseWheeledH(-120);
    scene.settle();
    CHECK(block->getScroll()->getHorizontalScrollOffset() > 0);

    // code that fits takes no bar's room
    Scene wide(1200);

    wide.show("```\n"+code+"\n```\n");

    MarkdownCodeBlock *fits=wide.block<MarkdownCodeBlock>(0);

    CHECK(fits->getHeight() == fits->getText()->getHeight()+2*padding);
    CHECK(fits->getScroll()->getMaxHorizontalScrollOffset() == 0);
}

TEST_CASE("Markdown code: wrapped at the block's width")
{
    Scene scene(200);

    scene.show("```\n"+code+"\n```\n");

    MarkdownCodeBlock *block=scene.block<MarkdownCodeBlock>(0);
    MarkdownText *text=block->getText();
    const size_t textId=text->getControlId();

    scene.document->setCodeWrap(true);
    scene.settle();
    CHECK(block->isWrapped());
    CHECK(block->getScroll() == nullptr);
    CHECK(block->getText()->getControlId() == textId);
    CHECK(text->getParent() == block);
    CHECK(text->getX() == MarkdownCodeBlock::Padding);
    CHECK(text->getWidth() == 200-2*MarkdownCodeBlock::Padding);
    CHECK(text->getLayout().lines().size() > 4);
    CHECK(block->getHeight() == text->getHeight()+2*MarkdownCodeBlock::Padding);

    // and back
    scene.document->setCodeWrap(false);
    scene.settle();
    REQUIRE(block->getScroll());
    CHECK(text->getParent() == block->getScroll());
    CHECK(text->getLayout().lines().size() == 4);

    // a code block shown after the switch takes the document's mode
    scene.document->setCodeWrap(true);
    scene.show("```\n"+code+"\n```\n\n    indented "+longLine+"\n");
    CHECK(scene.block<MarkdownCodeBlock>(1)->isWrapped());
}

TEST_CASE("Markdown code: the highlighter's spans")
{
    Scene scene;
    std::vector<std::string> languages;

    scene.document->setCodeHighlighter([&languages](const String &language, const String &text)
    {
        languages.push_back(language);
        if(text.compare(0, 3, "int") != 0)
            return std::vector<RichSpan>{ RichSpan::plain("not the code") };
        return std::vector<RichSpan>{
            RichSpan::colored("int", Color(255, 0, 0, 255), RichSpan::Code),
            RichSpan::plain(text.substr(3))
        };
    });
    scene.show("```cpp\n"+code+"\n```\n\n```\nother code\n```\n");

    MarkdownText *text=scene.block<MarkdownCodeBlock>(0)->getText();

    REQUIRE(text->getSpans().size() == 2);
    CHECK(text->getSpans()[0].text == "int");
    CHECK((text->getSpans()[0].color == Color(255, 0, 0, 255)));
    CHECK(text->getSpans()[0].style == RichSpan::Plain);
    CHECK(text->getText() == code);
    CHECK(languages == std::vector<std::string>{ "cpp", "" });

    // spans that are not the code: shown plain
    MarkdownText *other=scene.block<MarkdownCodeBlock>(1)->getText();

    REQUIRE(other->getSpans().size() == 1);
    CHECK(other->getText() == "other code");

    // a new highlighter shows every block again
    scene.document->setCodeHighlighter(nullptr);
    CHECK(text->getSpans().size() == 1);
    CHECK(text->getText() == code);
}

TEST_CASE("Markdown table: a grid of cells, columns from their content")
{
    Scene scene(600);

    scene.show("| Left | Centre | Right |\n|:-----|:------:|------:|\n| a | **bold** cell | 1 |\n"
        "| longer text here | `x` | 12345 |\n");

    MarkdownTable *table=scene.block<MarkdownTable>(0);

    REQUIRE(table->getRowCount() == 3);
    REQUIRE(table->getColumnCount() == 3);
    CHECK(table->getCell(0, 1)->getText() == "Centre");
    CHECK(table->getCell(0, 1)->getFont().bold);
    CHECK(!table->getCell(1, 1)->getFont().bold);
    CHECK(table->getCell(2, 0)->getText() == "longer text here");

    const std::vector<int> &edges=table->getColumnEdges();
    const int border=MarkdownTable::Border;
    const int padX=MarkdownTable::CellPaddingX;
    const int padY=MarkdownTable::CellPaddingY;

    REQUIRE(edges.size() == 4);
    for(size_t column=0; column < 3; ++column)
    {
        int widest=0;

        for(size_t row=0; row < 3; ++row)
            widest=std::max(widest, table->getCell(row, column)->getPreferredSize().width);
        INFO(column);
        CHECK(edges[column+1]-edges[column] == widest+2*padX+border);
    }
    CHECK(table->getPreferredSize().width == edges.back()+border);

    // placed by alignment: left, centred, right
    MarkdownText *a=table->getCell(1, 0);
    MarkdownText *one=table->getCell(1, 2);
    MarkdownText *x=table->getCell(2, 1);
    const int centreRoom=edges[2]-edges[1]-border-2*padX;

    CHECK(a->getX() == edges[0]+border+padX);
    CHECK(one->getX()+one->getWidth() == edges[3]-padX);
    CHECK(x->getX() == edges[1]+border+padX+(centreRoom-x->getWidth())/2);

    // rows: their tallest cell and the padding, between the lines
    const std::vector<int> &rows=table->getRowEdges();

    REQUIRE(rows.size() == 4);
    CHECK(a->getY() == rows[1]+border+padY);
    CHECK(rows[2]-rows[1] == a->getHeight()+2*padY+border);
    CHECK(table->getHeight() == rows.back()+border);
}

TEST_CASE("Markdown table: too wide for the document, its columns narrowed and cells wrapped")
{
    Scene scene(120);

    scene.show("| A | B |\n|---|---|\n| short | a much longer cell that will not fit |\n");

    MarkdownTable *table=scene.block<MarkdownTable>(0);
    const std::vector<int> &edges=table->getColumnEdges();
    MarkdownText *longCell=table->getCell(1, 1);

    CHECK(edges.back()+MarkdownTable::Border == 120);
    CHECK(longCell->getWidth() == edges[2]-edges[1]-MarkdownTable::Border-2*MarkdownTable::CellPaddingX);
    CHECK(longCell->getLayout().lines().size() > 1);
    CHECK(longCell->getHeight() > table->getCell(1, 0)->getHeight());
}

TEST_CASE("Markdown table: a cell's edit keeps every cell's control")
{
    Scene scene;

    scene.show("| A | B |\n|---|---|\n| one | two |\n| three | four |\n");

    MarkdownTable *table=scene.block<MarkdownTable>(0);
    std::vector<size_t> ids;
    std::vector<size_t> measures;

    for(size_t row=0; row < 3; ++row)
    {
        for(size_t column=0; column < 2; ++column)
        {
            ids.push_back(table->getCell(row, column)->getControlId());
            measures.push_back(table->getCell(row, column)->measureCount());
        }
    }

    scene.show("| A | B |\n|---|---|\n| one | TWO! |\n| three | four |\n");
    CHECK(scene.block<MarkdownTable>(0) == table);
    CHECK(table->getCell(1, 1)->getText() == "TWO!");

    size_t i=0;

    for(size_t row=0; row < 3; ++row)
    {
        for(size_t column=0; column < 2; ++column, ++i)
        {
            INFO(row << "," << column);
            CHECK(table->getCell(row, column)->getControlId() == ids[i]);
            if((row != 1) || (column != 1))
                CHECK(table->getCell(row, column)->measureCount() == measures[i]);
            else
                CHECK(table->getCell(row, column)->measureCount() > measures[i]);
        }
    }

    // a row added: new cells, the others kept
    scene.show("| A | B |\n|---|---|\n| one | TWO! |\n| three | four |\n| five | six |\n");
    REQUIRE(table->getRowCount() == 4);
    CHECK(table->getCell(0, 0)->getControlId() == ids[0]);
    CHECK(table->getCell(3, 1)->getText() == "six");
}

TEST_CASE("Markdown table: splitColumns")
{
    using Widths=std::vector<int>;

    CHECK(MarkdownTable::splitColumns(Widths{ 10, 20, 30 }, 100) == Widths{ 10, 20, 30 });
    CHECK(MarkdownTable::splitColumns(Widths{ 10, 20, 30 }, 60) == Widths{ 10, 20, 30 });
    // 10 is under an even share of 200 and keeps it; 100 and 300 share the
    // 190 left in proportion, 47.5 and 142.5, the remainder to the first
    CHECK(MarkdownTable::splitColumns(Widths{ 10, 100, 300 }, 200) == Widths{ 10, 48, 142 });
    // every column over its share: all in proportion
    CHECK(MarkdownTable::splitColumns(Widths{ 100, 100 }, 50) == Widths{ 25, 25 });
    CHECK(MarkdownTable::splitColumns(Widths{}, 50).empty());
}

TEST_CASE("Markdown images and raw HTML")
{
    Scene scene;

    scene.show("An ![a cat](cat.png) and ![](none.png) here.\n\n<div>\n*raw*\n</div>\n");

    MarkdownText *paragraph=scene.block<MarkdownText>(0);
    const std::vector<RichSpan> &spans=paragraph->getSpans();

    REQUIRE(spans.size() == 5);
    CHECK(spans[1].text == "a cat");
    CHECK(spans[1].style == (RichSpan::Italic|RichSpan::Placeholder));
    CHECK(spans[3].text == "image");
    CHECK(spans[3].style == (RichSpan::Italic|RichSpan::Placeholder));
    CHECK(scene.block<MarkdownText>(1)->getText() == "<div>\n*raw*\n</div>");
}
