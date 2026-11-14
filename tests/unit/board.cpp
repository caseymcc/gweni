/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Board owns fixed-width columns at the viewport's height, independent of
 *  card height, in one layout pass. Each BoardColumn has a separate header
 *  and a vertical-only body with a permanent gutter. Adding or growing
 *  wrapped cards preserves all scroll offsets and existing card widths.
 *
 *  A body's vertical wheel is consumed when it moves, fits or hits either
 *  end, without marking outside that body. A vertical wheel over headers,
 *  gaps and background moves Board sideways; horizontal and Shift+wheel
 *  reach Board even over a body. Hiding, removing and reparenting columns
 *  updates their owned placement and the horizontal range.
 */
#include "headlessFixture.h"

#include <gweni/controls/board.h>
#include <gweni/controls/boardColumn.h>
#include <gweni/controls/verticalScrollBar.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

std::string paragraph(int words)
{
    std::string text;
    for(int i=0; i < words; ++i)
        text+="word"+std::to_string(i)+" ";
    return text;
}

controls::Label *addCard(controls::BoardColumn *column, int words)
{
    auto *card=column->newChild<controls::Label>();
    card->setDock(DockPosition::Top);
    card->setWrap(true);
    card->setMouseInputEnabled(true);
    card->setText(paragraph(words));
    return card;
}

controls::BaseScrollBar *verticalBar(controls::ScrollControl *body)
{
    for(controls::Base *child:body->getActualChildren())
        if(auto *bar=gweni_cast<controls::VerticalScrollBar>(child))
            return bar;
    return nullptr;
}

void checkOnePass(const controls::LayoutStats &stats)
{
    INFO("re-dirtied by " << std::string(stats.redirtiedType?stats.redirtiedType:"none"));
    CHECK(stats.passes == 1);
    CHECK(stats.redirtiedType == nullptr);
    CHECK_FALSE(stats.renderLayout);
}

void checkNoPendingOutside(controls::Base *root, controls::Base *subtree)
{
    if(root == subtree)
        return;
    INFO("outside body: " << std::string(root->getTypeName()));
    CHECK_FALSE(root->isQueuedForRender());
    for(controls::Base *child:root->getActualChildren())
        checkNoPendingOutside(child, subtree);
}

struct Scene
{
    const bool wasVerifying=controls::layoutVerifyEnabled();
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas canvas{&skin};
    controls::Board *board;
    std::vector<controls::BoardColumn *> columns;
    controls::Label *longCard;
    controls::Label *shortCard;

    Scene()
    {
        controls::setLayoutVerify(true);
        canvas.setSize(500, 240);
        board=canvas.newChild<controls::Board>();
        board->setColumnWidth(180);
        board->setGap(14);
        for(int i=0; i < 4; ++i)
        {
            auto *column=board->newChild<controls::BoardColumn>();
            column->setText("Column "+std::to_string(i));
            columns.push_back(column);
        }
        longCard=addCard(columns[0], 200);
        shortCard=addCard(columns[1], 3);
        addCard(columns[2], 180);
    }

    ~Scene()
    {
        controls::setLayoutVerify(wasVerifying);
        canvas.inputModifierKey(keys::Shift, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
    }

    const controls::LayoutStats &frame()
    {
        const size_t failures=controls::layoutVerifyFailures();
        canvas.updateCanvas();
        canvas.renderCanvas();
        CHECK(controls::layoutVerifyFailures() == failures);
        return canvas.getLayoutStats();
    }

    void pointAt(controls::Base *control)
    {
        const Point point=control->localPosToCanvas(Point(10, 10));
        canvas.inputMouseMoved(point.x, point.y, 0, 0);
        frame();
    }
};

}

