/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Timeline's model, layout and hit test, on the headless fixture
 *  (a StubControl for every type, so no renderer). The layout: the time
 *  axis's labels in a row at the top, inside the padding, above the lanes;
 *  the caption column at the lanes' left, the widest caption with its padding
 *  (a third of the inner width at most, or the app's width), each lane's
 *  caption centred on its lane; the bar area the body's viewport right of the
 *  column, the range's ends at its edges, ticks in the timeline's zone. The
 *  lane from y: the lane under each unit of the bar area, none above or below
 *  it or past the last lane, scrolled with the lanes. The bar under a point:
 *  a binary search on the lane's bars sorted by start finds each bar of a
 *  packed lane at its first and last unit and nothing in the gaps between
 *  them or outside the area; where bars overlap the later one is found (and
 *  has the higher overlap level, MaxOverlapLevel at most), an earlier long
 *  bar under short later ones is still found between them (the running
 *  maximum of the ends), bars that only touch do not overlap; and on lanes of
 *  random bars every unit's answer is a brute force's over getBarRect() (the
 *  highest level, then the latest). A bar's rect: a unit wide at least, held
 *  OffAreaMargin outside the area however far out of range, none for a bar
 *  wholly out of range, a NaN or a lane out of view. Lanes that do not fit
 *  scroll: the bar shows, the wheel scrolls WheelLanes lanes a notch under the
 *  fixed axis and caption column, the captions of the lanes out of view are
 *  hidden, the offset is clamped at the end, and a scroll lays out in one
 *  pass as a full pass does. The automatic range is the bars' extent (a
 *  second round a single instant); a bar inside it lays nothing out, one past
 *  it lays the timeline out once, and with a fixed range no bar lays
 *  anything out.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/text.h>
#include <gweni/controls/timeline.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using namespace gweni;
using controls::Timeline;
using controls::TimelineBar;

namespace
{

struct Scene
{
    explicit Scene(int width=480, int height=240)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1200, 800);
        timeline=canvas->newChild<Timeline>();
        timeline->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        timeline->setBounds(10, 20, width, height);
        timeline->setUtcOffset(controls::axis::fixedUtcOffset(0));
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    int textWidth(const String &text)
    {
        return renderer.measureText(skin.getDefaultFont(), text).x;
    }

    //! The bar a brute force finds at local: every bar of the lane whose
    //! rect holds the point, the highest level and then the latest.
    bool bruteBarAt(const Point &local, size_t &lane, size_t &index) const
    {
        const int laneIndex=timeline->laneAt(local.y);
        const Rect area=timeline->getBarArea();

        if(laneIndex < 0 || local.x < area.x || local.x >= area.x+area.w)
            return false;

        bool found=false;

        for(size_t i=0; i < timeline->getBarCount(size_t(laneIndex)); ++i)
        {
            Rect rect;

            if(!timeline->getBarRect(size_t(laneIndex), i, rect) || local.x < rect.x || local.x >= rect.x+rect.w)
                continue;

            const int level=timeline->getOverlapLevel(size_t(laneIndex), i);

            if(!found || level > timeline->getOverlapLevel(size_t(laneIndex), index)
                || (level == timeline->getOverlapLevel(size_t(laneIndex), index) && i > index))
            {
                index=i;
                found=true;
            }
        }
        lane=size_t(laneIndex);
        return found;
    }

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    Timeline *timeline=nullptr;
};

double utc(int64_t year, int month, int day, int hour=0)
{
    return static_cast<double>(controls::axis::daysFromCivil(year, month, day))*86400.0+hour*3600.0;
}

//! The middle row of the lane, in the timeline's coordinates.
int laneMiddle(const Timeline &timeline, size_t lane)
{
    Rect rect;

    REQUIRE(timeline.getLaneRect(lane, rect));
    return rect.y+rect.h/2;
}

}//namespace

