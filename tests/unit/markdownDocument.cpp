/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::MarkdownDocument on the StubRenderer. It pins:
 *  - each block's control: a paragraph a MarkdownText of its spans in the
 *    body font, a heading one in headingFont(level) (bold, its size), a rule
 *    a MarkdownRule, a quote a MarkdownQuote of its blocks (nested quotes
 *    too), a list a MarkdownList of MarkdownListItems whose markers are the
 *    numbers from the list's start with its delimiter, or bullets by depth;
 *  - the layout at the width it is given: every block that wide less its
 *    padding, stacked with BlockSpacing between them (TightSpacing between a
 *    tight list's items), a quote's blocks BarWidth+BarGap in, a list item's
 *    ListIndent in with its marker right-aligned MarkerGap before them on
 *    their first line, its preferred height the blocks' and spacing, and a
 *    paragraph that wraps at a narrower width making it taller;
 *  - setMarkdown keeps the controls of matched blocks: an edited paragraph
 *    keeps its control (new spans), the others theirs, at the start, middle
 *    and end; an inserted block is a new control, the others kept; a deleted
 *    one is hidden at once and deleted with the next frame's delayed
 *    deletes; a swap keeps both controls and puts them in the new order
 *    among the children; a list item edited in a nested list keeps the
 *    lists' and items' controls, its paragraph's included; an item inserted
 *    in an ordered list renumbers the markers after it;
 *  - the same text again, and a text that parses to the same blocks, lay
 *    out and mark nothing;
 *  - a paragraph's edit that keeps its height lays out in one pass entering
 *    as many controls in a 6-block document as in a 300-block one, and one
 *    that makes it taller moves the blocks below by the growth;
 *  - setFont gives every text block, headings and markers the new font.
 */
#include "headlessFixture.h"

#include <gweni/controls/markdownDocument.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using controls::MarkdownDocument;
using controls::internal::MarkdownColumn;
using controls::internal::MarkdownList;
using controls::internal::MarkdownListItem;
using controls::internal::MarkdownQuote;
using controls::internal::MarkdownRule;
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

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i < 10 && frame().passes != 0; ++i)
            ;
    }

    void show(const std::string &markdown)
    {
        document->setMarkdown(markdown);
        settle();
    }
};

std::string textOf(controls::Base *control)
{
    MarkdownText *text=dynamic_cast<MarkdownText *>(control);

    REQUIRE(text);
    return text->getText();
}

template<typename T>
T *blockAs(MarkdownColumn *column, size_t index)
{
    T *block=dynamic_cast<T *>(column->getBlockControl(index));

    REQUIRE(block);
    return block;
}

std::vector<size_t> idsOf(MarkdownColumn *column)
{
    std::vector<size_t> ids;

    for(size_t i=0; i < column->getBlockCount(); ++i)
        ids.push_back(column->getBlockControl(i)->getControlId());
    return ids;
}

const char *document=
    "# Title\n"
    "\n"
    "First paragraph.\n"
    "\n"
    "Second paragraph.\n"
    "\n"
    "Third paragraph.\n";

}//namespace

