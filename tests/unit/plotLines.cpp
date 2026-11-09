/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  PlotLines: a series drawn as a polyline in a frame, one primitive slot per
 *  segment (renderer::Base::drawLine), redrawn only where a sample moved.
 *
 *  It runs the textured skin's real PlotLines renderer on a recording
 *  StubRenderer (a StubControl for every other type) and checks:
 *  - the sample points for several sizes, counts and thicknesses: sample i at
 *    T/2 + i*(W-T)/(N-1) across the plot and T/2 + (1-share)*(H-T) down it, the
 *    share clamped (a value past the range at the plot's edge), the first and
 *    last samples' strokes on the plot's first and last units, every point
 *    inside the plot; the recorded segments N-1 of them, each from sample i
 *    to sample i+1 at the plot's render origin, consecutive ones sharing
 *    their endpoint, each drawn with its neighbours (samples i-1 and i+2,
 *    none past the ends), in PlotLine's colour, the line's thickness and the
 *    plot's z+1 (the frame at its z); a NaN sample leaving a gap, the
 *    segments beside it drawn without a neighbour across it, a single sample
 *    drawing no segment;
 *  - a setValue(i) that moves its point drawing exactly segments i-2..i+1,
 *    the two it ends and the two that hold it as a neighbour (fewer at the
 *    ends), one that keeps its y (clamped at the range's edge) marking and
 *    drawing nothing, and a settled frame drawing nothing; a sample set to
 *    NaN hiding segments i-1 and i and drawing i-2 and i+1 again without it,
 *    and set back drawing i-2..i+1 with it;
 *  - a new range (fixed, or the automatic one moved by a value) and a new
 *    thickness redrawing every segment and not the frame;
 *  - push() moving every value one sample left with no layout pass, every
 *    segment of a series of distinct values redrawn, only the segments whose
 *    endpoints or neighbours moved of one with runs of equal values, and a
 *    push that changes no value marking nothing;
 *  - a new sample count resizing the slot list and drawing into no slot it
 *    does not own, the slots given back when it shrinks and when the plot goes;
 *  - the nearest sample's tooltip ("<index>: <value>"), a hover drawing no
 *    slot, and no tooltip outside the plot;
 *  - hiding and showing the plot, and its caption taking its line off the
 *    plot's top (every segment drawn again).
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/plotLines.h>
#include <gweni/controls/text.h>
#include <gweni/controls/toolTip.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <cmath>
#include <map>
#include <set>
#include <vector>

using namespace gweni;

namespace
{

//! Records the frame's draws and every slot hidden, besides StubRenderer's
//! `lines`.
class DrawRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color, Color, Color,
        int zIndex) override
    {
        rects.push_back(primitiveId);
        zIndices[primitiveId]=zIndex;
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.push_back(primitiveId);
    }

    void clear()
    {
        rects.clear();
        hidden.clear();
        lines.clear();
        events.clear();
    }

    std::vector<size_t> rects;
    std::vector<size_t> hidden;
    std::map<size_t, int> zIndices;
};

//! The textured skin's PlotLines renderer, a StubControl for the rest.
class PlotSkin: public test::StubSkin
{
public:
    explicit PlotSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "PlotLines")
            return std::make_unique<skin::textured::PlotLines>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene
{
    explicit Scene(int width=200, int height=80)
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        plot=canvas->newChild<controls::PlotLines>();
        plot->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        plot->setBounds(20, 30, width, height);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control is process-global
        HoveredControl=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Moves the mouse to \p local (the plot's coordinates) and renders a frame.
    void moveTo(int x, int y)
    {
        const Point at=plot->localPosToCanvas(Point(x, y));
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    void waitDelay()
    {
        clock+=canvas->getToolTipDelay();
        frame();
    }

    const std::vector<size_t> &ids() const
    {
        return plot->getSkinControl()->getPrimitives();
    }

    size_t segmentId(size_t segment) const
    {
        return ids().at(1+segment);
    }

    //! The slots of the segments drawn this frame.
    std::set<size_t> linesDrawn() const
    {
        std::set<size_t> drawn;

        for(const test::StubRenderer::LineDraw &line : renderer.lines)
            drawn.insert(line.id);
        return drawn;
    }

    std::set<size_t> segmentIds(size_t from, size_t to) const
    {
        std::set<size_t> set;

        for(size_t i=from; i < to; ++i)
            set.insert(segmentId(i));
        return set;
    }

    bool drewOnlyOwnSlots() const
    {
        const std::set<size_t> own(ids().begin(), ids().end());

        for(size_t id : renderer.rects)
        {
            if(own.count(id) == 0)
                return false;
        }
        for(size_t id : renderer.hidden)
        {
            if(own.count(id) == 0)
                return false;
        }
        for(const test::StubRenderer::LineDraw &line : renderer.lines)
        {
            if(own.count(line.id) == 0)
                return false;
        }
        return true;
    }

    DrawRecorder renderer;
    PlotSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::PlotLines *plot=nullptr;
    float clock=10.0f;
};

std::vector<float> sine(size_t count)
{
    std::vector<float> values(count);

    for(size_t i=0; i < count; ++i)
        values[i]=std::sin(float(i)*0.3f);
    return values;
}

//! Every sample's point against the documented placement.
void checkPoints(const controls::PlotLines *plot)
{
    const Rect area=plot->getPlotBounds();
    const size_t count=plot->getValueCount();
    const float t=plot->getLineThickness();
    const float min=plot->getScaleMin();
    const float max=plot->getScaleMax();

    for(size_t i=0; i < count; ++i)
    {
        CAPTURE(i);
        const PointF point=plot->getSamplePoint(i);
        const float x=(count > 1)?t/2.0f+float(i)*(float(area.w)-t)/float(count-1):float(area.w)/2.0f;
        float share=(plot->getValues()[i]-min)/(max-min);

        CHECK(point.x == doctest::Approx(float(area.x)+x));
        CHECK(point.x-t/2.0f >= float(area.x)-0.001f);
        CHECK(point.x+t/2.0f <= float(area.x+area.w)+0.001f);

        // a NaN value has no y
        if(std::isnan(share))
        {
            CHECK(std::isnan(point.y));
            continue;
        }

        share=std::min(1.0f, std::max(0.0f, share));
        CHECK(point.y == doctest::Approx(float(area.y)+t/2.0f+(1.0f-share)*(float(area.h)-t)));

        // the stroke, T across, inside the plot
        CHECK(point.y-t/2.0f >= float(area.y)-0.001f);
        CHECK(point.y+t/2.0f <= float(area.y+area.h)+0.001f);
    }
}

}//namespace

TEST_CASE("PlotLines: N samples spread across the plot, drawn as N-1 segments sharing their endpoints")
{
    for(int width : {200, 101, 37})
    {
        for(size_t count : {size_t(2), size_t(3), size_t(7), size_t(64)})
        {
            for(float thickness : {1.0f, 3.0f})
            {
                CAPTURE(width);
                CAPTURE(count);
                CAPTURE(thickness);
                Scene scene(width, 60);

                scene.plot->setLineThickness(thickness);
                scene.plot->setValues(sine(count));
                scene.frame();

                const Rect area=scene.plot->getPlotBounds();

                // the frame's padding all round
                CHECK((area == Rect(2, 2, width-4, 56)));
                checkPoints(scene.plot);

                // the first and last samples' strokes on the plot's first and
                // last units
                CHECK(scene.plot->getSamplePoint(0).x == doctest::Approx(float(area.x)+thickness/2.0f));
                CHECK(scene.plot->getSamplePoint(count-1).x
                    == doctest::Approx(float(area.x+area.w)-thickness/2.0f));

                // the frame, then N-1 segments, each in its own slot
                REQUIRE(scene.ids().size() == count);
                REQUIRE(scene.plot->getSegmentCount() == count-1);
                REQUIRE(scene.renderer.lines.size() == count-1);
                CHECK(scene.renderer.rects == std::vector<size_t>{scene.ids()[0]});
                CHECK(scene.drewOnlyOwnSlots());

                // the plot at 20,30 on the canvas renders at its own origin:
                // the recorded points are the control's own
                const Rect origin=scene.plot->getRenderBounds();

                for(size_t i=0; i+1 < count; ++i)
                {
                    CAPTURE(i);
                    const test::StubRenderer::LineDraw &line=scene.renderer.lines[i];
                    const PointF p0=scene.plot->getSamplePoint(i);
                    const PointF p1=scene.plot->getSamplePoint(i+1);

                    CHECK(line.id == scene.segmentId(i));
                    CHECK(line.p0.x == doctest::Approx(float(origin.x)+p0.x));
                    CHECK(line.p0.y == doctest::Approx(float(origin.y)+p0.y));
                    CHECK(line.p1.x == doctest::Approx(float(origin.x)+p1.x));
                    CHECK(line.p1.y == doctest::Approx(float(origin.y)+p1.y));
                    CHECK(line.thickness == thickness);
                    CHECK(line.zIndex == scene.plot->getZIndex()+1);
                    CHECK((line.color == skin::textured::tokyonight::PlotLine.top));

                    // the next segment starts where this one ends, and each is
                    // drawn with the samples before and after it
                    if(i+2 < count)
                        CHECK((scene.renderer.lines[i+1].p0 == line.p1));
                    if(i > 0)
                        CHECK((line.prev == scene.renderer.lines[i-1].p0));
                    else
                        CHECK(!line.prev);
                    if(i+2 < count)
                        CHECK((line.next == scene.renderer.lines[i+1].p1));
                    else
                        CHECK(!line.next);
                }
                CHECK(scene.renderer.zIndices[scene.ids()[0]] == scene.plot->getZIndex());
            }
        }
    }

    Scene scene;

    // a fixed range: a value past it on the plot's edge
    scene.plot->setScaleRange(0.0f, 10.0f);
    scene.plot->setValues({-5.0f, 0.0f, 5.0f, 10.0f, 20.0f});
    scene.frame();
    checkPoints(scene.plot);

    const Rect area=scene.plot->getPlotBounds();

    CHECK(scene.plot->getSamplePoint(0).y == doctest::Approx(float(area.y+area.h)-0.5f));
    CHECK(scene.plot->getSamplePoint(1).y == doctest::Approx(float(area.y+area.h)-0.5f));
    CHECK(scene.plot->getSamplePoint(2).y == doctest::Approx(float(area.y)+float(area.h)/2.0f));
    CHECK(scene.plot->getSamplePoint(3).y == doctest::Approx(float(area.y)+0.5f));
    CHECK(scene.plot->getSamplePoint(4).y == doctest::Approx(float(area.y)+0.5f));

    // the automatic range: min(0, lowest) to the highest value, as PlotHistogram's
    scene.plot->setAutoScale();
    scene.plot->setValues({2.0f, 4.0f, 8.0f});
    CHECK(scene.plot->getScaleMin() == 0.0f);
    CHECK(scene.plot->getScaleMax() == 8.0f);
    checkPoints(scene.plot);

    // a NaN leaves a gap: the two segments that touch it are hidden
    scene.plot->setValues({1.0f, 2.0f, std::nanf(""), 3.0f, 4.0f});
    scene.frame();
    PointF p0, p1;

    CHECK(scene.plot->getSegment(0, p0, p1));
    CHECK_FALSE(scene.plot->getSegment(1, p0, p1));
    CHECK_FALSE(scene.plot->getSegment(2, p0, p1));
    CHECK(scene.plot->getSegment(3, p0, p1));
    CHECK_FALSE(scene.plot->getSegment(4, p0, p1));
    CHECK(scene.linesDrawn() == std::set<size_t>{scene.segmentId(0), scene.segmentId(3)});
    CHECK(std::set<size_t>(scene.renderer.hidden.begin(), scene.renderer.hidden.end())
        == std::set<size_t>{scene.segmentId(1), scene.segmentId(2)});
    // with no neighbour across the gap or past the ends: 0 (the first) has
    // none, nor has 3 (the last)
    REQUIRE(scene.renderer.lines.size() == 2);
    CHECK(!scene.renderer.lines[0].prev);
    CHECK(!scene.renderer.lines[0].next);
    CHECK(!scene.renderer.lines[1].prev);
    CHECK(!scene.renderer.lines[1].next);

    // one sample: at the middle, no segment
    scene.plot->setValues({1.0f});
    scene.frame();
    CHECK(scene.plot->getSegmentCount() == 0);
    CHECK(scene.ids().size() == 1);
    CHECK(scene.plot->getSamplePoint(0).x == doctest::Approx(float(area.x)+float(area.w)/2.0f));
    CHECK(scene.renderer.lines.empty());
}

TEST_CASE("PlotLines: a setValue draws only the four segments around it, and nothing when its y stays")
{
    Scene scene;

    scene.plot->setScaleRange(-1.0f, 1.0f);
    scene.plot->setValues(sine(64));

    // the first frame draws the frame and every segment
    scene.frame();
    REQUIRE(scene.ids().size() == 64);
    CHECK(scene.linesDrawn() == scene.segmentIds(0, 63));
    CHECK(scene.drewOnlyOwnSlots());

    // settled: nothing
    scene.frame();
    CHECK(scene.renderer.lines.empty());
    CHECK(scene.renderer.rects.empty());
    CHECK(scene.renderer.hidden.empty());

    // a sample in the middle moved: the two segments it ends, and the two
    // beyond them that hold it as their neighbour
    scene.plot->setValue(20, 0.75f);
    CHECK((scene.plot->getStateChange() & controls::StateChange_Background) != 0);
    scene.frame();
    CHECK(scene.renderer.lines.size() == 4);
    CHECK(scene.linesDrawn() == scene.segmentIds(18, 22));
    CHECK(scene.renderer.rects.empty());
    CHECK(scene.renderer.hidden.empty());
    for(const test::StubRenderer::LineDraw &line : scene.renderer.lines)
    {
        const PointF moved=line.id == scene.segmentId(18) ? *line.next
            : line.id == scene.segmentId(19) ? line.p1
            : line.id == scene.segmentId(20) ? line.p0 : *line.prev;
        const Rect origin=scene.plot->getRenderBounds();

        CHECK(moved.y == doctest::Approx(float(origin.y)+scene.plot->getSamplePoint(20).y));
    }

    // the first and the last sample: two segments each
    scene.plot->setValue(0, 0.5f);
    scene.frame();
    CHECK(scene.linesDrawn() == scene.segmentIds(0, 2));

    scene.plot->setValue(63, -0.5f);
    scene.frame();
    CHECK(scene.linesDrawn() == scene.segmentIds(61, 63));

    // past the range's top, then further past it: the point stays on the
    // plot's top edge, so the second marks and draws nothing
    scene.plot->setValue(30, 2.0f);
    scene.frame();
    CHECK(scene.linesDrawn() == scene.segmentIds(28, 32));
    scene.plot->setValue(30, 3.0f);
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);
    scene.frame();
    CHECK(scene.renderer.lines.empty());

    // the same value: nothing; an index past the series: ignored
    scene.plot->setValue(30, 3.0f);
    scene.plot->setValue(64, 0.5f);
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);
    CHECK(scene.plot->getValueCount() == 64);

    // to NaN: the two segments it ends hidden, the two beyond them drawn again
    // without it as their neighbour; and back, all four drawn with it
    scene.plot->setValue(40, std::nanf(""));
    scene.frame();
    CHECK(std::set<size_t>(scene.renderer.hidden.begin(), scene.renderer.hidden.end())
        == std::set<size_t>{scene.segmentId(39), scene.segmentId(40)});
    CHECK(scene.linesDrawn() == std::set<size_t>{scene.segmentId(38), scene.segmentId(41)});
    for(const test::StubRenderer::LineDraw &line : scene.renderer.lines)
    {
        if(line.id == scene.segmentId(38))
        {
            CHECK(line.prev.has_value());
            CHECK(!line.next);
        }
        else
        {
            CHECK(!line.prev);
            CHECK(line.next.has_value());
        }
    }
    scene.plot->setValue(40, std::nanf(""));
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);
    scene.plot->setValue(40, 0.0f);
    scene.frame();
    CHECK(scene.linesDrawn() == scene.segmentIds(38, 42));
    CHECK(scene.renderer.hidden.empty());
    for(const test::StubRenderer::LineDraw &line : scene.renderer.lines)
    {
        CHECK(line.prev.has_value());
        CHECK(line.next.has_value());
    }
}