TEST_CASE("Timeline: the axis above the lanes, the caption column at their left, the bar area the rest")
{
    Scene scene;
    Timeline *timeline=scene.timeline;

    timeline->addLane("a");
    timeline->addLane("host-01 slot 2");
    timeline->addLane("bb");
    timeline->setXRange(utc(2024, 3, 1), utc(2024, 3, 2));
    scene.frame();

    const Padding &padding=timeline->getPadding();
    const Rect area=timeline->getBarArea();
    const Rect column=timeline->getCaptionColumnBounds();
    const int lineHeight=scene.renderer.lineHeight(scene.skin.getDefaultFont());

    // the widest caption with its padding
    CHECK(timeline->getCaptionColumnWidth() == scene.textWidth("host-01 slot 2")+2*Timeline::CaptionPadding);
    CHECK(column.x == padding.left);
    CHECK(column.w == timeline->getCaptionColumnWidth());
    CHECK(area.x == column.x+column.w);
    // the lanes fit: no bar, the area reaches the padding
    CHECK(!timeline->getBody()->getVerticalScrollBar()->visible());
    CHECK(area.x+area.w == timeline->getWidth()-padding.right);
    CHECK(area.y == padding.top+lineHeight+Timeline::LabelGap);
    CHECK(area.y+area.h == timeline->getHeight()-padding.bottom);
    CHECK(column.y == area.y);
    CHECK(column.h == area.h);

    // the range's ends at the area's edges
    CHECK(timeline->xPosition(utc(2024, 3, 1)) == doctest::Approx(area.x));
    CHECK(timeline->xPosition(utc(2024, 3, 2)) == doctest::Approx(area.x+area.w));
    CHECK(timeline->timeAt(area.x+area.w/2.0) == doctest::Approx(utc(2024, 3, 1, 12)));

    // the ticks in the timeline's zone, a label each, in the axis's row
    const std::vector<controls::axis::Tick> &ticks=timeline->getTicks();

    REQUIRE(ticks.size() >= 3);
    CHECK(ticks.front().label == "Mar 1");
    CHECK(ticks.front().position == doctest::Approx(area.x));
    REQUIRE(timeline->getTickLabels().size() == ticks.size());
    for(size_t i=0; i < ticks.size(); ++i)
    {
        const controls::internal::Text *label=timeline->getTickLabels()[i];

        CHECK(label->getText() == ticks[i].label);
        CHECK(label->getY() == padding.top);
        CHECK(label->getY()+label->getHeight() <= area.y);
        CHECK(label->getX() >= padding.left);
        CHECK(label->getX()+label->getWidth() <= timeline->getWidth()-padding.right);
    }

    // each caption centred on its lane, in the column
    for(size_t lane=0; lane < 3; ++lane)
    {
        const controls::internal::Text *label=timeline->getCaptionLabel(lane);
        Rect laneRect;

        REQUIRE(timeline->getLaneRect(lane, laneRect));
        CHECK(laneRect.y == area.y+int(lane)*timeline->getLaneHeight());
        CHECK(label->visible());
        CHECK(label->getText() == timeline->getLaneCaption(lane));
        CHECK(label->getX() == Timeline::CaptionPadding);
        CHECK(column.y+label->getY() == laneRect.y+(timeline->getLaneHeight()-lineHeight)/2);
    }

    // a fixed column, and an automatic one at a third of the inner width
    timeline->setCaptionColumnWidth(50);
    scene.frame();
    CHECK(timeline->getCaptionColumnWidth() == 50);
    CHECK(timeline->getBarArea().x == padding.left+50);
    timeline->setCaptionColumnWidth(0);
    timeline->setLaneCaption(0, String(200, 'x'));
    scene.frame();
    CHECK(timeline->getCaptionColumnWidth() == (timeline->getWidth()-padding.left-padding.right)/3);
}