TEST_CASE("Markdown document: each block's control")
{
    Scene scene;

    scene.show("# Heading *it*\n\n### Three\n\nBody **bold**.\n\n---\n\n> quote\n> > nested\n\n"
        "- a\n- b\n  1) x\n  2) y\n     - deep\n\n7. seven\n8. eight\n");

    MarkdownDocument *doc=scene.document;

    REQUIRE(doc->getBlockCount() == 7);

    MarkdownText *h1=blockAs<MarkdownText>(doc, 0);

    CHECK(h1->getText() == "Heading it");
    CHECK(h1->getFont().bold);
    CHECK(h1->getFont().size == doctest::Approx(24.0f));
    CHECK(h1->getSpans()[1].style == controls::RichSpan::Italic);
    CHECK(blockAs<MarkdownText>(doc, 1)->getFont().size == doctest::Approx(17.0f));

    MarkdownText *body=blockAs<MarkdownText>(doc, 2);

    CHECK(body->getText() == "Body bold.");
    CHECK(!body->getFont().bold);
    CHECK(body->getFont().size == doctest::Approx(11.0f));
    CHECK(body->getSpans()[1].style == controls::RichSpan::Bold);

    blockAs<MarkdownRule>(doc, 3);

    MarkdownQuote *quote=blockAs<MarkdownQuote>(doc, 4);

    REQUIRE(quote->getBlockCount() == 2);
    CHECK(textOf(quote->getBlockControl(0)) == "quote");
    CHECK(textOf(blockAs<MarkdownQuote>(quote, 1)->getBlockControl(0)) == "nested");

    MarkdownList *list=blockAs<MarkdownList>(doc, 5);

    REQUIRE(list->getBlockCount() == 2);
    CHECK(list->getDepth() == 0);
    CHECK(list->isTight());

    MarkdownListItem *a=blockAs<MarkdownListItem>(list, 0);
    MarkdownListItem *b=blockAs<MarkdownListItem>(list, 1);

    CHECK(a->getMarker()->getText() == "\xE2\x80\xA2");
    CHECK(textOf(a->getBlockControl(0)) == "a");
    REQUIRE(b->getBlockCount() == 2);

    MarkdownList *ordered=blockAs<MarkdownList>(b, 1);

    CHECK(ordered->getDepth() == 1);
    CHECK(blockAs<MarkdownListItem>(ordered, 0)->getMarker()->getText() == "1)");
    CHECK(blockAs<MarkdownListItem>(ordered, 1)->getMarker()->getText() == "2)");

    MarkdownList *deep=blockAs<MarkdownList>(blockAs<MarkdownListItem>(ordered, 1), 1);

    CHECK(deep->getDepth() == 2);
    CHECK(blockAs<MarkdownListItem>(deep, 0)->getMarker()->getText() == "\xE2\x96\xAA");

    // an ordered list's markers count from its start
    MarkdownList *seven=blockAs<MarkdownList>(doc, 6);

    CHECK(blockAs<MarkdownListItem>(seven, 0)->getMarker()->getText() == "7.");
    CHECK(blockAs<MarkdownListItem>(seven, 1)->getMarker()->getText() == "8.");
}

TEST_CASE("Markdown document: lays out at the width it is given")
{
    Scene scene(400);

    scene.show("Para one.\n\n> quoted\n\n- item\n- two\n\nLast.\n");

    MarkdownDocument *doc=scene.document;

    CHECK(doc->getWidth() == 400);
    REQUIRE(doc->getBlockCount() == 4);

    int y=0;

    for(size_t i=0; i < doc->getBlockCount(); ++i)
    {
        controls::Base *block=doc->getBlockControl(i);

        INFO(i);
        CHECK(block->getX() == 0);
        CHECK(block->getWidth() == 400);
        CHECK(block->getY() == y);
        CHECK(block->getHeight() == block->getPreferredSize().height);
        y+=block->getHeight()+MarkdownDocument::BlockSpacing;
    }
    CHECK(doc->getPreferredSize().height == y-MarkdownDocument::BlockSpacing);
    CHECK(doc->getHeight() == doc->getPreferredSize().height);

    MarkdownQuote *quote=blockAs<MarkdownQuote>(doc, 1);
    const int barRoom=MarkdownQuote::BarWidth+MarkdownQuote::BarGap;

    CHECK(quote->getBlockControl(0)->getX() == barRoom);
    CHECK(quote->getBlockControl(0)->getWidth() == 400-barRoom);

    MarkdownList *list=blockAs<MarkdownList>(doc, 2);
    MarkdownListItem *first=blockAs<MarkdownListItem>(list, 0);
    MarkdownListItem *second=blockAs<MarkdownListItem>(list, 1);

    CHECK(second->getY() == first->getHeight()+MarkdownDocument::TightSpacing);
    CHECK(first->getBlockControl(0)->getX() == MarkdownDocument::ListIndent);
    CHECK(first->getBlockControl(0)->getWidth() == 400-MarkdownDocument::ListIndent);

    MarkdownText *marker=first->getMarker();

    CHECK(marker->getWidth() > 0);
    CHECK(marker->getX()+marker->getWidth() == MarkdownDocument::ListIndent-MarkdownListItem::MarkerGap);
    CHECK(marker->getY() == first->getBlockControl(0)->getY());

    // a long paragraph wraps at a narrower width: taller
    Scene wide(800);
    Scene narrow(200);
    std::string paragraph;

    for(int i=0; i < 40; ++i)
        paragraph+="word ";
    wide.show(paragraph);
    narrow.show(paragraph);
    CHECK(narrow.document->getHeight() > wide.document->getHeight());
    CHECK(narrow.document->getBlockControl(0)->getWidth() == 200);
}