TEST_CASE("PlotLines: a new range or thickness redraws every segment, not the frame")
{
    Scene scene;
    const std::vector<float> values={0.25f, 0.5f, 0.75f, 1.0f, 0.5f, 0.0f};

    scene.plot->setScaleRange(0.0f, 1.0f);
    scene.plot->setValues(values);
    scene.frame();

    const std::set<size_t> all=scene.segmentIds(0, 5);

    // a fixed range twice as high
    scene.plot->setScaleRange(0.0f, 2.0f);
    scene.frame();
    CHECK(scene.linesDrawn() == all);
    CHECK(scene.renderer.rects.empty());

    // the same range again: nothing
    scene.plot->setScaleRange(0.0f, 2.0f);
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);

    // automatic: 0 to 1.0
    scene.plot->setAutoScale();
    CHECK(scene.plot->getScaleMax() == 1.0f);
    scene.frame();
    CHECK(scene.linesDrawn() == all);

    // a value past the automatic range rescales every segment
    scene.plot->setValue(2, 4.0f);
    CHECK(scene.plot->getScaleMax() == 4.0f);
    scene.frame();
    CHECK(scene.linesDrawn() == all);

    // a thicker line moves every point in by its half
    scene.plot->setLineThickness(2.0f);
    scene.frame();
    CHECK(scene.linesDrawn() == all);
    for(const test::StubRenderer::LineDraw &line : scene.renderer.lines)
        CHECK(line.thickness == 2.0f);

    // the same thickness: nothing; none at all: taken as 1
    scene.plot->setLineThickness(2.0f);
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);
    scene.plot->setLineThickness(0.0f);
    CHECK(scene.plot->getLineThickness() == 1.0f);
}

