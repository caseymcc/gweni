/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Sequencer's model, hit test and editing, on the headless
 *  fixture (a StubControl for every type, so no renderer), driven through the
 *  canvas's input. It pins: time and x through the view at two zooms (a
 *  zoom about a point keeps that point's time where it was, Ctrl+wheel zooms
 *  about the cursor, Shift+wheel and a middle drag pan, the plain wheel does
 *  not move time; disabled, neither moves time); the hit test (a key in front of a clip, the later of two
 *  overlapping clips and of two keys at one instant, a clip's edge grips, the
 *  ruler, nothing in a gap); a body drag snapping the pressed clip's edges to
 *  another track's clip edge, a key and the playhead within SnapDistance
 *  (the snap guide at the target), else its start to a whole frame, and Alt
 *  snapping to nothing; a key dragged snapping to a clip edge; a resize past
 *  the other edge keeping one frame; selection by click, Shift, Ctrl (an item
 *  toggled off not dragged), a plain press on a selected item keeping the
 *  group for a drag and its release selecting it alone, and a marquee
 *  (plain, Shift, Ctrl, Escape restoring, a click on empty space clearing,
 *  an item it shrinks off going back to its state at the press),
 *  onSelectionChanged once per change; Escape putting a drag back with no
 *  event; a removal during a drag (a vetoed Delete, the app's removeClip()
 *  or removeTrack() of a dragged item) putting the drag back with no event,
 *  and one of another item keeping it going on its items' moved indices; no
 *  track's search built on a drag frame, each dragged track's once after the
 *  release; a drag whose release never comes (hidden mid-drag, a lost release
  the next press finds) put back as Escape puts it; a press's
  onSelectionChanged handler removing the pressed item (no drag) or an earlier
  one (the pressed item dragged at its new index); a batch of the app's edits
  under a resting mouse building its track's search once, the hover hit-tested
  again in the next update; a pan or zoom mid-drag keeping the item under the
  cursor; a removal of another item mid-drag keeping the dragged item's hover;
  onItemsMoved once per release that moved something (none for a
 *  drag back to where it began), a whole group moving together; Delete
 *  through the veto; and the playhead's keys (Left and Right a frame, Home
 *  and End the range's ends, held to the range), a scrub in the ruler on
 *  whole frames, Escape giving it back, Space firing onPlayToggled.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/sequencer.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <cmath>
#include <functional>
#include <vector>

using namespace gweni;
using controls::Sequencer;
using controls::SequencerItem;

namespace
{

typedef SequencerItem::Kind Kind;

struct Scene: event::Handler
{
    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1400, 800);
        seq=canvas->newChild<Sequencer>();
        seq->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        seq->setBounds(10, 20, 1000, 300);
        seq->setFrameRate(10);
        seq->setTimeRange(0, 10);
        seq->onSelectionChanged.add(this, &Scene::selectionChanged);
        seq->onItemsMoved.add(this, &Scene::itemsMoved);
        seq->onPlayheadChanged.add(this, &Scene::playheadChanged);
        seq->onPlayToggled.add(this, &Scene::playToggled);
        seq->onRemoveRequested.add(this, &Scene::removeRequested);
        seq->onRemoved.add(this, &Scene::removed);
        for(int i=0; i < 4; ++i)
            seq->addTrack("Track "+std::to_string(i), 100+i);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        for(int key : {keys::Shift, keys::Control, keys::Alt})
            canvas->inputModifierKey(key, false);
        HoveredControl=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void selectionChanged(event::Info)
    {
        ++selectionCalls;
        if(onSelection)
            onSelection();
    }
    void itemsMoved(event::Info) { ++movedCalls; }
    void playheadChanged(event::Info) { ++playheadCalls; }
    void playToggled(event::Info) { ++toggleCalls; }
    void removeRequested(event::Info info)
    {
        requested.push_back(info.integer);
        if(info.integer == vetoId)
            seq->vetoRemove();
    }
    void removed(event::Info info) { removedIds.push_back(info.integer); }

    double perSecond() const
    {
        return seq->getBarArea().w/(seq->getViewEnd()-seq->getViewStart());
    }

    int laneMiddle(size_t track) const
    {
        Rect rect;

        REQUIRE(seq->getLaneRect(track, rect));
        return rect.y+rect.h/2;
    }

    //! The local point at time \p t (rounded to a unit) on \p track's middle row.
    Point at(size_t track, double t) const
    {
        return Point(int(std::lround(seq->xPosition(t))), laneMiddle(track));
    }

    Point canvasAt(const Point &local) const { return seq->localPosToCanvas(local); }

    void move(const Point &local)
    {
        const Point p=canvasAt(local);

        canvas->inputMouseMoved(p.x, p.y, 0, 0);
    }

    void press(const Point &local)
    {
        move(local);
        canvas->inputMouseButton(0, true);
    }

    void release() { canvas->inputMouseButton(0, false); }

    void key(int which)
    {
        canvas->inputModifierKey(which, true);
        canvas->inputModifierKey(which, false);
    }

