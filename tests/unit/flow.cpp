/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  layout::Flow on the StubRenderer: the column count of a Flow W units wide
 *  is floor((C+gap)/(minCardWidth+gap)) of its content width C, clamped to
 *  1..maxColumns, or with breakpoints the first entry W is below (past the
 *  last, maxColumns), compared with the Flow's own width and not the
 *  canvas's, and the same at every canvas scale. Its cards, row-major, are
 *  (C-(cols-1)*gap)/cols wide, the last column taking the remainder, gap
 *  apart; each row is its tallest card's height and every card in it that
 *  high, rows gap apart; its preferred height is the rows and gaps (and its
 *  padding), and a hidden card takes no cell. A card's change that keeps
 *  its row's height enters the Flow and that card alone (no other card's
 *  calculateSize or arrange runs) and moves no other card, with few cards
 *  and with enough for the parent's changed-children list; one that makes
 *  its row taller moves the rows below. In a stable-gutter ScrollControl a
 *  Flow of wrapped-text cards lays out in one pass on its first frame, its
 *  cards' text wrapped once, at the final width (its Y measure before the X
 *  arrange enters no card), and in one pass on a resize across a column
 *  count, as a canvas built at that size lays it out.
 */
#include "headlessFixture.h"

#include <gweni/controls/layout/flow.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/text.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using controls::layout::Flow;

namespace
{

//! A card that asks for a fixed size and counts the layout steps it is
//! entered in.
class Block: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Block, controls::Base)
    {}

    Size want{50, 20};
    int entries=0;

    void calculateSize(skin::Base * /*skin*/, Dim dim) override
    {
        ++entries;
        if(dim == Dim::X)
            m_preferredSize.width=want.width;
        else
            m_preferredSize.height=want.height;
    }

    void arrange(skin::Base *skin, Dim dim) override
    {
        ++entries;
        ParentClass::arrange(skin, dim);
    }

    void setWant(int width, int height)
    {
        want=Size(width, height);
        invalidate();
    }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    Flow *flow;
    std::vector<Block *> cards;

    explicit Scene(int width=1000, int count=7)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, 600);
        flow=canvas->newChild<Flow>();
        flow->setGap(10);
        flow->setMinCardWidth(200);
        flow->setMaxColumns(3);
        for(int i=0; i < count; ++i)
        {
            cards.push_back(flow->newChild<Block>());
            cards.back()->want.height=20+i;
        }
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
};

void checkOnePass(const controls::LayoutStats &stats)
{
    INFO("re-dirtied by " << std::string(stats.redirtiedType?stats.redirtiedType:"none")
        << "#" << stats.redirtiedId);
    CHECK(stats.passes == 1);
    CHECK(stats.redirtiedType == nullptr);
    CHECK_FALSE(stats.renderLayout);
}

std::string rect(const controls::Base *control)
{
    const Rect &r=control->getBounds();

    return std::to_string(r.x)+","+std::to_string(r.y)+" "+std::to_string(r.w)+"x"+std::to_string(r.h);
}

}//namespace

TEST_CASE("Flow: the column count is the min-card-width rule, clamped, or the breakpoint the Flow's width is below")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas canvas(&skin);
    Flow *flow=canvas.newChild<Flow>();
    flow->setGap(10);
    flow->setMinCardWidth(200);
    flow->setMaxColumns(3);

    // floor((C+10)/210): two columns need 410
    CHECK(flow->columnsFor(0, 0) == 1);
    CHECK(flow->columnsFor(150, 150) == 1);
    CHECK(flow->columnsFor(409, 409) == 1);
    CHECK(flow->columnsFor(410, 410) == 2);
    CHECK(flow->columnsFor(619, 619) == 2);
    CHECK(flow->columnsFor(620, 620) == 3);
    // clamped to the maximum
    CHECK(flow->columnsFor(5000, 5000) == 3);
    // the content width decides it, not the width
    CHECK(flow->columnsFor(420, 400) == 1);

    // given out of order: kept sorted, the first entry the width is below
    flow->setBreakpoints({{1536, 2}, {1024, 1}});
    REQUIRE(flow->getBreakpoints().size() == 2);
    CHECK(flow->getBreakpoints()[0].below == 1024);
    CHECK(flow->columnsFor(300, 300) == 1);
    CHECK(flow->columnsFor(1023, 1023) == 1);
    CHECK(flow->columnsFor(1024, 1024) == 2);
    CHECK(flow->columnsFor(1535, 1535) == 2);
    // past the last: the maximum
    CHECK(flow->columnsFor(1536, 1536) == 3);
    // the Flow's own width, whatever its padding leaves
    CHECK(flow->columnsFor(1024, 1000) == 2);

    flow->setBreakpoints({});
    CHECK(flow->columnsFor(620, 620) == 3);
}