TEST_CASE("PlotLines: push scrolls the series left, redrawing the segments that moved with no layout")
{
    Scene scene;

    scene.plot->setScaleRange(0.0f, 40.0f);
    scene.plot->setValues(std::vector<float>(32, 16.6f));
    scene.frame();
    scene.frame();

    const std::set<size_t> all=scene.segmentIds(0, 31);

    // a new frame time: the oldest dropped, every sample one step left
    scene.plot->push(33.4f);
    CHECK(scene.plot->getValueCount() == 32);
    CHECK(scene.plot->getValues().back() == 33.4f);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    // the segments of equal samples stay where they were; the last one rises
    // to the new sample, and the one before it holds it as its next
    CHECK(scene.linesDrawn() == scene.segmentIds(29, 31));

    scene.plot->push(16.6f);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    // the peak moved left: the segments around its old and new places
    CHECK(scene.linesDrawn() == scene.segmentIds(28, 31));

    // a series of different values: every point moves, every segment drawn
    std::vector<float> ramp(32);

    for(size_t i=0; i < ramp.size(); ++i)
        ramp[i]=float(i);
    scene.plot->setValues(ramp);
    scene.frame();
    scene.plot->push(32.0f);
    CHECK(scene.plot->getValues().front() == 1.0f);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(scene.linesDrawn() == all);

    // a push that changes no value marks nothing
    scene.plot->setValues(std::vector<float>(32, 5.0f));
    scene.frame();
    scene.plot->push(5.0f);
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);

    // an empty series takes its first value
    scene.plot->setValues({});
    scene.plot->push(1.0f);
    CHECK(scene.plot->getValues() == std::vector<float>{1.0f});
}

