/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SegmentedButtons on a real Canvas with the real (un-init()'d) textured
 *  skin on a StubRenderer, clicked through input::onMouseClicked and
 *  Canvas::inputMouseMoved.
 *
 *  Exclusive (the default): the first segment added is selected and exactly
 *  one is from then on, by click or setSelected(); deselecting it is refused,
 *  and a click on it keeps it, unless setAllowNone(true), which lets either
 *  clear it (turning it off again selects the first). Multi: each segment
 *  toggles on its own. onSelectionChange fires once per real change with the
 *  changed segment in info.control (the newly selected one when the exclusive
 *  selection moves, the cleared one when it is cleared), and never for a
 *  call that changes nothing; switching modes keeps the first selected
 *  segment. The segments are their content's width (wider with the check's
 *  room, whether selected or not) or, with setEqualWidths(true), the widest
 *  one's, all the tallest one's height, each one starting Joint units left of
 *  the previous one's right edge; the row is their width and height. A
 *  disabled row disables every segment and gives each its own state back.
 *  Removing a segment gives its ids back and moves the exclusive selection to
 *  the next enabled segment, else the previous one, as does a segment the app
 *  deletes itself.
 */
#include "headlessFixture.h"

#include <gweni/controls/segmentedButtons.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;
using controls::SegmentedButton;
using controls::SegmentedButtons;

namespace
{

size_t livePrimitives()
{
    return primitiveIdCount()-releasedPrimitiveIdCount();
}

size_t liveTexts()
{
    return textIdCount()-releasedTextIdCount();
}

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=nextClock();

    //! The input handler keeps the last click's time and place across cases,
    //! so each case's clock starts well after the last one's.
    static float nextClock()
    {
        static float start=0.0f;

        start+=1000.0f;
        return start;
    }

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    SegmentedButtons *row(std::initializer_list<const char *> texts)
    {
        SegmentedButtons *buttons=canvas->newChild<SegmentedButtons>();

        buttons->setPos(20, 20);
        for(const char *text : texts)
            buttons->addSegment(text);
        frame();
        return buttons;
    }

    void click(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        clock+=1.0f;
        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        input::onMouseClicked(canvas, 0, true);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }
};

//! Records onSelectionChange's segments.
struct Changes: public event::Handler
{
    std::vector<controls::Base *> segments;

    void changed(event::Info info)
    {
        segments.push_back(info.control);
    }
};

}//namespace

TEST_CASE("SegmentedButtons: an exclusive row has exactly one selected, by click or setSelected")
{
    Scene scene;
    SegmentedButtons *row=scene.row({"Day", "Week", "Month"});

    CHECK(row->getMode() == SegmentedButtons::Mode::Exclusive);
    CHECK(row->segmentCount() == 3);
    CHECK(row->getSelectedIndex() == 0);
    CHECK(row->getSelectedIndices() == std::vector<int>{0});

    scene.click(row->getSegment(2));
    CHECK(row->getSelectedIndex() == 2);
    CHECK(row->getSelectedIndices() == std::vector<int>{2});

    // a click on the selected one keeps it, and so does deselecting it
    scene.click(row->getSegment(2));
    CHECK(row->getSelectedIndex() == 2);
    row->setSelected(2, false);
    CHECK(row->getSelectedIndex() == 2);

    row->setSelected(1, true);
    CHECK(row->getSelectedIndices() == std::vector<int>{1});
    CHECK(row->getSegment(1)->isSelected());
    CHECK_FALSE(row->getSegment(2)->isSelected());

    // out of range: nothing
    row->setSelected(5, true);
    row->setSelected(-1, true);
    CHECK(row->getSelectedIndices() == std::vector<int>{1});
    CHECK(row->getSegment(5) == nullptr);
    CHECK_FALSE(row->isSelected(5));
}

TEST_CASE("SegmentedButtons: setAllowNone lets a click or setSelected clear the exclusive selection")
{
    Scene scene;
    SegmentedButtons *row=scene.row({"A", "B", "C"});

    row->setAllowNone(true);
    scene.click(row->getSegment(0));
    CHECK(row->getSelectedIndex() == -1);
    CHECK(row->getSelectedIndices().empty());

    scene.click(row->getSegment(1));
    CHECK(row->getSelectedIndex() == 1);
    scene.click(row->getSegment(2));
    CHECK(row->getSelectedIndices() == std::vector<int>{2});
    row->setSelected(2, false);
    CHECK(row->getSelectedIndex() == -1);

    // a segment added to an empty selection that allows none stays off
    row->addSegment("D");
    CHECK(row->getSelectedIndex() == -1);

    // not allowed again: the first is selected, and a click keeps it
    row->setAllowNone(false);
    CHECK(row->getSelectedIndex() == 0);
    scene.frame();
    scene.click(row->getSegment(0));
    CHECK(row->getSelectedIndex() == 0);
}