TEST_CASE("Flow: column spans are equal whole units, gap apart, the last taking the remainder")
{
    int x, w;

    // 1000 less 2 gaps of 10 is 980: 326 each, 2 left for the last
    Flow::columnSpan(1000, 3, 10, 0, x, w);
    CHECK(x == 0);
    CHECK(w == 326);
    Flow::columnSpan(1000, 3, 10, 1, x, w);
    CHECK(x == 336);
    CHECK(w == 326);
    Flow::columnSpan(1000, 3, 10, 2, x, w);
    CHECK(x == 672);
    CHECK(w == 328);

    Flow::columnSpan(1000, 1, 10, 0, x, w);
    CHECK(x == 0);
    CHECK(w == 1000);
}

TEST_CASE("Flow: cards are placed row-major at their column, every card its row's height, the rows gap apart")
{
    // 7 cards in 3 columns: rows of 3, 3 and 1, the cards 20+i high
    Scene scene(1000, 7);
    scene.flow->setPadding(Padding(4, 6, 8, 2));
    scene.settle();

    // the content box is 1000-4-8=988: (988-20)/3=322, the last 988-664=324
    CHECK(scene.flow->getColumns() == 3);
    CHECK(rect(scene.flow) == "0,0 1000x"+std::to_string(6+22+10+25+10+26+2));
    CHECK(rect(scene.cards[0]) == "4,6 322x22");
    CHECK(rect(scene.cards[1]) == "336,6 322x22");
    CHECK(rect(scene.cards[2]) == "668,6 324x22");
    CHECK(rect(scene.cards[3]) == "4,38 322x25");
    CHECK(rect(scene.cards[4]) == "336,38 322x25");
    CHECK(rect(scene.cards[5]) == "668,38 324x25");
    CHECK(rect(scene.cards[6]) == "4,73 322x26");
    CHECK(scene.flow->getPreferredSize().height == 6+22+10+25+10+26+2);
    // one column wide, whatever its width
    CHECK(scene.flow->getPreferredSize().width == 200+4+8);

    // a hidden card takes no cell: the ones after it move up one
    scene.cards[1]->hide();
    scene.settle();
    CHECK(rect(scene.cards[2]) == "336,6 322x23");
    CHECK(rect(scene.cards[3]) == "668,6 324x23");
    CHECK(rect(scene.cards[4]) == "4,39 322x26");
    CHECK(rect(scene.cards[6]) == "668,39 324x26");
    CHECK(scene.flow->getPreferredSize().height == 6+23+10+26+2);

    // narrower: two columns, 2*200+10 fits 588 and 3*200+20 does not
    scene.cards[1]->show();
    scene.canvas->setSize(600, 600);
    scene.settle();
    CHECK(scene.flow->getColumns() == 2);
    // (588-10)/2=289, the last 588-299=289
    CHECK(rect(scene.cards[0]) == "4,6 289x21");
    CHECK(rect(scene.cards[1]) == "303,6 289x21");
    CHECK(rect(scene.cards[2]) == "4,37 289x23");
    CHECK(rect(scene.cards[6]) == "4,"+std::to_string(6+21+10+23+10+25+10)+" 289x26");
    CHECK(scene.flow->getPreferredSize().height == 6+21+10+23+10+25+10+26+2);
}

TEST_CASE("Flow: breakpoints are compared with the Flow's own width, not the canvas's")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);
    canvas->setSize(3000, 600);
    controls::Base *pane=canvas->newChild<controls::Base>();
    pane->setDock(DockPosition::Left);
    pane->setSizeFlags({SizeFlag::Fixed, SizeFlag::Elastic});
    pane->setWidth(1023);
    Flow *flow=pane->newChild<Flow>();
    flow->setBreakpoints({{1024, 1}, {1536, 2}});
    flow->setMaxColumns(3);
    flow->newChild<Block>();
    flow->newChild<Block>();

    auto frame=[canvas]() { canvas->updateCanvas(); canvas->renderCanvas(); };

    frame();
    frame();
    CHECK(flow->getWidth() == 1023);
    CHECK(flow->getColumns() == 1);

    pane->setWidth(1024);
    frame();
    CHECK(flow->getColumns() == 2);

    pane->setWidth(1535);
    frame();
    CHECK(flow->getColumns() == 2);

    pane->setWidth(1536);
    frame();
    CHECK(flow->getColumns() == 3);
    delete canvas;
}

TEST_CASE("Flow: the column count and the logical bounds are the same at every canvas scale")
{
    for(int width : {409, 410, 619, 620, 1000})
    {
        Scene one(width, 5);
        one.settle();
        std::vector<std::string> expected;

        for(const Block *card:one.cards)
            expected.push_back(rect(card));

        for(float scale : {1.25f, 1.5f, 2.f})
        {
            Scene scaled(width, 5);
            scaled.canvas->setScale(scale);
            scaled.settle();
            INFO("width " << width << " scale " << scale);
            CHECK(scaled.flow->getColumns() == one.flow->getColumns());
            for(size_t i=0; i < scaled.cards.size(); ++i)
                CHECK(rect(scaled.cards[i]) == expected[i]);
        }
    }
}

