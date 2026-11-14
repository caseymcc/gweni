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
 *  a plain click. A press that a part of the card takes for a drag of its
 *  own (a Slider's thumb, a TextBox's selection) starts no card drag: the
 *  part keeps the mouse to its release. A press on the column's scroll bar
 *  starts no drag. The gap
 *  is the half-height rule over the cards' bounds and its bar is centred in
 *  the space between two cards. A drop within the column and one on another
 *  member of the group report the card's key and indexes, and gweni moves no
 *  card. A card the app adds above the gap during the drag keeps the gap
 *  before the card it was before, the bar following it, in one layout pass,
 *  and the drop reports the indexes as they are then. The dragged card
 *  deleted cancels the drag with nothing reported. Escape cancels, hiding the
 *  looks. A drop handler on the other column that takes reordering off the
 *  source column (setReorderable(nullptr)) or deletes it ends the drag, the
 *  drop reported once, with nothing read of the freed Reorderable (a real
 *  check in the ASAN tree).
 *  A drag sets the Normal cursor as it starts, whatever cursor the pressed
 *  control had set. A target column hidden during the drag stops being the
 *  target at the next update (the No cursor; a release drops nothing into
 *  it). A card the app adds in the same input phase as a move that changes
 *  the gap is seen: the dragged index and the veil follow it.
 *
 *  Auto-scroll, on the stepped canvas clock (1/60 s a frame): the cursor
 *  resting in the bottom band of an overflowing column's viewport scrolls it
 *  every frame after the first, by the band depth's share of the speed (a
 *  deeper cursor scrolls faster), the gap following the content under it, with
 *  one animation registered on the column; at the content's end it stops and a
 *  frame runs no animation; it stops too when the cursor leaves the band, when
 *  the drag ends and over a list that does not take the item; the top band
 *  scrolls back up, and at offset 0 does nothing.
 *  A refused gap first allowed by auto-scroll or a card insert shows its bar
 *  in that layout pass, with no second pass or render-phase layout, both on
 *  the first drag and when the indicator is reused from an earlier drag.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/anim.h>