TEST_CASE("Timeline: the lane from y, scrolled with the lanes")
{
    Scene scene;
    Timeline *timeline=scene.timeline;

    for(int i=0; i < 5; ++i)
        timeline->addLane("lane");
    scene.frame();

    const Rect area=timeline->getBarArea();
    const int height=timeline->getLaneHeight();

    CHECK(timeline->laneAt(area.y-1) == -1);
    CHECK(timeline->laneAt(area.y) == 0);
    CHECK(timeline->laneAt(area.y+height-1) == 0);
    CHECK(timeline->laneAt(area.y+height) == 1);
    CHECK(timeline->laneAt(area.y+4*height+height-1) == 4);
    // past the last lane
    CHECK(timeline->laneAt(area.y+5*height) == -1);

    // enough lanes to scroll
    for(int i=0; i < 35; ++i)
        timeline->addLane("lane");
    // clamped at once to what the last layout gave
    timeline->setScrollOffset(30);
    CHECK(timeline->getScrollOffset() == 0);
    scene.frame();
    timeline->setScrollOffset(30);
    scene.frame();
    REQUIRE(timeline->getScrollOffset() == 30);

    const Rect scrolled=timeline->getBarArea();

    CHECK(timeline->laneAt(scrolled.y) == 1);
    CHECK(timeline->laneAt(scrolled.y+9) == 1);
    CHECK(timeline->laneAt(scrolled.y+10) == 2);
    CHECK(timeline->laneAt(scrolled.y+scrolled.h-1) == (30+scrolled.h-1)/height);
    CHECK(timeline->laneAt(scrolled.y+scrolled.h) == -1);
    CHECK(timeline->laneAt(scrolled.y-1) == -1);
}

TEST_CASE("Timeline: a binary search finds each bar of a packed lane, and nothing in the gaps")
{
    Scene scene(1040, 240);
    Timeline *timeline=scene.timeline;

    timeline->addLane("a");
    timeline->addLane("b");
    timeline->setXRange(0.0, 500.0);

    // 50 bars [10i, 10i+6], added out of order so an index is not its place
    std::vector<size_t> indexOf(50);

    for(int k=0; k < 50; ++k)
    {
        const int i=(k*17)%50;

        indexOf[size_t(i)]=timeline->addBar(1, TimelineBar(10.0*i, 10.0*i+6.0, uint64_t(1000+i)));
    }
    scene.frame();

    const Rect area=timeline->getBarArea();
    const int y=laneMiddle(*timeline, 1);
    size_t lane=0;
    size_t index=0;

    REQUIRE(area.w > 900);
    for(int i=0; i < 50; ++i)
    {
        Rect rect;

        REQUIRE(timeline->getBarRect(1, indexOf[size_t(i)], rect));
        CHECK(timeline->getBar(1, indexOf[size_t(i)]).id == uint64_t(1000+i));
        CHECK(timeline->getOverlapLevel(1, indexOf[size_t(i)]) == 0);

        // its first and its last unit
        REQUIRE(timeline->barAt(Point(rect.x, y), lane, index));
        CHECK(lane == 1);
        CHECK(index == indexOf[size_t(i)]);
        REQUIRE(timeline->barAt(Point(rect.x+rect.w-1, y), lane, index));
        CHECK(index == indexOf[size_t(i)]);

        // the gap after it, every unit of it
        Rect next;

        if(i+1 < 50)
            REQUIRE(timeline->getBarRect(1, indexOf[size_t(i+1)], next));
        else
            next=Rect(area.x+area.w, rect.y, 0, rect.h);
        CHECK(next.x > rect.x+rect.w);
        for(int x=rect.x+rect.w; x < next.x; ++x)
            CHECK(!timeline->barAt(Point(x, y), lane, index));
    }

    // a lane with no bar at that x, outside the area, outside the lanes
    CHECK(!timeline->barAt(Point(area.x+30, laneMiddle(*timeline, 0)), lane, index));
    CHECK(!timeline->barAt(Point(area.x-1, y), lane, index));
    CHECK(!timeline->barAt(Point(area.x+area.w, y), lane, index));
    CHECK(!timeline->barAt(Point(area.x+30, area.y+2*timeline->getLaneHeight()), lane, index));
}