TEST_CASE("SegmentedButtons: a multi row toggles each segment on its own")
{
    Scene scene;
    SegmentedButtons *row=scene.row({"Bold", "Italic", "Underline"});

    // switching keeps the selection
    row->setMode(SegmentedButtons::Mode::Multi);
    CHECK(row->getSelectedIndices() == std::vector<int>{0});

    scene.click(row->getSegment(2));
    CHECK(row->getSelectedIndices() == std::vector<int>({0, 2}));
    scene.click(row->getSegment(0));
    CHECK(row->getSelectedIndices() == std::vector<int>{2});
    scene.click(row->getSegment(2));
    CHECK(row->getSelectedIndices().empty());
    CHECK(row->getSelectedIndex() == -1);

    row->setSelected(1, true);
    row->setSelected(2, true);
    CHECK(row->getSelectedIndices() == std::vector<int>({1, 2}));
    CHECK(row->getSelectedIndex() == 1);

    // back to exclusive: the first selected one stays
    row->setMode(SegmentedButtons::Mode::Exclusive);
    CHECK(row->getSelectedIndices() == std::vector<int>{1});

    // and with none selected, the first is
    row->setMode(SegmentedButtons::Mode::Multi);
    row->setSelected(1, false);
    CHECK(row->getSelectedIndices().empty());
    row->setMode(SegmentedButtons::Mode::Exclusive);
    CHECK(row->getSelectedIndices() == std::vector<int>{0});
}

TEST_CASE("SegmentedButtons: onSelectionChange fires once per real change, never for a no-op")
{
    Scene scene;
    SegmentedButtons *row=scene.row({"A", "B", "C"});
    Changes changes;

    row->onSelectionChange.add(&changes, &Changes::changed);

    // the selection moves: the new one, once
    scene.click(row->getSegment(1));
    REQUIRE(changes.segments.size() == 1);
    CHECK(changes.segments[0] == row->getSegment(1));

    // no-ops: a click on it, selecting it again, a refused deselect
    scene.click(row->getSegment(1));
    row->setSelected(1, true);
    row->setSelected(1, false);
    CHECK(changes.segments.size() == 1);

    // from the app
    row->setSelected(2, true);
    REQUIRE(changes.segments.size() == 2);
    CHECK(changes.segments[1] == row->getSegment(2));

    // cleared: the cleared one
    row->setAllowNone(true);
    CHECK(changes.segments.size() == 2);
    scene.click(row->getSegment(2));
    REQUIRE(changes.segments.size() == 3);
    CHECK(changes.segments[2] == row->getSegment(2));
    row->setSelected(2, false);
    CHECK(changes.segments.size() == 3);

    // multi: each toggle, once
    row->setMode(SegmentedButtons::Mode::Multi);
    CHECK(changes.segments.size() == 3);
    scene.click(row->getSegment(0));
    scene.click(row->getSegment(1));
    scene.click(row->getSegment(0));
    REQUIRE(changes.segments.size() == 6);
    CHECK(changes.segments[3] == row->getSegment(0));
    CHECK(changes.segments[4] == row->getSegment(1));
    CHECK(changes.segments[5] == row->getSegment(0));
    row->setSelected(1, true);
    row->setSelected(0, false);
    CHECK(changes.segments.size() == 6);

    // to exclusive with 1 selected: nothing changes, nothing fires
    row->setMode(SegmentedButtons::Mode::Exclusive);
    CHECK(changes.segments.size() == 6);
}