TEST_CASE("PlotLines: a new sample count resizes the slot list, drawing only into its own slots")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    {
        Scene scene;

        scene.plot->setValues(sine(32));
        scene.frame();
        CHECK(scene.ids().size() == 32);
        CHECK(scene.drewOnlyOwnSlots());

        const size_t frame=scene.ids()[0];

        // more samples: the list grows, the frame's slot kept, every segment drawn
        scene.plot->setValues(sine(40));
        scene.frame();
        CHECK(scene.ids().size() == 40);
        CHECK(scene.ids()[0] == frame);
        CHECK(scene.drewOnlyOwnSlots());
        CHECK_FALSE(test::hasDuplicates(scene.ids()));
        CHECK(scene.linesDrawn() == scene.segmentIds(0, 39));

        // fewer: the extra slots go back to the bank
        scene.plot->setValues(sine(10));
        scene.frame();
        CHECK(scene.ids().size() == 10);
        CHECK(scene.ids()[0] == frame);
        CHECK(scene.drewOnlyOwnSlots());

        // none: the frame alone
        scene.plot->setValues({});
        scene.frame();
        CHECK(scene.ids().size() == 1);
        CHECK(scene.drewOnlyOwnSlots());

        CHECK(scene.renderer.doubleAcquired == 0);
        CHECK(scene.renderer.doubleReleased == 0);
    }

    // the plot gone with its canvas, every id it took is back
    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
}