    //! A drag from \p from by \p dx units along x, in steps, not let go.
    void drag(const Point &from, int dx)
    {
        press(from);
        move(Point(from.x+dx/2, from.y));
        move(Point(from.x+dx, from.y));
    }

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    Sequencer *seq=nullptr;
    int selectionCalls=0;
    int movedCalls=0;
    int playheadCalls=0;
    int toggleCalls=0;
    size_t vetoId=static_cast<size_t>(-1);
    std::vector<size_t> requested;
    std::vector<size_t> removedIds;
    std::function<void()> onSelection;
};

}//namespace

TEST_CASE("Sequencer: time and x through the view at two zooms, zoom about the cursor, pan")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    scene.frame();

    const Rect area=seq->getBarArea();

    REQUIRE(area.w > 500);
    CHECK(seq->xPosition(0) == doctest::Approx(area.x));
    CHECK(seq->xPosition(10) == doctest::Approx(area.x+area.w));
    CHECK(seq->xPosition(2.5) == doctest::Approx(area.x+area.w/4.0));
    CHECK(seq->timeAt(area.x+area.w/2.0) == doctest::Approx(5.0));

    // twice the zoom about 4: the view 2..6, 4 where it was
    const double x4=seq->xPosition(4);

    seq->zoomAt(2.5, 4);
    CHECK(seq->getViewStart() == doctest::Approx(2.4));
    CHECK(seq->getViewEnd() == doctest::Approx(6.4));
    CHECK(seq->xPosition(4) == doctest::Approx(x4));
    CHECK(seq->xPosition(2.4) == doctest::Approx(area.x));
    CHECK(seq->xPosition(6.4) == doctest::Approx(area.x+area.w));
    CHECK(seq->timeAt(area.x+area.w/4.0) == doctest::Approx(3.4));

    // no layout: the ticks follow the view at once
    REQUIRE(!seq->getTicks().empty());
    CHECK(seq->getTicks().front().value >= 2.4-1e-9);

    seq->setViewRange(0, 10);

    // Ctrl+wheel about the cursor: the time under it stays under it
    const Point cursor(area.x+area.w*3/10, scene.laneMiddle(1));
    const double before=seq->timeAt(cursor.x);

    scene.move(cursor);
    scene.canvas->inputModifierKey(keys::Control, true);
    CHECK(scene.canvas->inputMouseWheel(60));
    scene.canvas->inputModifierKey(keys::Control, false);
    CHECK(seq->getViewEnd()-seq->getViewStart() == doctest::Approx(10/Sequencer::WheelZoomStep));
    CHECK(seq->timeAt(cursor.x) == doctest::Approx(before));

    // the plain wheel scrolls the tracks, never time
    const double start=seq->getViewStart();

    scene.canvas->inputMouseWheel(-60);
    CHECK(seq->getViewStart() == start);

    // Shift+wheel (the canvas's horizontal wheel) pans
    const double span=seq->getViewEnd()-seq->getViewStart();

    scene.canvas->inputModifierKey(keys::Shift, true);
    scene.canvas->inputMouseWheel(-60);
    scene.canvas->inputModifierKey(keys::Shift, false);
    CHECK(seq->getViewStart() == doctest::Approx(start+Sequencer::WheelPanFraction*span));
    CHECK(seq->getViewEnd()-seq->getViewStart() == doctest::Approx(span));

    // a middle drag pans by the mouse's motion
    const double panned=seq->getViewStart();

    scene.move(cursor);
    scene.canvas->inputMouseButton(2, true);
    scene.move(Point(cursor.x-100, cursor.y));
    scene.canvas->inputMouseButton(2, false);
    CHECK(seq->getViewStart() == doctest::Approx(panned+100/scene.perSecond()));
    CHECK(scene.canvas->inputCapture() == nullptr);

    // disabled, neither Ctrl+wheel nor Shift+wheel moves time, and the
    // horizontal wheel is not taken
    const double viewStart=seq->getViewStart();
    const double viewEnd=seq->getViewEnd();

    seq->setDisabled(true);
    scene.move(cursor);
    scene.canvas->inputModifierKey(keys::Control, true);
    scene.canvas->inputMouseWheel(60);
    scene.canvas->inputModifierKey(keys::Control, false);
    scene.canvas->inputModifierKey(keys::Shift, true);
    scene.canvas->inputMouseWheel(-60);
    scene.canvas->inputModifierKey(keys::Shift, false);
    CHECK(!seq->onMouseWheeledH(-60));
    CHECK(seq->getViewStart() == viewStart);
    CHECK(seq->getViewEnd() == viewEnd);
}