TEST_CASE("SegmentedButtons: segments are their content's width or the widest one's, sharing a border")
{
    Scene scene;
    SegmentedButtons *row=scene.row({"A", "Much longer", "Mid"});
    const int count=row->segmentCount();

    auto checkJoined=[&]() {
        for(int i=0; i < count; ++i)
        {
            const SegmentedButton *segment=row->getSegment(i);

            CHECK(segment->getY() == 0);
            CHECK(segment->getHeight() == row->getHeight());
            if(i == 0)
                CHECK(segment->getX() == 0);
            else
                CHECK(segment->getX() == row->getSegment(i-1)->right()-SegmentedButtons::Joint);
        }
        CHECK(row->getWidth() == row->getSegment(count-1)->right());
    };

    std::vector<int> natural;

    for(int i=0; i < count; ++i)
    {
        const SegmentedButton *segment=row->getSegment(i);

        natural.push_back(segment->getWidth());
        CHECK(segment->getWidth() == segment->getPreferredSize().width);
        CHECK(segment->getWidth() > 2*SegmentedButton::SidePadding);
    }
    CHECK(natural[1] > natural[2]);
    CHECK(natural[2] > natural[0]);
    checkJoined();

    row->setEqualWidths(true);
    scene.frame();
    for(int i=0; i < count; ++i)
        CHECK(row->getSegment(i)->getWidth() == natural[1]);
    checkJoined();

    row->setEqualWidths(false);
    row->setShowCheck(true);
    scene.frame();
    for(int i=0; i < count; ++i)
    {
        CHECK(row->getSegment(i)->getWidth()
            == natural[static_cast<size_t>(i)]+SegmentedButton::CheckSize+SegmentedButton::CheckGap);
        CHECK(row->getSegment(i)->getCheckRect().w == SegmentedButton::CheckSize);
    }
    checkJoined();

    // the static rule
    std::vector<int> x;

    CHECK(SegmentedButtons::segmentPositions({10, 20, 30}, x) == 58);
    CHECK(x == std::vector<int>({0, 9, 28}));
}

TEST_CASE("SegmentedButtons: the row is its tallest segment's height")
{
    Scene scene;
    SegmentedButtons *row=scene.row({"A", "B"});
    const int textRow=row->getHeight();

    CHECK(textRow == row->getSegment(0)->getPreferredSize().height);
    row->getSegment(1)->setFont(row->getSegment(1)->getFont().facename, 40, false);
    scene.frame();
    CHECK(row->getHeight() > textRow);
    CHECK(row->getSegment(0)->getHeight() == row->getHeight());
    CHECK(row->getSegment(1)->getHeight() == row->getHeight());
}

TEST_CASE("SegmentedButtons: a disabled row disables its segments and gives their own state back")
{
    Scene scene;
    SegmentedButtons *row=scene.row({"A", "B", "C"});

    row->getSegment(2)->setDisabled(true);
    row->setDisabled(true);
    for(int i=0; i < 3; ++i)
        CHECK(row->getSegment(i)->isDisabled());

    scene.click(row->getSegment(1));
    CHECK(row->getSelectedIndex() == 0);

    row->setDisabled(false);
    CHECK_FALSE(row->getSegment(0)->isDisabled());
    CHECK_FALSE(row->getSegment(1)->isDisabled());
    CHECK(row->getSegment(2)->isDisabled());
    scene.click(row->getSegment(2));
    CHECK(row->getSelectedIndex() == 0);
    scene.click(row->getSegment(1));
    CHECK(row->getSelectedIndex() == 1);
}

TEST_CASE("SegmentedButtons: removing a segment gives its ids back and moves the selection to a neighbour")
{
    Scene scene;
    const size_t primitives=livePrimitives();
    const size_t texts=liveTexts();
    SegmentedButtons *row=scene.row({"A", "B", "C", "D"});
    Changes changes;

    row->onSelectionChange.add(&changes, &Changes::changed);
    row->setSelected(1, true);
    scene.frame();

    const size_t rowPrimitives=livePrimitives();
    const size_t rowTexts=liveTexts();

    CHECK(rowPrimitives > primitives);
    CHECK(rowTexts > texts);

    // the selected one: the next is selected
    SegmentedButton *next=row->getSegment(2);

    changes.segments.clear();
    row->removeSegment(1);
    scene.frame();
    CHECK(row->segmentCount() == 3);
    CHECK(row->getSegment(1) == next);
    CHECK(row->getSelectedIndex() == 1);
    REQUIRE(changes.segments.size() == 1);
    CHECK(changes.segments[0] == next);
    CHECK(livePrimitives() < rowPrimitives);
    CHECK(liveTexts() < rowTexts);
    // the row is narrower and its segments still joined
    CHECK(row->getSegment(1)->getX() == row->getSegment(0)->right()-SegmentedButtons::Joint);

    // the last, selected, with the one before it disabled: the first enabled
    // one before it
    row->setSelected(2, true);
    row->getSegment(1)->setDisabled(true);
    row->removeSegment(2);
    CHECK(row->getSelectedIndex() == 0);

    // an unselected one: the selection stays
    row->removeSegment(1);
    CHECK(row->getSelectedIndex() == 0);
    CHECK(row->segmentCount() == 1);

    // deleted by the app: the row lets it go
    row->addSegment("E");
    row->setSelected(1, true);
    delete row->getSegment(1);
    CHECK(row->segmentCount() == 1);
    CHECK(row->getSelectedIndex() == 0);
    scene.frame();

    delete row;
    scene.frame();
    CHECK(livePrimitives() == primitives);
    CHECK(liveTexts() == texts);
}