#include <gweni/controls/button.h>
#include <gweni/controls/horizontalSlider.h>
#include <gweni/controls/label.h>
#include <gweni/controls/reorderable.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>
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

    //! A frame 1/60 s after the last on the canvas clock
    void step()
    {
        clock+=1.0f/60.0f;
        frame();
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

TEST_CASE("CardReorder: a press that a part of the card takes for its own drag starts no card drag")
{
    SUBCASE("a Slider's thumb")
    {
        Scene scene;
        controls::Button *card=scene.todo.cards[1];
        card->setHeight(60);
        auto *slider=card->newChild<controls::HorizontalSlider>();
        slider->setDock(DockPosition::Center);
        slider->setRange(0, 100);
        slider->setFloatValue(50);
        scene.frame();
        scene.frame();

        controls::internal::SliderBar *thumb=slider->getSliderBar();
        const Point at=thumb->localPosToCanvas(Point(thumb->getWidth()/2, thumb->getHeight()/2));
        scene.press(at);
        REQUIRE(thumb->isDepressed());
        scene.move(Point(at.x+10, at.y));
        CHECK_FALSE(draganddrop::isDragging());
        scene.move(Point(at.x+30, at.y));
        CHECK_FALSE(draganddrop::isDragging());
        CHECK(slider->getFloatValue() > 60.0f);
        scene.release();
        CHECK_FALSE(thumb->isDepressed());
        CHECK(scene.todo.drops.empty());
    }

    SUBCASE("a TextBox's selection")
    {
        Scene scene;
        controls::Button *card=scene.todo.cards[1];
        auto *box=card->newChild<controls::TextBox>();
        box->setDock(DockPosition::Center);
        box->setText("select me");
        scene.frame();
        scene.frame();

        const Point at=box->localPosToCanvas(Point(4, box->getHeight()/2));
        scene.press(at);
        scene.move(Point(at.x+40, at.y));
        CHECK_FALSE(draganddrop::isDragging());
        CHECK(MouseFocus == box);
        scene.release();
        CHECK(MouseFocus == nullptr);
        CHECK(scene.todo.drops.empty());
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
    CHECK(scene.todo.reorder->indicatorShown());
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
    CHECK(scene.todo.reorder->veilShown());
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

TEST_CASE("CardReorder: a drag sets the Normal cursor as it starts, over a cursor a control set")
{
    Scene scene;
    std::vector<int> asked;
    platform::setCursorHandler([&asked](unsigned char cursor) { asked.push_back(cursor); });

    // a card with a cursor of its own (a LabelClickable's Finger, a TextBox's
    // Beam), which the canvas sets as the mouse moves over it
    scene.todo.cards[1]->setCursor(cursor::Finger);
    const Point at=Scene::on(scene.todo.cards[1], CardHeight/4);
    scene.press(at);
    asked.clear();
    scene.move(Point(at.x+6, at.y));
    REQUIRE(draganddrop::isDragging());
    CHECK_FALSE(asked.empty());
    CHECK((asked.empty() ? -1 : asked.back()) == cursor::Normal);
    scene.release();
    platform::setCursorHandler(nullptr);
}

TEST_CASE("CardReorder: a target column hidden during the drag is the target no more")
{
    Scene scene;
    scene.startDrag(scene.todo.cards[1]);
    scene.move(Scene::on(scene.running.cards[0], 3));
    REQUIRE(Reorderable::dropTarget() == scene.running.reorder);

    scene.running.list->hide();
    scene.frame();
    CHECK(draganddrop::isDragging());
    CHECK(Reorderable::dropTarget() == nullptr);
    CHECK(draganddrop::currentCursor() == cursor::No);

    // a release with no move after the hide drops nothing into it
    scene.release();
    CHECK_FALSE(draganddrop::isDragging());
    CHECK(scene.running.drops.empty());
    CHECK(scene.todo.drops.empty());
}

TEST_CASE("CardReorder: a card added in the input phase of a gap-changing move is seen")
{
    Scene scene;
    std::vector<controls::Button *> &cards=scene.todo.cards;

    scene.startDrag(cards[3]);
    scene.move(Scene::on(cards[2], 3));
    REQUIRE(scene.todo.reorder->gap() == 2);
    REQUIRE(scene.todo.reorder->dragIndex() == 3);
    scene.frame();

    // the app adds a card at the top, and in the same input phase the mouse
    // moves to another gap (the indicator is placed again)
    controls::Button *added=scene.card(scene.todo);
    added->sendToBack();
    scene.canvas->inputMouseMoved(Scene::on(cards[0], 3).x, Scene::on(cards[0], 3).y, 0, 0);
    scene.frame();
    scene.frame();

    CHECK(draganddrop::isDragging());
    CHECK(scene.todo.reorder->dragIndex() == 4);
    CHECK(scene.todo.reorder->getSourceVeil()->getY() == cards[3]->getY());
}

TEST_CASE("CardReorder: a drop handler that takes reordering off the source column or deletes it ends the drag")
{
    SUBCASE("setReorderable(nullptr)")
    {
        Scene scene;
        scene.running.reorder->setOnItemDropped([&scene](const ItemDrop &drop) {
            scene.running.drops.push_back(drop);
            // the app takes reordering off the column the card left
            scene.todo.list->setReorderable(nullptr);
            scene.todo.reorder=nullptr;
        });
        scene.startDrag(scene.todo.cards[1]);
        scene.move(Scene::on(scene.running.cards[0], 3));
        REQUIRE(draganddrop::isDragging());

        scene.release();
        CHECK(scene.running.drops.size() == 1);
        CHECK_FALSE(draganddrop::isDragging());
        CHECK(scene.todo.list->getReorderable() == nullptr);
        scene.frame();
    }

    SUBCASE("the source column deleted")
    {
        Scene scene;
        scene.running.reorder->setOnItemDropped([&scene](const ItemDrop &drop) {
            scene.running.drops.push_back(drop);
            // the app deletes the column the card left
            delete scene.todo.list;
            scene.todo=Column();
        });
        scene.startDrag(scene.todo.cards[1]);
        scene.move(Scene::on(scene.running.cards[0], 3));
        REQUIRE(draganddrop::isDragging());

        scene.release();
        CHECK(scene.running.drops.size() == 1);
        CHECK_FALSE(draganddrop::isDragging());
        scene.frame();
    }
}

namespace
{

//! A canvas point \p fromBottom units above the bottom of \p list's viewport,
//! in the middle across
Point aboveViewportBottom(controls::ScrollControl *list, int fromBottom)
{
    const Rect &viewport=list->getViewport();
    return list->localPosToCanvas(Point(viewport.x+viewport.w/2, viewport.y+viewport.h-1-fromBottom));
}

}//namespace

TEST_CASE("CardReorder: a refused gap allowed by auto-scroll shows its indicator in one layout pass")
{
    Scene scene(20);
    controls::ScrollControl *list=scene.todo.list;
    CardReorder *reorder=scene.todo.reorder;
    size_t previousIndicatorId=0;

    SUBCASE("a fresh indicator") {}
    SUBCASE("an indicator hidden after an earlier drag")
    {
        scene.startDrag(scene.todo.cards[0]);
        REQUIRE(reorder->indicatorShown());
        previousIndicatorId=reorder->getIndicator()->getControlId();
        scene.release();
        REQUIRE(reorder->getIndicator()->hidden());
    }

    reorder->setCanDrop([](Reorderable *, uint64_t, Reorderable *, size_t index) {
        return index >= 10;
    });
    scene.startDrag(scene.todo.cards[0]);
    REQUIRE(draganddrop::isDragging());
    scene.move(aboveViewportBottom(list, 2));
    REQUIRE_FALSE(reorder->showsGap());
    REQUIRE_FALSE(reorder->indicatorShown());
    REQUIRE(draganddrop::currentCursor() == cursor::No);
    REQUIRE(anim::activeCount(list) == 1);

    bool allowed=false;
    for(int frame=0; frame < 40; ++frame)
    {
        scene.step();
        const controls::LayoutStats &stats=scene.canvas->getLayoutStats();
        INFO("frame ", frame, ", gap ", reorder->gap());
        CHECK(stats.passes <= 1);
        CHECK_FALSE(stats.renderLayout);
        CHECK(reorder->indicatorShown() == reorder->showsGap());
        allowed=allowed || reorder->showsGap();
    }
    CHECK(allowed);
    CHECK(draganddrop::isDragging());
    CHECK(draganddrop::currentCursor() == cursor::Normal);
    REQUIRE(reorder->getIndicator());
    if(previousIndicatorId)
        CHECK(reorder->getIndicator()->getControlId() == previousIndicatorId);
    scene.release();
    CHECK_FALSE(reorder->indicatorShown());
    CHECK(anim::activeCount(list) == 0);
}

TEST_CASE("CardReorder: a refused gap allowed by a card insert shows its indicator in one layout pass")
{
    Scene scene;
    CardReorder *reorder=scene.todo.reorder;

    SUBCASE("a fresh indicator") {}
    SUBCASE("an indicator hidden after an earlier drag")
    {
        scene.startDrag(scene.todo.cards[3]);
        REQUIRE(reorder->indicatorShown());
        scene.release();
        REQUIRE(reorder->getIndicator()->hidden());
    }

    reorder->setCanDrop([](Reorderable *, uint64_t, Reorderable *, size_t index) {
        return index == 1;
    });
    scene.startDrag(scene.todo.cards[3]);
    scene.move(Scene::on(scene.todo.cards[0], 3));
    REQUIRE_FALSE(reorder->showsGap());
    REQUIRE_FALSE(reorder->indicatorShown());
    REQUIRE(reorder->gap() == 0);

    // The edit is discovered by listArranged, after the panel placed its
    // cards. The gap stays before the same card, now at allowed index 1.
    scene.card(scene.todo)->sendToBack();
    scene.frame();
    const controls::LayoutStats &stats=scene.canvas->getLayoutStats();
    CHECK(stats.passes == 1);
    CHECK_FALSE(stats.renderLayout);
    CHECK(draganddrop::isDragging());
    CHECK(reorder->gap() == 1);
    CHECK(reorder->indicatorShown());
    CHECK(draganddrop::currentCursor() == cursor::Normal);
    scene.release();
    REQUIRE(scene.todo.drops.size() == 1);
    CHECK(scene.todo.drops[0].sourceIndex == 4);
    CHECK(scene.todo.drops[0].targetIndex == 1);
}

TEST_CASE("CardReorder: the cursor resting in the bottom band scrolls the column, faster deeper in")
{
    Scene scene(12);
    controls::ScrollControl *list=scene.todo.list;
    REQUIRE(list->getMaxVerticalScrollOffset() > 60);

    scene.startDrag(scene.todo.cards[0]);
    REQUIRE(draganddrop::isDragging());

    // 3 units into the 24-unit band from its bottom edge: depth 21
    scene.move(aboveViewportBottom(list, 2));
    CHECK(scene.todo.reorder->autoScrollDirection() == 1);
    CHECK(anim::activeCount(list) == 1);
    CHECK(scene.todo.reorder->autoScrollRate() == doctest::Approx(600.0f*22/24));

    std::vector<int> deep;
    for(int frame=0; frame < 4; ++frame)
    {
        const int before=list->getVerticalScrollOffset();
        scene.step();
        deep.push_back(list->getVerticalScrollOffset()-before);
    }
    // every frame scrolled, 550 units a second at 1/60 s a frame: 9 or 10
    for(int delta : deep)
        CHECK((delta == 9 || delta == 10));

    // the gap follows the content under the resting cursor: 6 frames scroll
    // more than a card (34 units)
    const size_t gap=scene.todo.reorder->gap();
    for(int frame=0; frame < 6; ++frame)
        scene.step();
    CHECK(scene.todo.reorder->gap() > gap);

    // shallower: 4 units into the band, 100 units a second, about 2 a frame
    scene.move(aboveViewportBottom(list, 20));
    CHECK(scene.todo.reorder->autoScrollRate() == doctest::Approx(600.0f*4/24));
    int shallow=0;
    for(int frame=0; frame < 6; ++frame)
    {
        const int before=list->getVerticalScrollOffset();
        scene.step();
        shallow+=list->getVerticalScrollOffset()-before;
    }
    CHECK(shallow >= 8);
    CHECK(shallow <= 12);

    scene.release();
}

TEST_CASE("CardReorder: the auto-scroll stops at the content's end, off the band and at the drag's end")
{
    SUBCASE("at the end, a frame runs no animation")
    {
        Scene scene(12);
        controls::ScrollControl *list=scene.todo.list;

        scene.startDrag(scene.todo.cards[0]);
        scene.move(aboveViewportBottom(list, 0));
        for(int frame=0; frame < 120 && anim::activeCount(list) > 0; ++frame)
            scene.step();

        CHECK(list->getVerticalScrollOffset() == list->getMaxVerticalScrollOffset());
        CHECK(anim::activeCount(list) == 0);
        CHECK(scene.todo.reorder->autoScrollDirection() == 0);
        scene.step();
        const controls::CanvasFrameStats stats=scene.inputFrame();
        CHECK(stats.update.animsRun == 0);
        // the last gap: past the last card
        CHECK(scene.todo.reorder->gap() == scene.todo.cards.size());
        CHECK(draganddrop::isDragging());
        scene.release();
    }

    SUBCASE("the cursor leaves the band, then the drag ends in it")
    {
        Scene scene(12);
        controls::ScrollControl *list=scene.todo.list;

        scene.startDrag(scene.todo.cards[0]);
        scene.move(aboveViewportBottom(list, 2));
        scene.step();
        scene.step();
        REQUIRE(anim::activeCount(list) == 1);

        scene.move(aboveViewportBottom(list, 60));
        CHECK(anim::activeCount(list) == 0);
        const int offset=list->getVerticalScrollOffset();
        scene.step();
        scene.step();
        CHECK(list->getVerticalScrollOffset() == offset);

        scene.move(aboveViewportBottom(list, 2));
        REQUIRE(anim::activeCount(list) == 1);
        input::onKeyEvent(scene.canvas, keys::Escape, true);
        input::onKeyEvent(scene.canvas, keys::Escape, false);
        CHECK(anim::activeCount(list) == 0);
        const int cancelled=list->getVerticalScrollOffset();
        scene.step();
        scene.step();
        CHECK(list->getVerticalScrollOffset() == cancelled);
        scene.release();
    }

    SUBCASE("over a column that does not take the item")
    {
        Scene scene(12);
        // Running's own: it is not reordered within, and Todo takes none of it
        scene.todo.reorder->setAcceptsFromGroup(false);
        controls::ScrollControl *list=scene.todo.list;

        scene.startDrag(scene.running.cards[0]);
        scene.move(aboveViewportBottom(list, 2));
        CHECK(Reorderable::dropTarget() == nullptr);
        CHECK(anim::activeCount(list) == 0);
        scene.step();
        CHECK(list->getVerticalScrollOffset() == 0);
        scene.release();
    }
}

TEST_CASE("CardReorder: the top band scrolls back up, and at the top does nothing")
{
    Scene scene(12);
    controls::ScrollControl *list=scene.todo.list;
    const Rect &viewport=list->getViewport();
    const Point top=list->localPosToCanvas(Point(viewport.x+viewport.w/2, viewport.y+2));

    scene.startDrag(scene.todo.cards[0]);
    scene.move(top);
    CHECK(anim::activeCount(list) == 0);
    CHECK(scene.todo.reorder->autoScrollDirection() == 0);

    // down a while, then up
    scene.move(aboveViewportBottom(list, 2));
    for(int frame=0; frame < 8; ++frame)
        scene.step();
    const int down=list->getVerticalScrollOffset();
    REQUIRE(down > 30);

    scene.move(top);
    CHECK(scene.todo.reorder->autoScrollDirection() == -1);
    scene.step();
    scene.step();
    CHECK(list->getVerticalScrollOffset() < down);
    for(int frame=0; frame < 60 && anim::activeCount(list) > 0; ++frame)
        scene.step();
    CHECK(list->getVerticalScrollOffset() == 0);
    CHECK(anim::activeCount(list) == 0);
    scene.release();
}