TEST_CASE("Sequencer: the hit test finds keys before clips, the later overlap, the edge grips and the ruler")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 3, "", 1);
    seq->addClip(0, 2, 4, "", 2);   // over the first from 2 to 3
    seq->addClip(1, 5, 7, "", 3);
    seq->addKey(2, 6, 10);
    seq->addKey(2, 6, 11);          // at the same instant: the later is found
    seq->addKey(0, 2.5, 12);        // in front of both clips
    scene.frame();

    SequencerItem item;

    CHECK(seq->hitTest(scene.at(0, 1.5), item) == Sequencer::Part::ClipBody);
    CHECK(item == SequencerItem(Kind::Clip, 0, 0));
    CHECK(seq->hitTest(scene.at(0, 2.8), item) == Sequencer::Part::ClipBody);
    CHECK(item == SequencerItem(Kind::Clip, 0, 1));
    CHECK(seq->getOverlapLevel(0, 1) == 1);
    CHECK(seq->hitTest(scene.at(0, 2.5), item) == Sequencer::Part::Key);
    CHECK(item == SequencerItem(Kind::Key, 0, 0));
    CHECK(seq->hitTest(scene.at(2, 6), item) == Sequencer::Part::Key);
    CHECK(item == SequencerItem(Kind::Key, 2, 1));

    Rect rect;

    REQUIRE(seq->getClipRect(1, 0, rect));

    const int middle=scene.laneMiddle(1);

    CHECK(seq->hitTest(Point(rect.x, middle), item) == Sequencer::Part::ClipLeftEdge);
    CHECK(seq->hitTest(Point(rect.x+Sequencer::GripWidth-1, middle), item) == Sequencer::Part::ClipLeftEdge);
    CHECK(seq->hitTest(Point(rect.x+Sequencer::GripWidth, middle), item) == Sequencer::Part::ClipBody);
    CHECK(seq->hitTest(Point(rect.x+rect.w-1, middle), item) == Sequencer::Part::ClipRightEdge);
    CHECK(item == SequencerItem(Kind::Clip, 1, 0));
    CHECK(seq->hitTest(Point(rect.x-1, middle), item) == Sequencer::Part::None);
    CHECK(seq->hitTest(scene.at(1, 8), item) == Sequencer::Part::None);
    CHECK(seq->hitTest(scene.at(3, 5), item) == Sequencer::Part::None);
    CHECK(seq->hitTest(Point(rect.x+10, seq->getAxisRow().y+2), item) == Sequencer::Part::Ruler);
}

TEST_CASE("Sequencer: a drag snaps to a neighbour's edge, a key, the playhead, else a frame, and Alt snaps to nothing")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 2, "", 1);   // the dragged clip
    seq->addClip(1, 3, 4, "", 2);   // a neighbour on another track
    seq->addKey(1, 5.55, 3);
    seq->setPlayhead(7.25);
    scene.frame();

    const double pps=scene.perSecond();
    const Point grab=scene.at(0, 1.5);
    double guide=0.0;

    SUBCASE("its end to the neighbour's start, from 3 units short")
    {
        scene.drag(grab, int(std::lround(1.0*pps))-3);
        CHECK(seq->getClip(0, 0).end == 3.0);
        CHECK(seq->getClip(0, 0).start == 2.0);
        REQUIRE(seq->getSnapGuide(guide));
        CHECK(guide == 3.0);
    }
    SUBCASE("its start to a key, from 2 units past")
    {
        scene.drag(grab, int(std::lround(4.55*pps))+2);
        CHECK(seq->getClip(0, 0).start == 5.55);
        REQUIRE(seq->getSnapGuide(guide));
        CHECK(guide == 5.55);
    }
    SUBCASE("its start to the playhead")
    {
        scene.drag(grab, int(std::lround(6.25*pps))-4);
        CHECK(seq->getClip(0, 0).start == 7.25);
        REQUIRE(seq->getSnapGuide(guide));
        CHECK(guide == 7.25);
    }
    SUBCASE("nothing near: its start on a whole frame, no guide")
    {
        const int dx=int(std::lround(0.5*pps))+1;

        scene.drag(grab, dx);

        const double raw=1.0+(seq->timeAt(grab.x+dx)-seq->timeAt(grab.x));

        CHECK(raw != doctest::Approx(seq->snapToFrame(raw)));
        CHECK(seq->getClip(0, 0).start == seq->snapToFrame(raw));
        CHECK(seq->getClip(0, 0).end-seq->getClip(0, 0).start == doctest::Approx(1.0));
        CHECK(!seq->getSnapGuide(guide));
    }
    SUBCASE("Alt held: exactly the mouse's motion, near the neighbour too")
    {
        const int dx=int(std::lround(1.0*pps))-3;

        scene.canvas->inputModifierKey(keys::Alt, true);
        scene.drag(grab, dx);
        CHECK(seq->getClip(0, 0).start == doctest::Approx(1.0+(seq->timeAt(grab.x+dx)-seq->timeAt(grab.x))));
        CHECK(seq->getClip(0, 0).end != 3.0);
        CHECK(!seq->getSnapGuide(guide));
    }
    SUBCASE("setSnapping(false) too")
    {
        seq->setSnapping(false);
        scene.drag(grab, int(std::lround(1.0*pps))-3);
        CHECK(seq->getClip(0, 0).end != 3.0);
        CHECK(!seq->getSnapGuide(guide));
    }
    SUBCASE("a key dragged snaps to a clip's edge")
    {
        scene.drag(scene.at(1, 5.55), -int(std::lround(1.55*pps))+2);
        CHECK(seq->getKey(1, 0).time == 4.0);
        REQUIRE(seq->getSnapGuide(guide));
        CHECK(guide == 4.0);
    }
    scene.release();
    CHECK(!seq->getSnapGuide(guide));
}