TEST_CASE("Board places fixed-width columns at the viewport height in one pass")
{
    Scene scene;
    checkOnePass(scene.frame());
    CHECK(scene.frame().passes == 0);
    CHECK(scene.board->canScrollHorizontal());
    CHECK_FALSE(scene.board->canScrollVertical());
    CHECK(scene.board->getMaxVerticalScrollOffset() == 0);
    CHECK(scene.board->getInnerPanel()->getHeight() == scene.board->getViewportHeight());
    CHECK(scene.board->getMaxHorizontalScrollOffset()
        == 4*180+3*14-scene.board->getViewport().w);
    for(size_t i=0; i < scene.columns.size(); ++i)
    {
        auto *column=scene.columns[i];
        CHECK(column->getPlacement() == controls::Placement::Owner);
        CHECK(column->getX() == int(i)*(180+14));
        CHECK(column->getY() == 0);
        CHECK(column->getWidth() == 180);
        CHECK(column->getHeight() == scene.board->getViewportHeight());
        CHECK(column->getBody()->getHeight() == column->getHeight()-36);
        CHECK(column->getText() == "Column "+std::to_string(i));
    }
    REQUIRE(scene.longCard->getHeight() > scene.columns[0]->getBody()->getHeight());

    // A column's height request is not a request on the board's viewport.
    scene.columns[0]->setHeight(900);
    scene.canvas.setSize(700, 320);
    scene.board->setColumnWidth(210);
    scene.board->setGap(20);
    scene.columns[1]->setHeaderHeight(48);
    checkOnePass(scene.frame());
    CHECK(scene.frame().passes == 0);
    CHECK(scene.columns[0]->getHeight() == scene.board->getViewportHeight());
    CHECK(scene.columns[1]->getBody()->getHeight() == scene.board->getViewportHeight()-48);
    CHECK(scene.columns[1]->getX() == 230);
    CHECK(scene.columns[0]->getWidth() == 210);
    CHECK(scene.board->getInnerPanel()->getHeight() == scene.board->getViewportHeight());
    CHECK(scene.board->getMaxVerticalScrollOffset() == 0);
}