TEST_CASE("Markdown document: setMarkdown keeps the controls of matched blocks")
{
    Scene scene;

    scene.show(document);

    MarkdownDocument *doc=scene.document;
    const std::vector<size_t> before=idsOf(doc);

    REQUIRE(before.size() == 4);

    SUBCASE("an edit at the start, the middle and the end")
    {
        scene.show("# Title!\n\nFirst paragraph.\n\nSecond paragraph.\n\nThird paragraph.\n");
        CHECK(idsOf(doc) == before);
        CHECK(textOf(doc->getBlockControl(0)) == "Title!");

        scene.show("# Title!\n\nFirst paragraph.\n\nSecond, edited.\n\nThird paragraph.\n");
        CHECK(idsOf(doc) == before);
        CHECK(textOf(doc->getBlockControl(2)) == "Second, edited.");

        scene.show("# Title!\n\nFirst paragraph.\n\nSecond, edited.\n\nThird.\n");
        CHECK(idsOf(doc) == before);
        CHECK(textOf(doc->getBlockControl(3)) == "Third.");
    }

    SUBCASE("an insert and a delete")
    {
        scene.show("# Title\n\nFirst paragraph.\n\nInserted.\n\nSecond paragraph.\n\nThird paragraph.\n");

        std::vector<size_t> after=idsOf(doc);

        REQUIRE(after.size() == 5);
        CHECK(after[0] == before[0]);
        CHECK(after[1] == before[1]);
        CHECK(after[3] == before[2]);
        CHECK(after[4] == before[3]);
        CHECK(after[2] != before[0]);
        CHECK(textOf(doc->getBlockControl(2)) == "Inserted.");

        // the controls in the blocks' order among the children
        std::vector<size_t> order;

        for(controls::Base *child:doc->getActualChildren())
            order.push_back(child->getControlId());
        CHECK(order == after);

        controls::Base *removed=doc->getBlockControl(1);
        const size_t removedId=removed->getControlId();

        scene.document->setMarkdown("# Title\n\nInserted.\n\nSecond paragraph.\n\nThird paragraph.\n");
        CHECK(removed->hidden());
        CHECK(doc->getBlockCount() == 4);
        CHECK(idsOf(doc) == std::vector<size_t>{ before[0], after[2], before[2], before[3] });
        scene.settle();
        CHECK(controls::Base::getControlById(removedId) == nullptr);
    }

    SUBCASE("a swap keeps both controls, in the new order")
    {
        scene.show("# Title\n\nSecond paragraph.\n\nFirst paragraph.\n\nThird paragraph.\n");
        CHECK(idsOf(doc) == std::vector<size_t>{ before[0], before[2], before[1], before[3] });

        std::vector<size_t> order;

        for(controls::Base *child:doc->getActualChildren())
            order.push_back(child->getControlId());
        CHECK(order == idsOf(doc));
        CHECK(doc->getBlockControl(1)->getY() < doc->getBlockControl(2)->getY());
    }
}

