/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's Timeline renderer (skin::textured::Timeline) and the
 *  Timeline's hover and click, on a recording StubRenderer, a StubControl for
 *  every other type, the mouse through the input handler. The renderer: the
 *  frame at the timeline's z; an odd lane's stripe at z+1 across the caption
 *  column and the bar area, an even lane's slot hidden; a grid line per tick
 *  and the two axis lines at z+2; a slot per bar at getBarRect(), at z+3 plus
 *  its overlap level (an overlapping later bar a layer in front), in
 *  TimelineBar, its own colour (with TimelineBar's border) or a named surface
 *  (magenta for a name no theme has), clipped to the bar area; depthLayers()
 *  every layer up to the hover's, the first child numbered after them; the
 *  lists taken back to front (lanes, grid, bars lane by lane, the hover). A
 *  settled frame writes nothing; a changed bar writes its slot alone, an added
 *  one its own (a spare slot of its lane's list); a scroll by a lane writes
 *  only the stripes and bars of the lanes in view before or after it and no
 *  grid line, and a lane out of view has its bars' slots hidden. The hover:
 *  the bar under the mouse gets the hover look, one slot at z+4+MaxOverlapLevel
 *  over its rect, which a hover frame writes alone (no bar's slot), a move
 *  within the bar writes nothing, and leaving hides it; over overlapping bars
 *  it is the later one's; the tooltip is the lane's caption and the bar's
 *  times in the zone, the app's text or a control it fills, placed below the
 *  bar (getToolTipAnchor()). A click fires onBarClicked with the lane, the
 *  index and the id, and nothing for a press and release on two bars, on
 *  empty lane, or a bar replaced between them. A timeline built hidden takes
 *  no slot until shown; a hidden one hides its slots; deleting it gives every
 *  id back.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/text.h>
#include <gweni/controls/timeline.h>
#include <gweni/controls/toolTip.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

using namespace gweni;
using controls::Timeline;
using controls::TimelineBar;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

//! Records each slot's rect draw (its rect, z and fill) and the clip region
//! every draw was made in, besides StubRenderer's `lines`.
class Recorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top, Color bottom,
        Color borderColor, int zIndex) override
    {
        rects[primitiveId]=rect;
        zIndices[primitiveId]=zIndex;
        fills[primitiveId]=top;
        borders[primitiveId]=borderColor;
        clips[primitiveId]=clipRegion();
        hidden.erase(primitiveId);
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
    }

    void drawLine(size_t primitiveId, PointF p0, PointF p1, float thickness, int zIndex,
        const PointF *prev=nullptr, const PointF *next=nullptr) override
    {
        clips[primitiveId]=clipRegion();
        zIndices[primitiveId]=zIndex;
        hidden.erase(primitiveId);
        StubRenderer::drawLine(primitiveId, p0, p1, thickness, zIndex, prev, next);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.insert(primitiveId);
        StubRenderer::hidePrimitive(primitiveId);
    }

    std::map<size_t, Rect> rects;
    std::map<size_t, int> zIndices;
    std::map<size_t, Color> fills;
    std::map<size_t, Color> borders;
    std::map<size_t, Rect> clips;
    //! the slots hidden and not drawn since
    std::set<size_t> hidden;
};

class TimelineSkin: public test::StubSkin
{
public:
    explicit TimelineSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Timeline")
            return std::make_unique<skin::textured::Timeline>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene: public event::Handler
{
    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        timeline=canvas->newChild<Timeline>();
        timeline->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        timeline->setBounds(10, 20, 480, 240);
        timeline->setUtcOffset(controls::axis::fixedUtcOffset(0));
        timeline->setXRange(0.0, 100.0);
        timeline->onBarClicked.add(this, &Scene::clicked);
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
        renderer.lines.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    const skin::textured::Timeline &skinTimeline() const
    {
        return *static_cast<const skin::textured::Timeline *>(timeline->getSkinControl());
    }

    size_t frameId() const
    {
        std::vector<size_t> ids;
        std::vector<size_t> texts;

        skinTimeline().appendIds(ids, texts);
        return ids.front();
    }

    std::set<size_t> written() const
    {
        const std::vector<size_t> &ids=renderer.writtenPrimitiveIds();

        return std::set<size_t>(ids.begin(), ids.end());
    }

    //! The middle of bar \p index of \p lane, in the timeline's coordinates.
    Point barMiddle(size_t lane, size_t index) const
    {
        Rect rect;

        REQUIRE(timeline->getBarRect(lane, index, rect));
        return Point(rect.x+rect.w/2, rect.y+rect.h/2);
    }

    void moveTo(const Point &local)
    {
        const Point at=timeline->localPosToCanvas(local);

        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    void click(const Point &local)
    {
        clock+=1.0f;
        moveTo(local);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    //! \p rect, in the timeline's coordinates, as the renderer is given it:
    //! at its render bounds.
    Rect drawn(Rect rect) const
    {
        rect.x+=timeline->getRenderBounds().x;
        rect.y+=timeline->getRenderBounds().y;
        return rect;
    }

    //! The bar area in canvas coordinates: the bars' clip.
    Rect areaClip() const
    {
        const Rect area=timeline->getBarArea();

        return Rect(timeline->getX()+area.x, timeline->getY()+area.y, area.w, area.h);
    }

    void clicked(event::Info info)
    {
        clicks.push_back({size_t(info.point.x), size_t(info.point.y), uint64_t(info.integer)});
    }

    struct Click
    {
        size_t lane;
        size_t index;
        uint64_t id;
        bool operator==(const Click &other) const
        {
            return lane == other.lane && index == other.index && id == other.id;
        }
    };

    Recorder renderer;
    TimelineSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    Timeline *timeline=nullptr;
    float clock=10.0f;
    std::vector<Click> clicks;
};

}//namespace

TEST_CASE("Timeline renderer: the frame, the lanes, the grid and the bars, each at its z and clipped")
{
    Scene scene;
    Timeline *timeline=scene.timeline;

    for(int i=0; i < 4; ++i)
        timeline->addLane("lane");
    timeline->addBar(0, TimelineBar(10, 30, 1));
    timeline->addBar(0, TimelineBar(40, 60, 2).withColor(Color(10, 20, 30)));
    timeline->addBar(1, TimelineBar(10, 50, 3).withSurface("ChartSeries1"));
    timeline->addBar(1, TimelineBar(20, 30, 4));
    timeline->addBar(2, TimelineBar(5, 15, 5).withSurface("NoSuchSurface"));
    scene.frame();

    const skin::textured::Timeline &drawn=scene.skinTimeline();
    const int z=timeline->getZIndex();
    const Rect area=timeline->getBarArea();
    const Rect areaClip=scene.areaClip();

    CHECK(scene.renderer.zIndices[scene.frameId()] == z);

    // the stripes: odd lanes, across the caption column and the bars
    const std::vector<size_t> lanes=drawn.lanePrimitives();

    REQUIRE(lanes.size() == 4);
    for(size_t i=0; i < 4; ++i)
    {
        Rect laneRect;

        REQUIRE(timeline->getLaneRect(i, laneRect));
        if(i%2 == 0)
        {
            CHECK(scene.renderer.hidden.count(lanes[i]) == 1);
            continue;
        }
        CHECK((scene.renderer.rects[lanes[i]] == scene.drawn(Rect(timeline->getCaptionColumnBounds().x,
            laneRect.y, area.x+area.w-timeline->getCaptionColumnBounds().x, laneRect.h))));
        CHECK(scene.renderer.zIndices[lanes[i]] == z+skin::textured::Timeline::LaneLayer);
        CHECK((scene.renderer.fills[lanes[i]] == tokyonight::TimelineLane.top));
    }

    // the grid: a line per tick and the two axis lines
    const std::vector<size_t> grid=drawn.gridPrimitives();

    REQUIRE(grid.size() == timeline->getTicks().size()+2);
    CHECK(scene.renderer.lines.size() == grid.size());
    for(size_t id : grid)
        CHECK(scene.renderer.zIndices[id] == z+skin::textured::Timeline::GridLayer);
    CHECK((scene.renderer.clips[grid.front()] == areaClip));

    // the bars: a slot each at its rect, its level's layer, in its surface
    auto barSlot=[&](size_t lane, size_t index) { return drawn.barPrimitives(lane).at(index); };
    const int barZ=z+skin::textured::Timeline::BarLayer;

    for(size_t lane=0; lane < 3; ++lane)
    {
        REQUIRE(drawn.barPrimitives(lane).size() == timeline->getBarCount(lane));
        for(size_t i=0; i < timeline->getBarCount(lane); ++i)
        {
            Rect rect;
            const size_t id=barSlot(lane, i);

            REQUIRE(timeline->getBarRect(lane, i, rect));
            CHECK((scene.renderer.rects[id] == scene.drawn(rect)));
            CHECK(scene.renderer.zIndices[id] == barZ+timeline->getOverlapLevel(lane, i));
            CHECK((scene.renderer.clips[id] == areaClip));
        }
    }
    CHECK(drawn.barPrimitives(3).empty());
    CHECK((scene.renderer.fills[barSlot(0, 0)] == tokyonight::TimelineBar.top));
    CHECK((scene.renderer.fills[barSlot(0, 1)] == Color(10, 20, 30)));
    CHECK((scene.renderer.borders[barSlot(0, 1)] == tokyonight::TimelineBar.border));
    CHECK((scene.renderer.fills[barSlot(1, 0)] == tokyonight::ChartSeries1.top));
    CHECK((scene.renderer.fills[barSlot(2, 0)] == colors::Missing));
    // the later of two overlapping bars a layer in front
    CHECK(scene.renderer.zIndices[barSlot(1, 1)] == barZ+1);

    // the hover's slot, hidden with no hover, in front of every bar
    const std::vector<size_t> hover=drawn.hoverPrimitives();

    REQUIRE(hover.size() == 1);
    CHECK(scene.renderer.hidden.count(hover[0]) == 1);

    // taken back to front: the lanes, the grid, the bars lane by lane, the
    // hover, every one after the frame
    std::vector<size_t> order(lanes.begin(), lanes.end());

    order.insert(order.end(), grid.begin(), grid.end());
    for(size_t lane=0; lane < 4; ++lane)
    {
        const std::vector<size_t> bars=drawn.barPrimitives(lane);

        order.insert(order.end(), bars.begin(), bars.end());
    }
    order.push_back(hover[0]);
    CHECK(std::is_sorted(order.begin(), order.end()));
    CHECK(order.front() > scene.frameId());

    // every layer is the timeline's: its first child is numbered after them
    CHECK(timeline->getChildren().front()->getZIndex() >= z+int(const_cast<skin::textured::Timeline &>(drawn).depthLayers()));
    CHECK(int(const_cast<skin::textured::Timeline &>(drawn).depthLayers()) == skin::textured::Timeline::HoverLayer+1);
}

TEST_CASE("Timeline renderer: a settled frame writes nothing, a changed or added bar its slot alone")
{
    Scene scene;
    Timeline *timeline=scene.timeline;

    for(int i=0; i < 3; ++i)
    {
        timeline->addLane("lane");
        timeline->addBar(size_t(i), TimelineBar(10, 20, uint64_t(i)));
        timeline->addBar(size_t(i), TimelineBar(30, 40, uint64_t(10+i)));
    }
    scene.frame();
    scene.frame();
    CHECK(scene.written().empty());

    const skin::textured::Timeline &drawn=scene.skinTimeline();

    // a bar moved: its slot alone, at its new rect
    timeline->setBar(1, 0, TimelineBar(12, 25, 1));
    scene.frame();
    CHECK(scene.written() == std::set<size_t>{drawn.barPrimitives(1)[0]});
    Rect rect;
    REQUIRE(timeline->getBarRect(1, 0, rect));
    CHECK((scene.renderer.rects[drawn.barPrimitives(1)[0]] == scene.drawn(rect)));

    // the same bar given again: nothing
    timeline->setBar(1, 0, TimelineBar(12, 25, 1));
    scene.frame();
    CHECK(scene.written().empty());

    // an added bar: a spare slot of its lane's list
    const std::vector<size_t> before=drawn.barPrimitives(2);

    timeline->addBar(2, TimelineBar(50, 60, 99));
    scene.frame();
    const std::vector<size_t> after=drawn.barPrimitives(2);
    REQUIRE(after.size() == before.size()+1);
    CHECK(std::equal(before.begin(), before.end(), after.begin()));
    CHECK(scene.written() == std::set<size_t>{after.back()});
    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("Timeline renderer: a scroll writes the lanes in view alone, and hides the bars out of it")
{
    Scene scene;
    Timeline *timeline=scene.timeline;

    for(int i=0; i < 40; ++i)
    {
        timeline->addLane("lane");
        timeline->addBar(size_t(i), TimelineBar(10, 20, uint64_t(i)));
        timeline->addBar(size_t(i), TimelineBar(30, 40, uint64_t(100+i)));
    }
    scene.frame();
    scene.frame();

    const skin::textured::Timeline &drawn=scene.skinTimeline();
    const int height=timeline->getLaneHeight();
    auto inView=[&]()
    {
        std::set<size_t> lanes;

        for(size_t i=0; i < timeline->getLaneCount(); ++i)
        {
            Rect rect;

            if(timeline->getLaneRect(i, rect))
                lanes.insert(i);
        }
        return lanes;
    };
    const std::set<size_t> shownBefore=inView();

    // a lane out of view has its bars hidden
    REQUIRE(!shownBefore.count(39));
    for(size_t id : drawn.barPrimitives(39))
        CHECK(scene.renderer.hidden.count(id) == 1);

    timeline->setScrollOffset(height);
    scene.frame();
    REQUIRE(timeline->getScrollOffset() == height);

    std::set<size_t> lanes=shownBefore;
    const std::set<size_t> shownAfter=inView();

    lanes.insert(shownAfter.begin(), shownAfter.end());

    // only the stripes and bars of the lanes in view before or after
    std::set<size_t> allowed;

    for(size_t lane : lanes)
    {
        allowed.insert(drawn.lanePrimitives()[lane]);
        for(size_t id : drawn.barPrimitives(lane))
            allowed.insert(id);
    }

    const std::set<size_t> written=scene.written();

    CHECK(!written.empty());
    for(size_t id : written)
        CHECK(allowed.count(id) == 1);
    for(size_t id : drawn.gridPrimitives())
        CHECK(written.count(id) == 0);
    // lane 0 left the view: its bars hidden
    REQUIRE(!shownAfter.count(0));
    for(size_t id : drawn.barPrimitives(0))
        CHECK(scene.renderer.hidden.count(id) == 1);
    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("Timeline hover: the hover look's slot alone, the tooltip, and nothing for a move within a bar")
{
    Scene scene;
    Timeline *timeline=scene.timeline;

    timeline->addLane("host-01");
    timeline->addLane("host-02");
    timeline->setXRange(0.0, 7200.0);
    timeline->addBar(0, TimelineBar(600, 1800, 7));
    timeline->addBar(0, TimelineBar(3600, 5400, 8));
    timeline->addBar(1, TimelineBar(0, 7200, 9));
    // overlapping in lane 1: the later one
    timeline->addBar(1, TimelineBar(2000, 3000, 10));
    scene.frame();
    scene.frame();

    const skin::textured::Timeline &drawn=scene.skinTimeline();
    const size_t hover=drawn.hoverPrimitives().at(0);
    const int z=timeline->getZIndex();

    CHECK(!timeline->isHovering());
    CHECK(!timeline->hasToolTip());

    scene.moveTo(scene.barMiddle(0, 0));
    REQUIRE(timeline->isHovering());
    CHECK(timeline->getHoverLane() == 0);
    CHECK(timeline->getHoverIndex() == 0);
    CHECK(scene.written() == std::set<size_t>{hover});
    Rect bar;
    REQUIRE(timeline->getBarRect(0, 0, bar));
    CHECK((scene.renderer.rects[hover] == scene.drawn(bar)));
    CHECK(scene.renderer.zIndices[hover] == z+skin::textured::Timeline::HoverLayer);
    CHECK((scene.renderer.borders[hover] == tokyonight::TimelineBarHover.border));
    CHECK((scene.renderer.clips[hover] == scene.areaClip()));
    CHECK(timeline->getToolTipText() == "host-01  Jan 1 00:10 - Jan 1 00:30");

    // below the bar
    const Rect anchor=timeline->getToolTipAnchor();
    const Point corner=timeline->localPosToCanvas(Point(bar.x, bar.y));
    CHECK((anchor == Rect(corner.x, corner.y, bar.w, bar.h)));

    // within the bar: nothing
    scene.moveTo(Point(bar.x+1, bar.y+1));
    CHECK(scene.written().empty());
    CHECK(timeline->getHoverIndex() == 0);

    // the next bar: the hover's slot alone
    scene.moveTo(scene.barMiddle(0, 1));
    CHECK(scene.written() == std::set<size_t>{hover});
    CHECK(timeline->getHoverIndex() == 1);
    CHECK(timeline->getToolTipText() == "host-01  Jan 1 01:00 - Jan 1 01:30");

    // overlapping bars: the later one, the one in front
    scene.moveTo(scene.barMiddle(1, 1));
    CHECK(timeline->getHoverLane() == 1);
    CHECK(timeline->getHoverIndex() == 1);
    REQUIRE(timeline->getBarRect(1, 1, bar));
    CHECK((scene.renderer.rects[hover] == scene.drawn(bar)));
    // the earlier one where the later does not reach
    Rect earlier;
    REQUIRE(timeline->getBarRect(1, 0, earlier));
    scene.moveTo(Point(earlier.x+2, earlier.y+2));
    CHECK(timeline->getHoverIndex() == 0);

    // a gap: no hover, the slot hidden, no tooltip
    Rect first;
    Rect second;
    REQUIRE(timeline->getBarRect(0, 0, first));
    REQUIRE(timeline->getBarRect(0, 1, second));
    scene.moveTo(Point((first.x+first.w+second.x)/2, first.y+2));
    CHECK(!timeline->isHovering());
    CHECK(scene.written() == std::set<size_t>{hover});
    CHECK(scene.renderer.hidden.count(hover) == 1);
    CHECK(!timeline->hasToolTip());

    // a bar changed under a resting mouse: the tooltip follows
    scene.moveTo(scene.barMiddle(0, 0));
    timeline->setBar(0, 0, TimelineBar(0, 1800, 7));
    scene.frame();
    CHECK(timeline->getToolTipText() == "host-01  Jan 1 - Jan 1 00:30");
}

TEST_CASE("Timeline hover: the app's text or control fills the tooltip for each bar")
{
    Scene scene;
    Timeline *timeline=scene.timeline;

    timeline->addLane("a");
    timeline->addBar(0, TimelineBar(10, 20, 41));
    timeline->addBar(0, TimelineBar(40, 50, 42));

    std::vector<size_t> asked;

    timeline->setHoverText([&asked](const Timeline &t, size_t lane, size_t index)
        {
            asked.push_back(index);
            return String("task ")+std::to_string(t.getBar(lane, index).id);
        });
    scene.frame();

    scene.moveTo(scene.barMiddle(0, 0));
    REQUIRE(asked.size() == 1);
    CHECK(timeline->getToolTipText() == "task 41");
    scene.moveTo(scene.barMiddle(0, 0)+Point(1, 0));
    CHECK(asked.size() == 1);

    controls::Label *label=timeline->newChild<controls::Label>();
    std::vector<size_t> filled;

    timeline->setHoverControl(label, [&filled](const Timeline &t, size_t lane, size_t index, controls::Base *control)
        {
            filled.push_back(index);
            static_cast<controls::Label *>(control)->setText("run "+std::to_string(t.getBar(lane, index).id));
        });
    CHECK(timeline->getToolTip() == label);
    REQUIRE(filled.size() == 1);
    scene.moveTo(scene.barMiddle(0, 1));
    REQUIRE(filled.size() == 2);
    CHECK(label->getText() == "run 42");
    CHECK(timeline->getToolTip() == label);

    // shown in the canvas's box once the delay has passed
    scene.clock+=scene.canvas->getToolTipDelay()+0.1f;
    scene.frame();
    REQUIRE(scene.canvas->toolTipShown());
    CHECK(scene.canvas->toolTipControl()->content() == label);

    scene.moveTo(Point(timeline->getBarArea().x-2, scene.barMiddle(0, 0).y));
    CHECK(timeline->getToolTip() == nullptr);
    CHECK(!timeline->hasToolTip());
}

TEST_CASE("Timeline click: onBarClicked fires with the lane, the index and the id")
{
    Scene scene;
    Timeline *timeline=scene.timeline;

    timeline->addLane("a");
    timeline->addLane("b");
    timeline->addBar(0, TimelineBar(10, 20, 100));
    timeline->addBar(1, TimelineBar(10, 50, 200));
    timeline->addBar(1, TimelineBar(30, 40, 201));
    timeline->addBar(1, TimelineBar(60, 70, 202));
    scene.frame();

    scene.click(scene.barMiddle(0, 0));
    REQUIRE(scene.clicks.size() == 1);
    CHECK(scene.clicks[0] == Scene::Click{0, 0, 100});
    CHECK(timeline->getClickedLane() == 0);
    CHECK(timeline->getClickedIndex() == 0);
    CHECK(timeline->getClickedId() == 100);

    // the later of two overlapping bars
    scene.click(scene.barMiddle(1, 1));
    REQUIRE(scene.clicks.size() == 2);
    CHECK(scene.clicks[1] == Scene::Click{1, 1, 201});
    scene.click(scene.barMiddle(1, 2));
    REQUIRE(scene.clicks.size() == 3);
    CHECK(scene.clicks[2] == Scene::Click{1, 2, 202});

    // empty lane, a press and a release on two bars, a bar replaced between
    // them: nothing
    Rect bar;
    REQUIRE(timeline->getBarRect(0, 0, bar));
    scene.click(Point(bar.x+bar.w+20, bar.y+2));
    scene.moveTo(scene.barMiddle(1, 2));
    scene.canvas->inputMouseButton(0, true);
    scene.moveTo(scene.barMiddle(0, 0));
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    scene.moveTo(scene.barMiddle(0, 0));
    scene.canvas->inputMouseButton(0, true);
    timeline->setBar(0, 0, TimelineBar(10, 20, 101));
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(scene.clicks.size() == 3);
    CHECK(MouseFocus == nullptr);
}

TEST_CASE("Timeline renderer: built hidden it takes no slot until shown, hidden it hides them, deleted it gives them back")
{
    {
        Scene scene;
        Timeline *timeline=scene.timeline;

        timeline->setHidden(true);
        timeline->addLane("a");
        timeline->addLane("b");
        timeline->addBar(0, TimelineBar(10, 20, 1));
        timeline->addBar(1, TimelineBar(30, 40, 2));
        scene.frame();

        const skin::textured::Timeline &drawn=scene.skinTimeline();

        CHECK(drawn.lanePrimitives().empty());
        CHECK(drawn.hoverPrimitives().empty());

        timeline->setHidden(false);
        scene.frame();
        REQUIRE(drawn.barPrimitives(1).size() == 1);
        const size_t bar=drawn.barPrimitives(1)[0];
        CHECK(scene.renderer.hidden.count(bar) == 0);

        timeline->setHidden(true);
        scene.frame();
        CHECK(scene.renderer.hidden.count(bar) == 1);
        CHECK(scene.renderer.hidden.count(scene.frameId()) == 1);
        timeline->setHidden(false);
        scene.frame();
        CHECK(scene.renderer.hidden.count(bar) == 0);

        std::vector<size_t> ids;
        std::vector<size_t> texts;

        drawn.appendIds(ids, texts);
        REQUIRE(ids.size() > 4);
        for(size_t id : ids)
            REQUIRE(scene.renderer.livePrimitives.count(id) == 1);
        delete timeline;
        scene.timeline=nullptr;
        scene.frame();
        for(size_t id : ids)
            CHECK(scene.renderer.livePrimitives.count(id) == 0);
    }
}