TEST_CASE("PlotLines: the nearest sample's tooltip, a hover drawing nothing")
{
    Scene scene;

    scene.plot->setScaleRange(0.0f, 1.0f);
    scene.plot->setValues({0.25f, 0.5f, 0.75f, 1.0f, 0.5f});
    scene.frame();

    const Rect area=scene.plot->getPlotBounds();

    // every unit of the plot names the sample nearest its middle
    for(int x=area.x; x < area.x+area.w; ++x)
    {
        CAPTURE(x);
        const int sample=scene.plot->sampleAt(Point(x, area.y));
        REQUIRE(sample >= 0);
        const float middle=float(x)+0.5f;
        const float distance=std::fabs(scene.plot->getSamplePoint(size_t(sample)).x-middle);

        for(size_t i=0; i < 5; ++i)
            CHECK(distance <= std::fabs(scene.plot->getSamplePoint(i).x-middle)+0.001f);
    }
    CHECK(scene.plot->sampleAt(Point(area.x-1, area.y)) == -1);
    CHECK(scene.plot->sampleAt(Point(area.x+area.w, area.y)) == -1);
    CHECK(scene.plot->sampleAt(Point(area.x, area.y+area.h)) == -1);

    // near sample 2: its tooltip after the delay, no slot drawn
    const int x2=int(scene.plot->getSamplePoint(2).x);

    scene.moveTo(x2, area.y+5);
    CHECK(scene.plot->getHoveredSample() == 2);
    CHECK(scene.plot->getToolTipText() == "2: 0.75");
    CHECK(scene.renderer.lines.empty());
    CHECK(scene.renderer.rects.empty());
    CHECK(scene.plot->getStateChange() == controls::StateChange_Nothing);
    CHECK_FALSE(scene.canvas->toolTipShown());

    scene.waitDelay();
    REQUIRE(scene.canvas->toolTipShown());
    CHECK(scene.canvas->toolTipControl()->getText() == "2: 0.75");

    // to sample 3: the text changes at once, still no slot drawn
    scene.moveTo(int(scene.plot->getSamplePoint(3).x), area.y+5);
    CHECK(scene.plot->getHoveredSample() == 3);
    CHECK(scene.renderer.lines.empty());
    CHECK(scene.canvas->toolTipControl()->getText() == "3: 1");

    // the hovered sample's value changes: so does its tooltip
    scene.plot->setValue(3, 0.125f);
    CHECK(scene.plot->getToolTipText() == "3: 0.125");

    // into the padding: no sample, no tooltip
    scene.moveTo(1, 1);
    CHECK(scene.plot->getHoveredSample() == -1);
    CHECK(scene.plot->getToolTipText().empty());
    CHECK_FALSE(scene.canvas->toolTipShown());
}

