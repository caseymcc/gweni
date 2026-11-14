/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::CardReorder on real ScrollControls (vertical columns of toggle
 *  Buttons docked Top, the second one's first card holding a Label), driven
 *  through the input handler.
 *
 *  A press on a card, or on a part of it (the Label), moved 5 units starts the
 *  column's drag of that card, keyed by its control id: the pressed Button is
 *  let go without a click (not toggled, not depressed). A press moved less is
 *  a plain click. A press on the column's scroll bar starts no drag. The gap
 *  is the half-height rule over the cards' bounds and its bar is centred in
 *  the space between two cards. A drop within the column and one on another
 *  member of the group report the card's key and indexes, and gweni moves no
 *  card. A card the app adds above the gap during the drag keeps the gap
 *  before the card it was before, the bar following it, in one layout pass,
 *  and the drop reports the indexes as they are then. The dragged card
 *  deleted cancels the drag with nothing reported. Escape cancels, hiding the
 *  looks.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/button.h>
#include <gweni/controls/label.h>
#include <gweni/controls/reorderable.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/dragAndDrop.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;
using controls::CardReorder;
using controls::ItemDrop;
using controls::Reorderable;

namespace
{

constexpr int CardHeight=30;
constexpr int CardGap=4;

struct Column
{
    controls::ScrollControl *list=nullptr;
    CardReorder *reorder=nullptr;
    std::vector<controls::Button *> cards;
    std::vector<ItemDrop> drops;
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;
    Column todo;
    Column running;
    controls::ReorderGroup group;

    //! Two 150-wide columns docked left and right of a 400x300 canvas: Todo
    //! with \p todoCards cards, Running with 3, in one group.
    explicit Scene(int todoCards=4)
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        column(todo, DockPosition::Left, todoCards);
        column(running, DockPosition::Right, 3);
        group.add(todo.reorder);
        group.add(running.reorder);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        draganddrop::cancel();
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void column(Column &column, DockPosition dock, int cards)
    {
        column.list=canvas->newChild<controls::ScrollControl>();
        column.list->setDock(dock);
        column.list->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
        column.list->setWidth(150);
        column.list->setScroll(false, true);
        column.reorder=CardReorder::attach(column.list);
        column.reorder->setOnItemDropped([&column](const ItemDrop &drop) { column.drops.push_back(drop); });

        for(int i=0; i < cards; ++i)
            column.cards.push_back(card(column));
    }

    controls::Button *card(Column &column)
    {
        controls::Button *card=column.list->newChild<controls::Button>();
        card->setDock(DockPosition::Top);
        card->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
        card->setHeight(CardHeight);
        card->setMargin(Margin(0, 0, 0, CardGap));
        card->setIsToggle(true);
        return card;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void move(Point to)
    {
        canvas->inputMouseMoved(to.x, to.y, 0, 0);
        frame();
    }

    //! A canvas point \p y units down \p card, in its middle across
    static Point on(controls::Base *card, int y)
    {
        return card->localPosToCanvas(Point(card->getWidth()/2, y));
    }

    void press(Point at)
    {
        clock+=1.0f;
        move(at);
        input::onMouseClicked(canvas, 0, true);
        frame();
    }

    //! Presses \p card a quarter of the way down and moves 6 units right
    void startDrag(controls::Base *card)
    {
        const Point at=on(card, card->getHeight()/4);
        press(at);
        move(Point(at.x+6, at.y));
    }

    void release()
    {
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    //! Runs a frame for what was done since the last one and returns its
    //! counts: previousFrameStats() is taken as the next frame starts, so this
    //! runs one more (with nothing to do)
    controls::CanvasFrameStats inputFrame()
    {
        frame();
        frame();
        return canvas->previousFrameStats();
    }
};

}//namespace

TEST_CASE("CardReorder: a press on a card or a part of it starts its drag, and a short one is a click")
{
    SUBCASE("on the card, then on a Label inside one")
    {
        Scene scene;
        controls::Button *card=scene.todo.cards[1];

        scene.startDrag(card);
        REQUIRE(draganddrop::isDragging());
        CHECK(draganddrop::SourceControl == scene.todo.list);
        CHECK(Reorderable::dragSource() == scene.todo.reorder);
        CHECK(scene.todo.reorder->dragKey() == CardReorder::keyOf(card));
        CHECK(scene.todo.reorder->dragIndex() == 1);
        // the Button let go of its press, and is not clicked by the release
        CHECK_FALSE(card->isDepressed());
        scene.release();
        CHECK_FALSE(card->getToggleState());
        CHECK_FALSE(card->isDepressed());

        // a part of a card: the Label inside Running's first card
        controls::Label *label=scene.running.cards[0]->newChild<controls::Label>();
        label->setDock(DockPosition::Center);
        label->setText("part");
        scene.frame();
        REQUIRE(label->getHeight() > 0);
        scene.startDrag(label);
        REQUIRE(draganddrop::isDragging());
        CHECK(draganddrop::SourceControl == scene.running.list);
        CHECK(scene.running.reorder->dragKey() == CardReorder::keyOf(scene.running.cards[0]));
        scene.release();
    }

    SUBCASE("moved less than 5 units: a click")
    {
        Scene scene;
        controls::Button *card=scene.todo.cards[2];
        const Point at=Scene::on(card, 10);

        scene.press(at);
        scene.move(Point(at.x+2, at.y+1));
        CHECK_FALSE(draganddrop::isDragging());
        scene.release();
        CHECK(card->getToggleState());
    }
}

TEST_CASE("CardReorder: a press on the column's scroll bar starts no drag")
{
    Scene scene(12);
    controls::Base *bar=nullptr;

    for(controls::Base *child : scene.todo.list->getActualChildren())
    {
        if(child != scene.todo.list->getInnerPanel() && child->visible())
            bar=child;
    }
    REQUIRE(bar != nullptr);
    REQUIRE(bar->getHeight() > 20);

    const Point at=bar->localPosToCanvas(Point(bar->getWidth()/2, bar->getHeight()/2));
    scene.press(at);
    scene.move(Point(at.x, at.y+8));
    CHECK_FALSE(draganddrop::isDragging());
    scene.release();
}

TEST_CASE("CardReorder: the gap is the half-height rule over the cards, its bar between two cards")
{
    Scene scene;
    const std::vector<controls::Button *> &cards=scene.todo.cards;

    scene.startDrag(cards[0]);
    REQUIRE(draganddrop::isDragging());

    // card 2's upper half: before it; its lower half: after it
    scene.move(Scene::on(cards[2], CardHeight/2-1));
    CHECK(scene.todo.reorder->gap() == 2);

    controls::internal::DropIndicator *indicator=scene.todo.reorder->getIndicator();
    REQUIRE(indicator != nullptr);
    CHECK(indicator->visible());
    // centred in the 4-unit space between cards 1 and 2
    const int between=(cards[1]->getY()+cards[1]->getHeight()+cards[2]->getY())/2;
    CHECK(indicator->getY() == between-Reorderable::IndicatorThickness/2);
    CHECK(indicator->getHeight() == Reorderable::IndicatorThickness);
    CHECK(indicator->getWidth() == scene.todo.list->getInnerPanel()->getWidth());

    scene.move(Scene::on(cards[2], CardHeight/2+1));
    CHECK(scene.todo.reorder->gap() == 3);

    // the veil over the dragged card
    controls::internal::DragSourceVeil *veil=scene.todo.reorder->getSourceVeil();
    REQUIRE(veil != nullptr);
    CHECK(veil->visible());
    CHECK(veil->getBounds().y == cards[0]->getY());
    CHECK(veil->getBounds().h == CardHeight);
    scene.release();
}

TEST_CASE("CardReorder: drops within the column and on the other one report the card and its indexes")
{
    Scene scene;

    // card 0 below card 2: gap 3, index 2 once it has left
    scene.startDrag(scene.todo.cards[0]);
    scene.move(Scene::on(scene.todo.cards[2], CardHeight-2));
    scene.release();
    REQUIRE(scene.todo.drops.size() == 1);
    CHECK(scene.todo.drops[0].key == CardReorder::keyOf(scene.todo.cards[0]));
    CHECK(scene.todo.drops[0].sourceIndex == 0);
    CHECK(scene.todo.drops[0].targetIndex == 2);
    CHECK(scene.todo.reorder->cards() == std::vector<controls::Base *>(scene.todo.cards.begin(),
        scene.todo.cards.end()));

    // Todo's card 3 onto Running, before its card 1
    scene.startDrag(scene.todo.cards[3]);
    scene.move(Scene::on(scene.running.cards[1], 3));
    scene.release();
    REQUIRE(scene.running.drops.size() == 1);
    const ItemDrop &drop=scene.running.drops[0];
    CHECK(drop.source == scene.todo.reorder);
    CHECK(drop.target == scene.running.reorder);
    CHECK(drop.key == CardReorder::keyOf(scene.todo.cards[3]));
    CHECK(drop.sourceIndex == 3);
    CHECK(drop.targetIndex == 1);
    CHECK(scene.todo.drops.size() == 1);
}

TEST_CASE("CardReorder: a card added above the gap keeps it before the same card, in one pass")
{
    Scene scene;
    std::vector<controls::Button *> &cards=scene.todo.cards;

    // card 3 dragged, the gap before card 2
    scene.startDrag(cards[3]);
    scene.move(Scene::on(cards[2], 3));
    REQUIRE(scene.todo.reorder->gap() == 2);
    scene.frame();

    // the app adds a card at the top
    controls::Button *added=scene.card(scene.todo);
    added->sendToBack();
    const controls::CanvasFrameStats stats=scene.inputFrame();

    CHECK(draganddrop::isDragging());
    CHECK(scene.todo.reorder->gap() == 3);
    CHECK(scene.todo.reorder->dragIndex() == 4);
    CHECK(stats.layout.passes == 1);
    controls::internal::DropIndicator *indicator=scene.todo.reorder->getIndicator();
    REQUIRE(indicator != nullptr);
    CHECK(indicator->getY() == (cards[1]->getY()+cards[1]->getHeight()+cards[2]->getY())/2
        -Reorderable::IndicatorThickness/2);
    CHECK(scene.todo.reorder->getSourceVeil()->getY() == cards[3]->getY());

    scene.release();
    REQUIRE(scene.todo.drops.size() == 1);
    CHECK(scene.todo.drops[0].key == CardReorder::keyOf(cards[3]));
    CHECK(scene.todo.drops[0].sourceIndex == 4);
    CHECK(scene.todo.drops[0].targetIndex == 3);
}

TEST_CASE("CardReorder: the dragged card deleted cancels the drag, and Escape hides the looks")
{
    SUBCASE("deleted")
    {
        Scene scene;
        scene.startDrag(scene.todo.cards[1]);
        scene.move(Scene::on(scene.running.cards[0], 3));
        REQUIRE(draganddrop::isDragging());

        delete scene.todo.cards[1];
        scene.frame();
        CHECK_FALSE(draganddrop::isDragging());
        scene.release();
        CHECK(scene.running.drops.empty());
        CHECK(scene.todo.drops.empty());
    }

    SUBCASE("Escape")
    {
        Scene scene;
        scene.startDrag(scene.todo.cards[1]);
        scene.move(Scene::on(scene.todo.cards[3], 3));
        REQUIRE(scene.todo.reorder->getIndicator()->visible());

        input::onKeyEvent(scene.canvas, keys::Escape, true);
        input::onKeyEvent(scene.canvas, keys::Escape, false);
        scene.frame();
        CHECK_FALSE(draganddrop::isDragging());
        CHECK_FALSE(scene.todo.reorder->getIndicator()->visible());
        CHECK_FALSE(scene.todo.reorder->getSourceVeil()->visible());
        scene.release();
        CHECK(scene.todo.drops.empty());
        CHECK_FALSE(scene.todo.cards[1]->getToggleState());
    }
}