TEST_CASE("Timeline: where bars overlap, the later one is on top and the one hit")
{
    Scene scene(1040, 240);
    Timeline *timeline=scene.timeline;

    for(int i=0; i < 6; ++i)
        timeline->addLane("lane");
    timeline->setXRange(0.0, 500.0);

    // a short bar over a long one, and a long bar over a short one
    timeline->addBar(0, TimelineBar(100, 400, 1));
    timeline->addBar(0, TimelineBar(200, 300, 2));
    timeline->addBar(1, TimelineBar(200, 300, 3));
    timeline->addBar(1, TimelineBar(100, 400, 4));
    // a long early bar under short later ones, found between them
    timeline->addBar(2, TimelineBar(0, 500, 5));
    for(int i=0; i < 5; ++i)
        timeline->addBar(2, TimelineBar(100.0*i+50, 100.0*i+60, uint64_t(6+i)));
    // a chain: each overlapping the one before
    timeline->addBar(3, TimelineBar(0, 300, 20));
    timeline->addBar(3, TimelineBar(100, 400, 21));
    timeline->addBar(3, TimelineBar(200, 500, 22));
    // nested past the levels there are
    for(int i=0; i < 6; ++i)
        timeline->addBar(4, TimelineBar(10.0*i, 500.0-10.0*i, uint64_t(30+i)));
    // touching bars do not overlap
    timeline->addBar(5, TimelineBar(100, 200, 40));
    timeline->addBar(5, TimelineBar(200, 300, 41));
    timeline->addBar(5, TimelineBar(0, 100, 42));
    scene.frame();

    size_t lane=0;
    size_t index=0;
    auto hit=[&](size_t laneIndex, double t) -> int
    {
        if(!timeline->barAt(Point(int(std::floor(timeline->xPosition(t))), laneMiddle(*timeline, laneIndex)),
            lane, index))
            return -1;
        CHECK(lane == laneIndex);
        return int(index);
    };

    CHECK(timeline->getOverlapLevel(0, 0) == 0);
    CHECK(timeline->getOverlapLevel(0, 1) == 1);
    CHECK(hit(0, 250) == 1);
    CHECK(hit(0, 150) == 0);
    CHECK(hit(0, 350) == 0);

    CHECK(timeline->getOverlapLevel(1, 0) == 0);
    CHECK(timeline->getOverlapLevel(1, 1) == 1);
    CHECK(hit(1, 250) == 1);
    CHECK(hit(1, 150) == 1);

    CHECK(hit(2, 100) == 0);
    CHECK(hit(2, 255) == 3);
    CHECK(hit(2, 480) == 0);
    for(size_t i=1; i <= 5; ++i)
        CHECK(timeline->getOverlapLevel(2, i) == 1);

    CHECK(timeline->getOverlapLevel(3, 0) == 0);
    CHECK(timeline->getOverlapLevel(3, 1) == 1);
    CHECK(timeline->getOverlapLevel(3, 2) == 2);
    CHECK(hit(3, 50) == 0);
    CHECK(hit(3, 150) == 1);
    CHECK(hit(3, 250) == 2);
    CHECK(hit(3, 450) == 2);

    for(size_t i=0; i < 6; ++i)
        CHECK(timeline->getOverlapLevel(4, i) == std::min(int(i), Timeline::MaxOverlapLevel));
    CHECK(hit(4, 250) == 5);
    CHECK(hit(4, 5) == 0);
    CHECK(hit(4, 35) == 3);
    // past the levels, the later of two at the top level, though it starts
    // earlier (the search meets the other one first)
    timeline->addBar(4, TimelineBar(5, 495, 36));
    CHECK(timeline->getOverlapLevel(4, 6) == Timeline::MaxOverlapLevel);
    CHECK(hit(4, 35) == 6);

    CHECK(timeline->getOverlapLevel(5, 0) == 0);
    CHECK(timeline->getOverlapLevel(5, 1) == 0);
    CHECK(timeline->getOverlapLevel(5, 2) == 0);
    CHECK(hit(5, 150) == 0);
    CHECK(hit(5, 250) == 1);
    CHECK(hit(5, 50) == 2);

    // a bar's new range gives its lane new levels
    timeline->setBar(1, 1, TimelineBar(310, 400, 4));
    CHECK(timeline->getOverlapLevel(1, 1) == 0);
    CHECK(hit(1, 250) == 0);
    timeline->removeBar(0, 0);
    CHECK(timeline->getBarCount(0) == 1);
    CHECK(timeline->getOverlapLevel(0, 0) == 0);
    CHECK(timeline->getBar(0, 0).id == 2);
}