TEST_CASE("Sequencer: a resize keeps one frame, and Escape puts a drag back with no event")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 3, "", 1);
    scene.frame();

    Rect rect;

    REQUIRE(seq->getClipRect(0, 0, rect));

    const int middle=scene.laneMiddle(0);
    const Point right(rect.x+rect.w-1, middle);

    SUBCASE("the right edge dragged past the left keeps a frame")
    {
        scene.drag(right, -int(std::lround(5*scene.perSecond())));
        CHECK(seq->getClip(0, 0).start == 1.0);
        CHECK(seq->getClip(0, 0).end == doctest::Approx(1.0+seq->frameLength()));
        scene.release();
        CHECK(scene.movedCalls == 1);
        CHECK(seq->getMovedItems() == std::vector<SequencerItem>{SequencerItem(Kind::Clip, 0, 0)});
    }
    SUBCASE("the left edge dragged past the right keeps a frame")
    {
        scene.drag(Point(rect.x, middle), int(std::lround(5*scene.perSecond())));
        CHECK(seq->getClip(0, 0).end == 3.0);
        CHECK(seq->getClip(0, 0).start == doctest::Approx(3.0-seq->frameLength()));
        scene.release();
    }
    SUBCASE("the right edge to 2 s, then Escape")
    {
        scene.drag(right, -int(std::lround(1*scene.perSecond())));
        CHECK(seq->getClip(0, 0).end == doctest::Approx(2.0));
        scene.key(keys::Escape);
        CHECK(seq->getClip(0, 0).start == 1.0);
        CHECK(seq->getClip(0, 0).end == 3.0);
        CHECK(!seq->dragging());
        // the rest of the drag does nothing
        scene.move(Point(right.x-200, middle));
        CHECK(seq->getClip(0, 0).end == 3.0);
        scene.release();
        CHECK(scene.movedCalls == 0);
    }
}

TEST_CASE("Sequencer: onItemsMoved once per release, a group moving together")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 2, "", 1);
    seq->addClip(2, 1.5, 2.5, "", 2);
    seq->addKey(1, 1.2, 3);
    scene.frame();

    // Shift adds: the three selected, one onSelectionChanged per press
    scene.press(scene.at(0, 1.5));
    scene.release();
    scene.canvas->inputModifierKey(keys::Shift, true);
    scene.press(scene.at(2, 2.0));
    scene.release();
    scene.press(scene.at(1, 1.2));
    scene.release();
    scene.canvas->inputModifierKey(keys::Shift, false);
    CHECK(scene.selectionCalls == 3);
    CHECK(seq->getSelection().size() == 3);
    CHECK(scene.movedCalls == 0);

    // a plain press on a selected item drags the group
    const int dx=int(std::lround(2.0*scene.perSecond()));

    scene.drag(scene.at(0, 1.5), dx);
    scene.move(Point(scene.at(0, 1.5).x+dx+1, scene.laneMiddle(0)));
    CHECK(scene.movedCalls == 0);
    scene.release();
    CHECK(scene.movedCalls == 1);
    CHECK(scene.selectionCalls == 3);
    CHECK(seq->getMovedItems().size() == 3);

    const double dt=seq->getClip(0, 0).start-1.0;

    CHECK(dt == doctest::Approx(2.0).epsilon(0.05));
    CHECK(seq->getClip(2, 0).start == doctest::Approx(1.5+dt));
    CHECK(seq->getKey(1, 0).time == doctest::Approx(1.2+dt));

    // a press and release with no drag moves nothing and selects it alone
    scene.press(scene.at(0, 1.5+dt));
    scene.release();
    CHECK(scene.movedCalls == 1);
    CHECK(scene.selectionCalls == 4);
    CHECK(seq->getSelection() == std::vector<SequencerItem>{SequencerItem(Kind::Clip, 0, 0)});

    // a drag that comes back to where it began moves nothing: no event
    scene.drag(scene.at(0, 1.5+dt), 60);
    scene.move(scene.at(0, 1.5+dt));
    scene.release();
    CHECK(scene.movedCalls == 1);
    CHECK(seq->getClip(0, 0).start == doctest::Approx(1.0+dt));

    // an app's edits fire nothing
    seq->setClip(0, 0, 0, 1);
    seq->clearSelection();
    seq->select(SequencerItem(Kind::Key, 1, 0));
    CHECK(scene.selectionCalls == 4);
    CHECK(scene.movedCalls == 1);
}