TEST_CASE("BoardColumn keeps its permanent gutter when fitting cards grow past the viewport")
{
    Scene scene;
    checkOnePass(scene.frame());
    auto *body=scene.columns[1]->getBody();
    auto *bar=verticalBar(body);
    REQUIRE(bar);
    REQUIRE_FALSE(bar->hidden());
    REQUIRE(bar->isDisabled());
    REQUIRE(body->getMaxVerticalScrollOffset() == 0);
    REQUIRE_FALSE(body->canScrollHorizontal());
    const int width=scene.shortCard->getWidth();
    const int viewportWidth=body->getViewport().w;

    auto *another=addCard(scene.columns[1], 3);
    checkOnePass(scene.frame());
    CHECK_FALSE(bar->hidden());
    CHECK(bar->isDisabled());
    CHECK(scene.shortCard->getWidth() == width);
    CHECK(another->getWidth() == width);
    CHECK(body->getViewport().w == viewportWidth);

    addCard(scene.columns[1], 200);
    checkOnePass(scene.frame());
    CHECK_FALSE(bar->hidden());
    CHECK_FALSE(bar->isDisabled());
    CHECK(body->getMaxVerticalScrollOffset() > 0);
    CHECK(scene.shortCard->getWidth() == width);
    CHECK(body->getViewport().w == viewportWidth);
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("BoardColumn consumes vertical wheel in its body at both ends and while fitting")
{
    Scene scene;
    scene.frame();
    auto *body=scene.columns[0]->getBody();
    auto *target=scene.longCard;
    int delta=-60;
    bool moves=true;
    SUBCASE("overflow scrolls only the body") {}
    SUBCASE("the bottom consumes without chaining")
    {
        body->scrollToBottom();
        moves=false;
    }
    SUBCASE("the top consumes without chaining")
    {
        delta=60;
        moves=false;
    }
    SUBCASE("a fitting body consumes without chaining")
    {
        body=scene.columns[1]->getBody();
        target=scene.shortCard;
        moves=false;
    }
    scene.frame();
    // Directly address the card, so an offscreen bottom card still tests the
    // actual-parent path used by a hit card.
    const int before=body->getVerticalScrollOffset();
    CHECK(target->onMouseWheeled(delta));
    checkNoPendingOutside(&scene.canvas, body);
    scene.canvas.updateCanvas();
    checkNoPendingOutside(&scene.canvas, body);
    scene.canvas.renderCanvas();
    CHECK(scene.board->getHorizontalScrollOffset() == 0);
    CHECK(scene.columns[2]->getBody()->getVerticalScrollOffset() == 0);
    if(moves)
        CHECK(body->getVerticalScrollOffset() > before);
    else
        CHECK(body->getVerticalScrollOffset() == before);
}

TEST_CASE("Board takes horizontal and Shift wheel through a column body")
{
    Scene scene;
    scene.frame();
    scene.pointAt(scene.longCard);
    REQUIRE(HoveredControl == scene.longCard);
    REQUIRE(scene.columns[0]->getBody()->getMaxVerticalScrollOffset() > 0);

    SUBCASE("horizontal wheel")
    {
        CHECK(scene.canvas.inputMouseWheelH(-60));
    }
    SUBCASE("Shift wheel from any input translator")
    {
        scene.canvas.inputModifierKey(keys::Shift, true);
        CHECK(scene.canvas.inputMouseWheel(-60));
    }
    checkNoPendingOutside(&scene.canvas, scene.board);
    checkOnePass(scene.frame());
    CHECK(scene.board->getHorizontalScrollOffset() > 0);
    CHECK(scene.columns[0]->getBody()->getVerticalScrollOffset() == 0);
    CHECK(scene.columns[1]->getBody()->getVerticalScrollOffset() == 0);
}

TEST_CASE("Board takes vertical wheel over headers gaps and its background")
{
    Scene scene;
    scene.frame();
    SUBCASE("header")
    {
        scene.pointAt(scene.columns[0]->getHeader());
        REQUIRE(HoveredControl == scene.columns[0]->getHeader());
    }
    SUBCASE("gap")
    {
        const Rect bounds=scene.board->getInnerPanel()->getCanvasBounds();
        const Point point(bounds.x+185, bounds.y+20);
        scene.canvas.inputMouseMoved(point.x, point.y, 0, 0);
        scene.frame();
        REQUIRE(HoveredControl == scene.board->getInnerPanel());
    }
    SUBCASE("background in the board's inset")
    {
        const Point point=scene.board->localPosToCanvas(Point(2, 20));
        scene.canvas.inputMouseMoved(point.x, point.y, 0, 0);
        scene.frame();
        REQUIRE(HoveredControl);
        REQUIRE(std::string(HoveredControl->getTypeName()) == "Board");
        REQUIRE(HoveredControl == scene.board);
    }
    CHECK(scene.canvas.inputMouseWheel(-60));
    checkNoPendingOutside(&scene.canvas, scene.board);
    checkOnePass(scene.frame());
    CHECK(scene.board->getHorizontalScrollOffset() > 0);
    CHECK(scene.columns[0]->getBody()->getVerticalScrollOffset() == 0);
}

TEST_CASE("Board and every body keep offsets when cards are added or text grows")
{
    Scene scene;
    scene.frame();
    scene.board->setHorizontalScrollOffset(80);
    scene.columns[0]->getBody()->setVerticalScrollOffset(40);
    scene.columns[2]->getBody()->setVerticalScrollOffset(30);
    scene.frame();
    const int width=scene.shortCard->getWidth();

    addCard(scene.columns[0], 20);
    addCard(scene.columns[1], 3);
    checkOnePass(scene.frame());
    CHECK(scene.board->getHorizontalScrollOffset() == 80);
    CHECK(scene.columns[0]->getBody()->getVerticalScrollOffset() == 40);
    CHECK(scene.columns[1]->getBody()->getVerticalScrollOffset() == 0);
    CHECK(scene.columns[2]->getBody()->getVerticalScrollOffset() == 30);
    CHECK(scene.columns[3]->getBody()->getVerticalScrollOffset() == 0);
    CHECK(scene.shortCard->getWidth() == width);

    const int oldRange=scene.columns[0]->getBody()->getMaxVerticalScrollOffset();
    scene.longCard->setText(paragraph(350));
    checkOnePass(scene.frame());
    CHECK(scene.columns[0]->getBody()->getMaxVerticalScrollOffset() > oldRange);
    CHECK(scene.board->getHorizontalScrollOffset() == 80);
    CHECK(scene.columns[0]->getBody()->getVerticalScrollOffset() == 40);
    CHECK(scene.columns[1]->getBody()->getVerticalScrollOffset() == 0);
    CHECK(scene.columns[2]->getBody()->getVerticalScrollOffset() == 30);
    CHECK(scene.columns[3]->getBody()->getVerticalScrollOffset() == 0);
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("Board repacks shown columns and releases owned placement on reparent")
{
    Scene scene;
    scene.frame();
    auto *column=scene.columns[1];
    column->hide();
    checkOnePass(scene.frame());
    CHECK(scene.columns[2]->getX() == 194);
    CHECK(scene.board->getMaxHorizontalScrollOffset()
        == 3*180+2*14-scene.board->getViewport().w);
    column->show();
    checkOnePass(scene.frame());
    CHECK(scene.columns[2]->getX() == 388);

    auto *other=scene.canvas.newChild<controls::Board>();
    other->setDock(DockPosition::None);
    other->setBounds(0, 0, 400, 120);
    column->setParent(other);
    checkOnePass(scene.frame());
    CHECK(column->getPlacement() == controls::Placement::Owner);
    CHECK(column->getWidth() == other->getColumnWidth());
    CHECK(column->getHeight() == other->getViewportHeight());
    CHECK(scene.columns[2]->getX() == 194);

    auto *ordinary=scene.canvas.newChild<controls::Base>();
    ordinary->setBounds(0, 0, 300, 120);
    column->setParent(ordinary);
    CHECK(column->getPlacement() == controls::Placement::Parent);
    CHECK(other->getChildren().empty());
    checkOnePass(scene.frame());
    CHECK(other->getMaxHorizontalScrollOffset() == 0);
}