TEST_CASE("Timeline: on lanes of random bars, the hit test is a brute force's")
{
    Scene scene(700, 320);
    Timeline *timeline=scene.timeline;
    uint32_t seed=12345;
    auto next=[&seed]() { seed=seed*1664525u+1013904223u; return seed >> 8; };

    for(int lane=0; lane < 10; ++lane)
    {
        timeline->addLane("lane");
        const int bars=int(next()%40);

        for(int i=0; i < bars; ++i)
        {
            // mostly packed, some overlapping, some a unit or less
            const double start=double(next()%10000)/10.0;
            const double length=(next()%4 == 0) ? double(next()%10)/100.0 : double(next()%600)/10.0;

            timeline->addBar(size_t(lane), TimelineBar(start, start+length, uint64_t(lane*100+i)));
        }
    }
    scene.frame();

    const Rect area=timeline->getBarArea();
    size_t mismatches=0;
    size_t hits=0;

    for(size_t lane=0; lane < timeline->getLaneCount(); ++lane)
    {
        Rect laneRect;

        if(!timeline->getLaneRect(lane, laneRect))
            continue;

        const int y=laneRect.y+laneRect.h/2;

        for(int x=area.x-2; x < area.x+area.w+2; ++x)
        {
            size_t foundLane=0;
            size_t foundIndex=0;
            size_t bruteLane=0;
            size_t bruteIndex=0;
            const bool found=timeline->barAt(Point(x, y), foundLane, foundIndex);
            const bool brute=scene.bruteBarAt(Point(x, y), bruteLane, bruteIndex);

            if(found != brute || (found && (foundLane != bruteLane || foundIndex != bruteIndex)))
                ++mismatches;
            if(found)
                ++hits;
        }
    }
    CHECK(mismatches == 0);
    CHECK(hits > 1000);
}

TEST_CASE("Timeline: a bar's rect is a unit wide at least and held near the area")
{
    Scene scene;
    Timeline *timeline=scene.timeline;

    timeline->addLane("a");
    timeline->setXRange(0.0, 100.0);

    const size_t instant=timeline->addBar(0, TimelineBar(50, 50));
    const size_t huge=timeline->addBar(0, TimelineBar(-1e300, 1e300));
    const size_t before=timeline->addBar(0, TimelineBar(-50, -10));
    const size_t after=timeline->addBar(0, TimelineBar(110, 120));
    const size_t notFinite=timeline->addBar(0, TimelineBar(std::numeric_limits<double>::quiet_NaN(), 10));
    const size_t swapped=timeline->addBar(0, TimelineBar(40, 30));
    scene.frame();

    const Rect area=timeline->getBarArea();
    Rect rect;

    REQUIRE(timeline->getBarRect(0, instant, rect));
    CHECK(rect.w == 1);
    CHECK(rect.y == area.y+Timeline::BarInset);
    CHECK(rect.h == timeline->getLaneHeight()-2*Timeline::BarInset);
    REQUIRE(timeline->getBarRect(0, huge, rect));
    CHECK(rect.x == area.x-Timeline::OffAreaMargin);
    CHECK(rect.x+rect.w == area.x+area.w+Timeline::OffAreaMargin);
    CHECK(!timeline->getBarRect(0, before, rect));
    CHECK(!timeline->getBarRect(0, after, rect));
    CHECK(!timeline->getBarRect(0, notFinite, rect));
    CHECK(timeline->getBar(0, swapped).start == 30.0);
    CHECK(timeline->getBar(0, swapped).end == 40.0);
    REQUIRE(timeline->getBarRect(0, swapped, rect));
    CHECK(rect.x == int(std::lround(timeline->xPosition(30.0))));
    CHECK(rect.x+rect.w == int(std::lround(timeline->xPosition(40.0))));
    // past the lanes and the bars
    CHECK(!timeline->getBarRect(1, 0, rect));
    CHECK(!timeline->getBarRect(0, 99, rect));
}