TEST_CASE("Flow: a card's change that keeps its row's height enters the Flow and that card alone")
{
    for(int count : {7, 40})
    {
        INFO(count << " cards");
        Scene scene(1000, count);
        scene.settle();

        // card 4 is in the second row with 3 and 5, which is 25 high (card 5)
        std::vector<std::string> before;

        for(const Block *card:scene.cards)
            before.push_back(rect(card));
        for(Block *card:scene.cards)
            card->entries=0;

        scene.cards[4]->setWant(50, 22);
        const controls::LayoutStats &stats=scene.frame();
        checkOnePass(stats);
        // the canvas, the Flow and the card: each its two measures and two arranges
        CHECK(stats.visited == 3*4);
        for(size_t i=0; i < scene.cards.size(); ++i)
        {
            INFO("card " << i);
            CHECK(scene.cards[i]->entries == ((i == 4)?4:0));
            CHECK(rect(scene.cards[i]) == before[i]);
        }

        // taller than its row: that row grows and the rows below move down
        scene.cards[4]->setWant(50, 30);
        checkOnePass(scene.frame());
        CHECK(scene.cards[3]->getHeight() == 30);
        CHECK(scene.cards[6]->getY() == std::stoi(before[6].substr(before[6].find(',')+1))+5);
        CHECK(rect(scene.cards[0]) == before[0]);
    }
}

namespace
{

std::string words(int count)
{
    std::string s;

    for(int i=0; i < count; ++i)
        s+="word"+std::to_string(i)+" ";
    return s;
}

//! FlowDemo's arrangement: wrapped-text cards in a Flow docked Top in a
//! vertical ScrollControl with a stable gutter.
struct ScrollScene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::ScrollControl *scroll;
    Flow *flow;
    std::vector<controls::Label *> texts;

    explicit ScrollScene(int width)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, 300);
        scroll=canvas->newChild<controls::ScrollControl>();
        scroll->setDock(DockPosition::Center);
        scroll->setScroll(false, true);
        scroll->setAutoHideBars(false);
        flow=scroll->newChild<Flow>();
        flow->setGap(8);
        flow->setBreakpoints({{500, 1}, {800, 2}});
        flow->setMaxColumns(3);
        for(int i=0; i < 9; ++i)
        {
            controls::Base *card=flow->newChild<controls::Base>();
            card->setPadding(Padding(4, 4, 4, 4));
            controls::Label *text=card->newChild<controls::Label>();
            text->setDock(DockPosition::Top);
            text->setWrap(true);
            text->setText(words(10+7*i));
            texts.push_back(text);
        }
    }

    ~ScrollScene()
    {
        delete canvas;
    }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    std::vector<std::string> bounds() const
    {
        std::vector<std::string> out{rect(flow)};

        for(const controls::Label *text:texts)
            out.push_back(rect(text->getParent())+" "+rect(text));
        out.push_back(std::to_string(scroll->getMaxVerticalScrollOffset()));
        return out;
    }
};

const controls::internal::Text *textOf(const controls::Label *label)
{
    for(const controls::Base *child:const_cast<controls::Label *>(label)->getChildren())
    {
        if(auto *text=dynamic_cast<const controls::internal::Text *>(child))
            return text;
    }
    return nullptr;
}

}//namespace

TEST_CASE("Flow: wrapped-text cards in a stable-gutter ScrollControl lay out in one pass, first frame and resizes")
{
    ScrollScene scene(700);
    checkOnePass(scene.frame());
    CHECK(scene.frame().passes == 0);
    CHECK(scene.flow->getColumns() == 2);
    // each card's text wrapped once, at the card's width: never at the width
    // a card has before the Flow placed it
    for(const controls::Label *label:scene.texts)
    {
        REQUIRE(textOf(label));
        CHECK(textOf(label)->wrapCount() == 1);
    }
    // the content overflows: the rows at the cards' wrapped heights
    CHECK(scene.scroll->getMaxVerticalScrollOffset() > 0);
    CHECK(scene.scroll->getMaxVerticalScrollOffset()
        == scene.flow->getHeight()-scene.scroll->getViewport().h);

    for(int width : {400, 900, 700, 520, 530, 700})
    {
        INFO("width " << width);
        scene.canvas->setSize(width, 300);
        checkOnePass(scene.frame());
        CHECK(scene.frame().passes == 0);

        ScrollScene fresh(width);
        checkOnePass(fresh.frame());
        CHECK(scene.flow->getColumns() == fresh.flow->getColumns());
        CHECK(scene.bounds() == fresh.bounds());
    }
}