TEST_CASE("Sequencer: selection by click, Ctrl and a marquee, Escape restoring")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 2, "", 1);
    seq->addClip(1, 1, 2, "", 2);
    seq->addClip(2, 6, 7, "", 3);
    seq->addKey(3, 1.5, 4);
    scene.frame();

    const SequencerItem a(Kind::Clip, 0, 0);
    const SequencerItem b(Kind::Clip, 1, 0);
    const SequencerItem c(Kind::Clip, 2, 0);
    const SequencerItem k(Kind::Key, 3, 0);

    // a click selects the one item
    scene.press(scene.at(2, 6.5));
    scene.release();
    CHECK(seq->getSelection() == std::vector<SequencerItem>{c});
    CHECK(scene.selectionCalls == 1);

    // Ctrl toggles; toggled off it is not dragged
    scene.canvas->inputModifierKey(keys::Control, true);
    scene.press(scene.at(0, 1.5));
    scene.release();
    CHECK(seq->getSelection() == std::vector<SequencerItem>{a, c});
    scene.drag(scene.at(0, 1.5), 80);
    scene.release();
    scene.canvas->inputModifierKey(keys::Control, false);
    CHECK(seq->getSelection() == std::vector<SequencerItem>{c});
    CHECK(seq->getClip(0, 0).start == 1.0);
    CHECK(scene.selectionCalls == 3);
    CHECK(scene.movedCalls == 0);

    // a plain marquee from empty space over tracks 0..3 at 1..2 s
    const Point from(int(std::lround(seq->xPosition(0.5))), scene.laneMiddle(0)-6);
    const Point to(int(std::lround(seq->xPosition(2.5))), scene.laneMiddle(3)+2);

    scene.press(from);
    scene.move(Point(from.x+40, from.y+40));
    scene.move(to);
    CHECK(seq->marqueeActive());
    CHECK(seq->getSelection() == std::vector<SequencerItem>{a, b, k});
    // shrunk back over tracks 0 and 1, the key it left is unselected again,
    // and over 3 once more it is selected
    scene.move(Point(to.x, scene.laneMiddle(1)));
    CHECK(seq->getSelection() == std::vector<SequencerItem>{a, b});
    scene.move(to);
    CHECK(seq->getSelection() == std::vector<SequencerItem>{a, b, k});
    CHECK(scene.selectionCalls == 3);
    scene.release();
    CHECK(scene.selectionCalls == 4);
    CHECK(!seq->marqueeActive());

    // Escape gives the selection at the press back
    scene.press(from);
    scene.move(Point(to.x, scene.laneMiddle(1)));
    CHECK(seq->getSelection() == std::vector<SequencerItem>{a, b});
    scene.key(keys::Escape);
    CHECK(seq->getSelection() == std::vector<SequencerItem>{a, b, k});
    scene.release();
    CHECK(scene.selectionCalls == 4);

    // Shift adds to the selection at the press, Ctrl toggles it
    seq->clearSelection();
    seq->select(c);
    scene.canvas->inputModifierKey(keys::Shift, true);
    scene.press(from);
    scene.move(Point(to.x, scene.laneMiddle(0)+2));
    scene.release();
    scene.canvas->inputModifierKey(keys::Shift, false);
    CHECK(seq->getSelection() == std::vector<SequencerItem>{a, c});
    scene.canvas->inputModifierKey(keys::Control, true);
    scene.press(from);
    scene.move(Point(to.x, scene.laneMiddle(1)+2));
    CHECK(seq->getSelection() == std::vector<SequencerItem>{b, c});
    // back over track 0 alone, b is as at the press again
    scene.move(Point(to.x, scene.laneMiddle(0)+2));
    CHECK(seq->getSelection() == std::vector<SequencerItem>{c});
    scene.move(Point(to.x, scene.laneMiddle(1)+2));
    scene.release();
    scene.canvas->inputModifierKey(keys::Control, false);
    CHECK(seq->getSelection() == std::vector<SequencerItem>{b, c});

    // a click on empty lane space clears it
    const int calls=scene.selectionCalls;

    scene.press(scene.at(1, 9));
    scene.release();
    CHECK(seq->getSelection().empty());
    CHECK(scene.selectionCalls == calls+1);
}

TEST_CASE("Sequencer: Delete removes the selection through the veto")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 2, "", 1);
    seq->addClip(0, 3, 4, "", 2);
    seq->addKey(1, 1.5, 3);
    scene.frame();
    seq->select(SequencerItem(Kind::Clip, 0, 0));
    seq->select(SequencerItem(Kind::Clip, 0, 1));
    seq->select(SequencerItem(Kind::Key, 1, 0));
    scene.vetoId=2;

    // focused by a press in the ruler (which only scrubs)
    scene.press(Point(seq->getBarArea().x+5, seq->getAxisRow().y+2));
    scene.release();
    scene.key(keys::Delete);
    CHECK(scene.requested == std::vector<size_t>{3, 2, 1});
    CHECK(scene.removedIds == std::vector<size_t>{3, 1});
    REQUIRE(seq->getClipCount(0) == 1);
    CHECK(seq->getClip(0, 0).id == 2);
    CHECK(seq->getKeyCount(1) == 0);
    CHECK(seq->getTotalItemCount() == 1);
    CHECK(seq->getSelection() == std::vector<SequencerItem>{SequencerItem(Kind::Clip, 0, 0)});
    CHECK(scene.selectionCalls == 1);

    // all vetoed: nothing goes, nothing fires but the requests
    scene.vetoId=2;
    scene.key(keys::Delete);
    CHECK(seq->getClipCount(0) == 1);
    CHECK(scene.removedIds.size() == 2);
    CHECK(scene.selectionCalls == 1);
}