TEST_CASE("Timeline: lanes that do not fit scroll under the fixed axis and caption column")
{
    const bool verify=controls::layoutVerifyEnabled();
    const size_t failures=controls::layoutVerifyFailures();

    controls::setLayoutVerify(true);
    {
        Scene scene;
        Timeline *timeline=scene.timeline;

        timeline->setXRange(0.0, 100.0);
        for(int i=0; i < 40; ++i)
        {
            timeline->addLane("lane");
            timeline->addBar(size_t(i), TimelineBar(10, 90, uint64_t(i)));
        }

        auto onePass=[&]()
        {
            scene.canvas->updateCanvas();
            const controls::LayoutStats &stats=scene.canvas->getLayoutStats();
            CHECK(stats.passes == 1);
            CHECK(stats.redirtiedType == nullptr);
            CHECK(!stats.capped);
            scene.canvas->renderCanvas();
            CHECK(!scene.canvas->getLayoutStats().renderLayout);
        };

        onePass();

        controls::internal::TimelineBody *body=timeline->getBody();
        const Rect area=timeline->getBarArea();
        const Rect column=timeline->getCaptionColumnBounds();
        const int height=timeline->getLaneHeight();
        const std::vector<controls::internal::Text *> labels=timeline->getTickLabels();
        std::vector<Rect> labelBounds;

        for(const controls::internal::Text *label : labels)
            labelBounds.push_back(label->getBounds());

        // the bar shows, and the area ends at it
        REQUIRE(body->getVerticalScrollBar()->visible());
        CHECK(area.x+area.w == body->getX()+body->getVerticalScrollBar()->getX());

        auto shownCaptions=[&]()
        {
            std::vector<size_t> shown;

            for(size_t lane=0; lane < timeline->getLaneCount(); ++lane)
            {
                Rect laneRect;
                const bool inView=timeline->getLaneRect(lane, laneRect);

                CHECK(timeline->getCaptionLabel(lane)->visible() == inView);
                if(inView)
                {
                    shown.push_back(lane);
                    CHECK(column.y+timeline->getCaptionLabel(lane)->getY()
                        == laneRect.y+(height-scene.renderer.lineHeight(scene.skin.getDefaultFont()))/2);
                }
            }
            return shown;
        };

        std::vector<size_t> shown=shownCaptions();

        REQUIRE(!shown.empty());
        CHECK(shown.front() == 0);
        CHECK(shown.size() == size_t((area.h+height-1)/height));

        // a wheel notch over the lanes: WheelLanes lanes
        const Point middle=timeline->localPosToCanvas(Point(area.x+area.w/2, area.y+area.h/2));

        scene.canvas->inputMouseMoved(middle.x, middle.y, 0, 0);
        scene.canvas->inputMouseWheel(-60);
        onePass();
        CHECK(timeline->getScrollOffset() == Timeline::WheelLanes*height);
        shown=shownCaptions();
        CHECK(shown.front() == size_t(Timeline::WheelLanes));
        CHECK(timeline->laneAt(area.y) == Timeline::WheelLanes);

        // the axis, the column and the area stay
        CHECK((timeline->getBarArea() == area));
        CHECK((timeline->getCaptionColumnBounds() == column));
        REQUIRE(timeline->getTickLabels() == labels);
        for(size_t i=0; i < labels.size(); ++i)
            CHECK((labels[i]->getBounds() == labelBounds[i]));

        // a bar moved with its lane
        Rect bar;

        REQUIRE(timeline->getBarRect(size_t(Timeline::WheelLanes), 0, bar));
        CHECK(bar.y == area.y+Timeline::BarInset);
        CHECK(!timeline->getBarRect(0, 0, bar));

        // a part lane at the top, and the end
        timeline->setScrollOffset(Timeline::WheelLanes*height+7);
        onePass();
        CHECK(timeline->laneAt(area.y) == Timeline::WheelLanes);
        shownCaptions();
        timeline->setScrollOffset(100000);
        onePass();
        CHECK(timeline->getScrollOffset() == 40*height-area.h);
        shown=shownCaptions();
        CHECK(shown.back() == 39);

        // the wheel at the end goes on to the parent: the timeline keeps it
        scene.canvas->inputMouseWheel(-60);
        scene.canvas->updateCanvas();
        CHECK(scene.canvas->getLayoutStats().passes == 0);
        scene.canvas->renderCanvas();
        CHECK(timeline->getScrollOffset() == 40*height-area.h);
        timeline->scrollToLane(0);
        onePass();
        CHECK(timeline->getScrollOffset() == 0);
        timeline->scrollToLane(39);
        onePass();
        CHECK(timeline->getScrollOffset() == 40*height-area.h);

        // a resize, and fewer lanes
        timeline->setBounds(10, 20, 600, 300);
        onePass();
        for(int i=0; i < 30; ++i)
            timeline->removeLane(0);
        onePass();
        CHECK(timeline->getScrollOffset() == 0);
        CHECK(!body->getVerticalScrollBar()->visible());
        shownCaptions();
    }
    CHECK(controls::layoutVerifyFailures() == failures);
    controls::setLayoutVerify(verify);
}