TEST_CASE("PlotLines: hidden and shown, and its caption taking a line off the plot")
{
    Scene scene;

    scene.plot->setScaleRange(0.0f, 1.0f);
    scene.plot->setValues({0.0f, 0.5f, 1.0f, std::nanf(""), 0.5f});
    scene.frame();

    // hidden: every slot hidden, nothing drawn
    scene.plot->setHidden(true);
    scene.frame();
    CHECK(scene.renderer.lines.empty());
    CHECK(scene.renderer.rects.empty());
    CHECK(std::set<size_t>(scene.renderer.hidden.begin(), scene.renderer.hidden.end())
        == std::set<size_t>(scene.ids().begin(), scene.ids().end()));

    // a change while hidden draws nothing
    scene.plot->setValue(1, 0.25f);
    scene.frame();
    CHECK(scene.renderer.lines.empty());

    // shown: the frame and the two segments with both ends drawn, the gap hidden
    scene.plot->setHidden(false);
    scene.frame();
    CHECK(scene.renderer.rects == std::vector<size_t>{scene.ids()[0]});
    CHECK(scene.linesDrawn() == std::set<size_t>{scene.segmentId(0), scene.segmentId(1)});
    CHECK(std::set<size_t>(scene.renderer.hidden.begin(), scene.renderer.hidden.end())
        == std::set<size_t>{scene.segmentId(2), scene.segmentId(3)});

    // a caption: the plot starts below its line, every drawn segment moves down
    const Rect full=scene.plot->getPlotBounds();

    scene.plot->setCaption("frame time");
    scene.frame();

    controls::internal::Text *caption=scene.plot->getCaptionControl();

    REQUIRE(caption);
    CHECK(caption->getZIndex() >= scene.plot->getZIndex()+2);

    const Rect area=scene.plot->getPlotBounds();

    REQUIRE(caption->getBounds().h > 0);
    CHECK(area.y == full.y+caption->getBounds().h);
    CHECK(scene.linesDrawn() == std::set<size_t>{scene.segmentId(0), scene.segmentId(1)});
    checkPoints(scene.plot);

    // settled with a caption: nothing
    scene.frame();
    CHECK(scene.renderer.lines.empty());
    CHECK(scene.renderer.rects.empty());
}