TEST_CASE("Sequencer: the playhead's keys, the scrub and Space")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    scene.frame();

    const Rect row=seq->getAxisRow();

    // a scrub in the ruler on whole frames, focusing the sequencer
    const Point ruler(int(std::lround(seq->xPosition(3.33))), row.y+2);

    scene.press(ruler);
    CHECK(seq->scrubbing());
    CHECK(seq->getPlayhead() == seq->snapToFrame(seq->timeAt(ruler.x)));
    scene.move(Point(int(std::lround(seq->xPosition(4.47))), row.y+2));
    CHECK(seq->getPlayhead() == doctest::Approx(4.5));
    scene.release();
    CHECK(scene.playheadCalls == 2);

    const double at=seq->getPlayhead();

    scene.key(keys::Right);
    CHECK(seq->getPlayhead() == doctest::Approx(at+0.1));
    scene.key(keys::Left);
    scene.key(keys::Left);
    CHECK(seq->getPlayhead() == doctest::Approx(at-0.1));
    scene.key(keys::End);
    CHECK(seq->getPlayhead() == 10.0);
    scene.key(keys::Right);
    CHECK(seq->getPlayhead() == 10.0);
    scene.key(keys::Home);
    CHECK(seq->getPlayhead() == 0.0);
    scene.key(keys::Left);
    CHECK(seq->getPlayhead() == 0.0);
    CHECK(scene.playheadCalls == 7);

    scene.key(keys::Space);
    CHECK(scene.toggleCalls == 1);

    // Escape mid-scrub gives the playhead back
    scene.press(ruler);
    CHECK(seq->getPlayhead() != 0.0);
    scene.key(keys::Escape);
    CHECK(seq->getPlayhead() == 0.0);
    scene.release();
}

// A removal while a drag lasts: one that takes a dragged item (a Delete with
// that item's removal vetoed, the app's removeClip() or removeTrack()) puts
// every dragged item back first and reports nothing, and one that takes any
// other item leaves the drag going on the same items, their indices moved,
// so the release reports what moved under its new index.
TEST_CASE("Sequencer: a removal during a drag puts the drag back or follows the edit")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 2, "", 1);
    seq->addClip(0, 6, 7, "", 2);
    seq->addClip(1, 3, 4, "", 3);
    seq->addKey(2, 8, 4);
    scene.frame();

    const int twoSeconds=int(std::lround(2*scene.perSecond()));

    SUBCASE("a Delete with the dragged clip's removal vetoed: the clip back, no move reported")
    {
        scene.vetoId=1;
        scene.drag(scene.at(0, 1.3), twoSeconds);
        REQUIRE(seq->getClip(0, 0).start == doctest::Approx(3.0));
        scene.key(keys::Delete);
        CHECK(scene.requested == std::vector<size_t>{1});
        CHECK(!seq->dragging());
        CHECK(seq->getClip(0, 0).id == 1);
        CHECK(seq->getClip(0, 0).start == 1.0);
        CHECK(seq->getClip(0, 0).end == 2.0);
        scene.move(Point(scene.at(0, 1.3).x+2*twoSeconds, scene.laneMiddle(0)));
        CHECK(seq->getClip(0, 0).start == 1.0);
        scene.release();
        CHECK(scene.movedCalls == 0);
    }
    SUBCASE("the app removes a clip of a group being dragged: both back, no move reported")
    {
        seq->select(SequencerItem(Kind::Clip, 1, 0));
        scene.canvas->inputModifierKey(keys::Shift, true);
        scene.drag(scene.at(0, 1.5), twoSeconds);
        scene.canvas->inputModifierKey(keys::Shift, false);
        REQUIRE(seq->getClip(1, 0).start == doctest::Approx(5.0));
        seq->removeClip(1, 0);
        CHECK(!seq->dragging());
        CHECK(seq->getClip(0, 0).start == 1.0);
        CHECK(seq->getClipCount(1) == 0);
        scene.release();
        CHECK(scene.movedCalls == 0);
    }
    SUBCASE("the app removes the dragged clip's track: no move reported")
    {
        scene.drag(scene.at(1, 3.5), twoSeconds);
        seq->removeTrack(1);
        CHECK(!seq->dragging());
        scene.release();
        CHECK(scene.movedCalls == 0);
        CHECK(seq->getSelection().empty());
    }
    SUBCASE("the app removes an earlier clip of the dragged one's track: the drag goes on")
    {
        scene.drag(scene.at(0, 6.5), -twoSeconds);
        REQUIRE(seq->getClip(0, 1).start == doctest::Approx(4.0));
        seq->removeClip(0, 0);
        CHECK(seq->dragging());
        CHECK(seq->getSelection() == std::vector<SequencerItem>{SequencerItem(Kind::Clip, 0, 0)});
        // the next move moves the clip that is dragged, now index 0
        scene.move(Point(scene.at(0, 6.5).x-2*twoSeconds, scene.laneMiddle(0)));
        CHECK(seq->getClip(0, 0).id == 2);
        CHECK(seq->getClip(0, 0).start == doctest::Approx(2.0));
        scene.release();
        CHECK(scene.movedCalls == 1);
        CHECK(seq->getMovedItems() == std::vector<SequencerItem>{SequencerItem(Kind::Clip, 0, 0)});
    }
    SUBCASE("the app removes a key and an earlier track: the drag goes on, its track moved up")
    {
        scene.drag(scene.at(1, 3.5), twoSeconds);
        seq->removeKey(2, 0);
        seq->removeTrack(0);
        CHECK(seq->dragging());
        CHECK(seq->getSelection() == std::vector<SequencerItem>{SequencerItem(Kind::Clip, 0, 0)});
        CHECK(seq->getClip(0, 0).id == 3);
        CHECK(seq->getClip(0, 0).start == doctest::Approx(5.0));
        scene.release();
        CHECK(scene.movedCalls == 1);
        CHECK(seq->getMovedItems() == std::vector<SequencerItem>{SequencerItem(Kind::Clip, 0, 0)});
    }
}