TEST_CASE("Timeline: the automatic range, and a bar inside it lays nothing out")
{
    Scene scene;
    Timeline *timeline=scene.timeline;

    timeline->addLane("a");
    timeline->addLane("b");
    // no bar
    CHECK(timeline->getXMin() == 0.0);
    CHECK(timeline->getXMax() == 1.0);
    // a single instant: a second round it
    timeline->addBar(0, TimelineBar(50, 50));
    CHECK(timeline->getXMin() == 49.5);
    CHECK(timeline->getXMax() == 50.5);

    timeline->addBar(0, TimelineBar(100, 200));
    timeline->addBar(1, TimelineBar(150, 400));
    CHECK(timeline->getXMin() == 50.0);
    CHECK(timeline->getXMax() == 400.0);
    scene.frame();

    // inside the range: nothing laid out
    timeline->addBar(1, TimelineBar(200, 300));
    scene.canvas->updateCanvas();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.canvas->renderCanvas();
    timeline->setBar(1, 1, TimelineBar(210, 310));
    scene.canvas->updateCanvas();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.canvas->renderCanvas();

    // past it: the timeline once
    timeline->addBar(0, TimelineBar(500, 600));
    scene.canvas->updateCanvas();
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    scene.canvas->renderCanvas();
    CHECK(timeline->getXMax() == 600.0);

    // the bar holding an end moved in: the range follows
    timeline->setBar(0, 2, TimelineBar(500, 550));
    scene.frame();
    CHECK(timeline->getXMax() == 550.0);
    timeline->removeBar(0, 0);
    scene.frame();
    CHECK(timeline->getXMin() == 100.0);
    timeline->clearBars(0);
    scene.frame();
    CHECK(timeline->getXMin() == 150.0);
    CHECK(timeline->getXMax() == 400.0);
    CHECK(timeline->getTotalBarCount() == 2);

    // a fixed range: no bar lays anything out
    timeline->setXRange(0.0, 100.0);
    scene.frame();
    timeline->addBar(0, TimelineBar(-500, 5000));
    scene.canvas->updateCanvas();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.canvas->renderCanvas();
    CHECK(timeline->getXMax() == 100.0);
    timeline->setXAutoRange();
    scene.frame();
    CHECK(timeline->getXMin() == -500.0);
    CHECK(timeline->getXMax() == 5000.0);
}