TEST_CASE("Markdown document: a list item edited in a nested list keeps the lists' controls")
{
    Scene scene;

    scene.show("Intro.\n\n- one\n- two\n  1. a\n  2. b\n  3. c\n- three\n");

    MarkdownList *list=blockAs<MarkdownList>(scene.document, 1);
    MarkdownListItem *two=blockAs<MarkdownListItem>(list, 1);
    MarkdownList *nested=blockAs<MarkdownList>(two, 1);
    const std::vector<size_t> items=idsOf(list);
    const std::vector<size_t> nestedItems=idsOf(nested);
    const size_t bText=blockAs<MarkdownListItem>(nested, 1)->getBlockControl(0)->getControlId();

    scene.show("Intro.\n\n- one\n- two\n  1. a\n  2. b, edited\n  3. c\n- three\n");
    CHECK(scene.document->getBlockControl(1) == list);
    CHECK(idsOf(list) == items);
    CHECK(two->getBlockControl(1) == nested);
    CHECK(idsOf(nested) == nestedItems);

    controls::Base *b=blockAs<MarkdownListItem>(nested, 1)->getBlockControl(0);

    CHECK(b->getControlId() == bText);
    CHECK(textOf(b) == "b, edited");

    // an item inserted before b renumbers the markers after it
    scene.show("Intro.\n\n- one\n- two\n  1. a\n  2. new\n  3. b, edited\n  4. c\n- three\n");
    REQUIRE(nested->getBlockCount() == 4);
    CHECK(nested->getBlockControl(0)->getControlId() == nestedItems[0]);
    CHECK(nested->getBlockControl(2)->getControlId() == nestedItems[1]);
    CHECK(nested->getBlockControl(3)->getControlId() == nestedItems[2]);
    for(size_t i=0; i < 4; ++i)
        CHECK(blockAs<MarkdownListItem>(nested, i)->getMarker()->getText() == std::to_string(i+1)+".");
}

TEST_CASE("Markdown document: the same blocks again lay out and mark nothing")
{
    Scene scene;

    scene.show(document);

    const std::vector<size_t> before=idsOf(scene.document);

    scene.document->setMarkdown(document);
    CHECK(scene.frame().passes == 0);
    // another text, the same blocks: an extra blank line and a trailing one
    scene.document->setMarkdown(std::string("\n")+document+"\n\n");
    CHECK(scene.frame().passes == 0);
    CHECK(idsOf(scene.document) == before);
}

TEST_CASE("Markdown document: an edit lays out its block, the blocks below only moved")
{
    auto visitedForEdit=[](int paragraphs)
    {
        Scene scene;
        std::string before;
        std::string after;

        for(int i=0; i < paragraphs; ++i)
        {
            const std::string text="Paragraph number "+std::to_string(1000+i)+".\n\n";

            before+=text;
            after+=(i == 3)?"Paragraph number XXXX.\n\n":text;
        }
        scene.show(before);
        scene.document->setMarkdown(after);

        const controls::LayoutStats &stats=scene.frame();

        CHECK(stats.passes == 1);
        CHECK(textOf(scene.document->getBlockControl(3)) == "Paragraph number XXXX.");
        return stats.visited;
    };

    const size_t small=visitedForEdit(6);
    const size_t large=visitedForEdit(300);

    CHECK(small > 0);
    CHECK(small == large);

    // a paragraph made taller moves the blocks below by the growth
    Scene scene(200);

    scene.show(document);

    MarkdownDocument *doc=scene.document;
    const int oldHeight=doc->getBlockControl(2)->getHeight();
    const int lastY=doc->getBlockControl(3)->getY();
    std::string longer="# Title\n\nFirst paragraph.\n\nSecond";

    for(int i=0; i < 30; ++i)
        longer+=" word";
    longer+=".\n\nThird paragraph.\n";
    scene.show(longer);

    const int grown=doc->getBlockControl(2)->getHeight()-oldHeight;

    CHECK(grown > 0);
    CHECK(doc->getBlockControl(3)->getY() == lastY+grown);
}

TEST_CASE("Markdown document: setFont gives every text block the new font")
{
    Scene scene;

    scene.show("# Title\n\nBody.\n\n> - item\n");

    Font font=scene.skin.getDefaultFont();

    font.size=22.0f;
    scene.document->setFont(font);
    scene.settle();

    CHECK(blockAs<MarkdownText>(scene.document, 0)->getFont().size == doctest::Approx(48.0f));
    CHECK(blockAs<MarkdownText>(scene.document, 1)->getFont().size == doctest::Approx(22.0f));

    MarkdownQuote *quote=blockAs<MarkdownQuote>(scene.document, 2);
    MarkdownListItem *item=blockAs<MarkdownListItem>(blockAs<MarkdownList>(quote, 0), 0);

    CHECK(blockAs<MarkdownText>(item, 0)->getFont().size == doctest::Approx(22.0f));
    CHECK(item->getMarker()->getFont().size == doctest::Approx(22.0f));
}