// The cost of a drag frame: no track's search is built while the drag lasts
// (here a group of 2001 clips over two tracks), only each dragged track's
// once on its release.
TEST_CASE("Sequencer: a drag frame builds no search, the release the dragged track's")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 2, "", 1);
    for(int i=0; i < 2000; ++i)
        seq->addClip(3, 20+i, 20.5+i, "", 100+i);
    for(int i=0; i < 2000; ++i)
        seq->select(SequencerItem(Kind::Clip, 3, size_t(i)));
    scene.frame();

    const Point from=scene.at(0, 1.5);
    SequencerItem item;

    // every track's search built (the hit test builds its track's)
    for(size_t track=0; track < 4; ++track)
        seq->hitTest(scene.at(track, 9.0), item);

    // a Ctrl press adds the clip to the selection and builds its track's
    // search (the hit test): from there on the drag builds none
    scene.canvas->inputModifierKey(keys::Control, true);
    scene.press(from);
    scene.canvas->inputModifierKey(keys::Control, false);

    const size_t builds=seq->getSearchBuildCount();

    for(int step=1; step <= 6; ++step)
    {
        scene.move(Point(from.x+step*7, from.y));
        scene.frame();
        CHECK(seq->getSearchBuildCount() == builds);
    }
    CHECK(seq->getClip(0, 0).start > 1.0);
    CHECK(seq->getClip(3, 0).start > 20.0);
    scene.release();
    scene.frame();
    // the release: the two dragged tracks' clip searches, once each, as
    // they are next searched
    for(size_t track=0; track < 4; ++track)
        seq->hitTest(scene.at(track, 9.0), item);
    CHECK(seq->getSearchBuildCount() == builds+2);
    CHECK(scene.movedCalls == 1);
}

// A drag whose release never comes: hidden mid-drag (the input code drops the
// mouse focus, so the release goes nowhere) or a release lost with the
// window's focus (the next press finds the drag still set). Either way the
// drag is put back as Escape puts it back, nothing reported, and the clip is
// found where it is again.
TEST_CASE("Sequencer: a drag whose release never comes is put back")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 2, "", 1);
    seq->addClip(1, 5, 6, "", 2);
    scene.frame();

    const int twoSeconds=int(std::lround(2*scene.perSecond()));
    SequencerItem item;

    SUBCASE("hidden mid-drag, shown, released, pressed elsewhere")
    {
        scene.drag(scene.at(0, 1.5), twoSeconds);
        REQUIRE(seq->getClip(0, 0).start == doctest::Approx(3.0));
        seq->hide();
        scene.frame();
        CHECK(!seq->dragging());
        CHECK(seq->getClip(0, 0).start == 1.0);
        CHECK(seq->getClip(0, 0).end == 2.0);
        seq->show();
        scene.frame();
        scene.release();
        scene.press(scene.at(2, 9.0));
        scene.release();
    }
    SUBCASE("the release lost: the next press puts it back")
    {
        scene.drag(scene.at(0, 1.5), twoSeconds);
        REQUIRE(seq->getClip(0, 0).start == doctest::Approx(3.0));
        scene.press(scene.at(2, 9.0));
        CHECK(seq->getClip(0, 0).start == 1.0);
        scene.release();
    }
    CHECK(scene.movedCalls == 0);
    CHECK(seq->hitTest(scene.at(0, 1.5), item) == Sequencer::Part::ClipBody);
    CHECK(item == SequencerItem(Kind::Clip, 0, 0));
    CHECK(seq->hitTest(scene.at(0, 3.5), item) == Sequencer::Part::None);
}

// An onSelectionChanged handler of a press's that edits the model: the press
// drags the item it pressed at its new index, and nothing if it went.
TEST_CASE("Sequencer: a press's selection handler removing an item")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 2, "", 1);
    seq->addClip(0, 4, 5, "", 2);
    seq->addClip(0, 7, 8, "", 3);
    scene.frame();

    const int twoSeconds=int(std::lround(2*scene.perSecond()));

    SUBCASE("the pressed clip removed: no drag, nothing else moved")
    {
        scene.onSelection=[&]() { scene.onSelection=nullptr; seq->removeClip(0, 1); };
        scene.drag(scene.at(0, 4.5), twoSeconds);
        CHECK(!seq->dragging());
        REQUIRE(seq->getClipCount(0) == 2);
        CHECK(seq->getClip(0, 0).start == 1.0);
        CHECK(seq->getClip(0, 1).id == 3);
        CHECK(seq->getClip(0, 1).start == 7.0);
        scene.release();
        CHECK(scene.movedCalls == 0);
    }
    SUBCASE("an earlier clip removed: the pressed one dragged at its new index")
    {
        scene.onSelection=[&]() { scene.onSelection=nullptr; seq->removeClip(0, 0); };
        scene.drag(scene.at(0, 4.5), twoSeconds);
        CHECK(seq->dragging());
        REQUIRE(seq->getClipCount(0) == 2);
        CHECK(seq->getClip(0, 0).id == 2);
        CHECK(seq->getClip(0, 0).start == doctest::Approx(6.0));
        CHECK(seq->getClip(0, 1).start == 7.0);
        scene.release();
        CHECK(scene.movedCalls == 1);
    }
}

// The hover is hit-tested again after the app's edits once, at the top of the
// next update phase: a Delete of many selected items, or many clips added,
// with the mouse resting on their track build its search once, not per item.
TEST_CASE("Sequencer: a batch of edits under a resting mouse builds the search once")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    for(int i=0; i < 1000; ++i)
        seq->addClip(0, 20+i, 20.5+i, "", 100+i);
    scene.frame();
    // focused by a press in the ruler, then the mouse resting on track 0
    scene.press(Point(seq->getBarArea().x+5, seq->getAxisRow().y+2));
    scene.release();
    scene.move(scene.at(0, 9.0));
    scene.frame();

    size_t builds=seq->getSearchBuildCount();

    SUBCASE("a Delete of 300 clips")
    {
        for(int i=0; i < 300; ++i)
            seq->select(SequencerItem(Kind::Clip, 0, size_t(2*i)));
        scene.key(keys::Delete);
        CHECK(seq->getClipCount(0) == 700);
    }
    SUBCASE("300 clips added")
    {
        for(int i=0; i < 300; ++i)
            seq->addClip(0, 2000+i, 2000.5+i, "", 5000+i);
    }
    CHECK(seq->getSearchBuildCount() == builds);
    scene.frame();
    CHECK(seq->getSearchBuildCount() <= builds+1);
    CHECK(!seq->isHovering());

    // the hover still follows an edit: a clip added under the mouse is
    // hovered in the next update
    seq->addClip(0, 8.5, 9.5, "", 9999);
    scene.frame();
    CHECK(seq->isHovering());
    CHECK(seq->getClip(0, seq->getHoverItem().index).id == 9999);
}

// A pan or zoom mid-drag: the item stays under the cursor (the drag goes from
// the time under the press, not from the press's x in the view now).
TEST_CASE("Sequencer: a pan or zoom mid-drag keeps the item under the cursor")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->setSnapping(false);
    seq->addClip(0, 1, 2, "", 1);
    scene.frame();

    const Point from=scene.at(0, 1.5);

    scene.drag(from, 20);
    REQUIRE(seq->dragging());

    const Point mouse(from.x+20, from.y);
    const double offset=seq->timeAt(mouse.x)-seq->getClip(0, 0).start;

    SUBCASE("panned 1 s")
    {
        seq->panBy(1.0);
    }
    SUBCASE("zoomed in twice about 5 s")
    {
        seq->zoomAt(2.0, 5.0);
    }
    CHECK(seq->timeAt(mouse.x)-seq->getClip(0, 0).start == doctest::Approx(offset));
    CHECK(seq->getClip(0, 0).end-seq->getClip(0, 0).start == doctest::Approx(1.0));
    scene.release();
    CHECK(scene.movedCalls == 1);
}

// A removal of another item mid-drag keeps the dragged item's hover look.
TEST_CASE("Sequencer: a removal mid-drag keeps the dragged item hovered")
{
    Scene scene;
    Sequencer *seq=scene.seq;

    seq->addClip(0, 1, 2, "", 1);
    seq->addClip(0, 6, 7, "", 2);
    seq->addClip(1, 3, 4, "", 3);
    scene.frame();

    scene.drag(scene.at(0, 6.5), 20);
    REQUIRE(seq->dragging());
    REQUIRE(seq->isHovering());
    seq->removeClip(1, 0);
    CHECK(seq->isHovering());
    CHECK(seq->getHoverItem() == SequencerItem(Kind::Clip, 0, 1));
    seq->removeClip(0, 0);
    scene.frame();
    CHECK(seq->dragging());
    CHECK(seq->isHovering());
    CHECK(seq->getHoverItem() == SequencerItem(Kind::Clip, 0, 0));
    scene.release();
}
