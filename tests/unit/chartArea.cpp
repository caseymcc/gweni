/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A Chart's area series (Area, StepArea, StackedArea, StackedStepArea) and
 *  the textured renderer's slots for them, on a recording StubRenderer:
 *
 *    stack      the stacked areas stack over every x any of them has, each
 *               one's value there its point's y or 0 where it has none (a
 *               NaN too), positive up and negative down, an area's line
 *               the top of its band at each of those x; the automatic range
 *               holds the stack's top, and a hidden one leaves the stack (its
 *               x and its band) to the others;
 *    fill       an unstacked area fills down to 0 held to its axis's range,
 *               a NaN a gap of the two segments beside it; a step area and a
 *               stacked step area hold each value to the next x, flat
 *               segments and a step line's vertices;
 *    layers     an area's fill is a drawTrapezoid per segment in its series'
 *               layer, in ChartArea<colour>, at getAreaSegment()'s edges, in
 *               the plot's clip; a step area's a rect per step, no trapezoid;
 *               an area's line a drawLine per segment in ChartSeries<colour>
 *               in a layer of its own in front of every series' fill
 *               (depthLayersOf() counts it), its slots after every fill's,
 *               the markers' layer after the lines';
 *    writes     with every range fixed, a changed point of a stacked area
 *               writes the two trapezoids beside it, the two of each area
 *               stacked on it at that x, and those areas' line segments
 *               around it; one of an unstacked area its two trapezoids and
 *               its line's; one of a step area its step's rect; a settled
 *               frame nothing; a far point's trapezoid is cut to the plot
 *               along its edges;
 *    hover      a click inside an area's band (the front one where two
 *               overlap) fires onPointClicked with its point nearest in x,
 *               one near its line's vertex too, nothing above them all; the
 *               hover finds an area's x; a legend click hides a stacked area,
 *               the one above dropping onto what is left, its slots hidden.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/chart.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <vector>

using namespace gweni;
using controls::Chart;
using controls::ChartAxisSide;
using controls::ChartHover;
using controls::ChartSeriesType;
using controls::ChartXAxisType;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

const double NaN=std::numeric_limits<double>::quiet_NaN();

//! Records each slot's rect and z and the clip region every draw was made in,
//! besides StubRenderer's `lines` and `trapezoids`.
class Recorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top, Color bottom,
        Color borderColor, int zIndex) override
    {
        rects[primitiveId]=rect;
        zIndices[primitiveId]=zIndex;
        clips[primitiveId]=clipRegion();
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
    }

    void drawLine(size_t primitiveId, PointF p0, PointF p1, float thickness, int zIndex,
        const PointF *prev=nullptr, const PointF *next=nullptr) override
    {
        zIndices[primitiveId]=zIndex;
        clips[primitiveId]=clipRegion();
        StubRenderer::drawLine(primitiveId, p0, p1, thickness, zIndex, prev, next);
    }

    void drawTrapezoid(size_t primitiveId, float x0, float x1, float top0, float top1, float bottom0,
        float bottom1, Color color, int zIndex) override
    {
        zIndices[primitiveId]=zIndex;
        clips[primitiveId]=clipRegion();
        StubRenderer::drawTrapezoid(primitiveId, x0, x1, top0, top1, bottom0, bottom1, color, zIndex);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.insert(primitiveId);
        StubRenderer::hidePrimitive(primitiveId);
    }

    void clear()
    {
        rects.clear();
        zIndices.clear();
        clips.clear();
        hidden.clear();
        lines.clear();
        trapezoids.clear();
    }

    std::map<size_t, Rect> rects;
    std::map<size_t, int> zIndices;
    std::map<size_t, Rect> clips;
    std::set<size_t> hidden;
};

//! The textured skin's Chart renderer, a StubControl for the rest.
class ChartSkin: public test::StubSkin
{
public:
    explicit ChartSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Chart")
            return std::make_unique<skin::textured::Chart>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene: public event::Handler
{
    explicit Scene(bool legend=false)
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        chart=canvas->newChild<Chart>();
        chart->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        chart->setBounds(10, 20, 400, 240);
        chart->setLegendVisible(legend);
        chart->setXAxisType(ChartXAxisType::Value);
        chart->onPointClicked.add(this, &Scene::clicked);
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
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    const skin::textured::Chart &skinChart() const
    {
        return *static_cast<const skin::textured::Chart *>(chart->getSkinControl());
    }

    std::vector<size_t> fill(size_t series) const { return skinChart().seriesPrimitives(series); }
    std::vector<size_t> line(size_t series) const { return skinChart().areaLinePrimitives(series); }

    std::set<size_t> written() const
    {
        const std::vector<size_t> &ids=renderer.writtenPrimitiveIds();

        return std::set<size_t>(ids.begin(), ids.end());
    }

    const test::StubRenderer::TrapezoidDraw *trapezoid(size_t id) const
    {
        for(const test::StubRenderer::TrapezoidDraw &draw : renderer.trapezoids)
        {
            if(draw.id == id)
                return &draw;
        }
        return nullptr;
    }

    Rect plotClip() const
    {
        const Rect plot=chart->getPlotBounds();
        const Point at=chart->localPosToCanvas(Point(plot.x, plot.y));

        return Rect(at.x, at.y, plot.w, plot.h);
    }

    //! The chart's point at (x, y) in data on the left axis, in its own units.
    Point local(double x, double y) const
    {
        return Point(int(std::floor(chart->xPosition(x))), int(std::floor(chart->yPosition(ChartAxisSide::Left, y))));
    }

    void clickAt(const Point &canvasPoint)
    {
        clock+=1.0f;
        canvas->inputMouseMoved(canvasPoint.x, canvasPoint.y, 0, 0);
        frame();
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    void click(const Point &localPoint) { clickAt(chart->localPosToCanvas(localPoint)); }

    void clicked(event::Info info)
    {
        clicks.push_back({size_t(info.integer), size_t(info.point.x)});
    }

    struct Click
    {
        size_t series;
        size_t index;
        bool operator==(const Click &other) const { return series == other.series && index == other.index; }
    };

    Recorder renderer;
    ChartSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    Chart *chart=nullptr;
    float clock=10.0f;
    std::vector<Click> clicks;
};

std::set<size_t> slots(const std::vector<size_t> &ids, size_t from, size_t to)
{
    return std::set<size_t>(ids.begin()+std::ptrdiff_t(from), ids.begin()+std::ptrdiff_t(to));
}

std::set<size_t> joined(std::initializer_list<std::set<size_t>> sets)
{
    std::set<size_t> all;

    for(const std::set<size_t> &one : sets)
        all.insert(one.begin(), one.end());
    return all;
}

}//namespace

TEST_CASE("Chart areas: stacked areas stack over every x any has, a missing x counting as 0")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXRange(-0.5, 2.5);
    chart->setYRange(ChartAxisSide::Left, 0.0, 10.0);

    const size_t low=chart->addSeries("low", ChartSeriesType::StackedArea);
    const size_t high=chart->addSeries("high", ChartSeriesType::StackedArea);
    const size_t line=chart->addSeries("line", ChartSeriesType::Line);

    chart->setSeriesPoints(low, {{0, 1}, {1, 2}, {2, 3}});
    // none at x = 1: 0 there
    chart->setSeriesPoints(high, {{0, 4}, {2, 5}});
    chart->setSeriesPoints(line, {{0.5, 9}});
    scene.frame();

    CHECK(chart->getAreaStackXs() == std::vector<double>{0.0, 1.0, 2.0});
    CHECK(chart->getAreaStackBase(low, 1) == 0.0);
    CHECK(chart->getAreaStackTop(low, 1) == 2.0);
    CHECK(chart->getAreaStackBase(high, 0) == 1.0);
    CHECK(chart->getAreaStackTop(high, 0) == 5.0);
    CHECK(chart->getAreaStackBase(high, 1) == 2.0);
    CHECK(chart->getAreaStackTop(high, 1) == 2.0);
    CHECK(chart->getAreaStackTop(high, 2) == 8.0);
    // a line has no stack
    CHECK(chart->getAreaSegmentCount(line) == 0);

    // each stacked area a segment between each two x of the stack, its line
    // the top of its band
    REQUIRE(chart->getAreaSegmentCount(high) == 2);
    double x0, x1, top0, top1, base0, base1;

    REQUIRE(chart->getAreaSegment(high, 0, x0, x1, top0, top1, base0, base1));
    CHECK(x0 == doctest::Approx(chart->xPosition(0.0)));
    CHECK(x1 == doctest::Approx(chart->xPosition(1.0)));
    CHECK(top0 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 5.0)));
    CHECK(top1 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 2.0)));
    CHECK(base0 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 1.0)));
    CHECK(base1 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 2.0)));
    // the low one's top is the high one's base, at every x of the stack
    for(size_t segment=0; segment<2; ++segment)
    {
        double lx0, lx1, ltop0, ltop1, lbase0, lbase1;

        REQUIRE(chart->getAreaSegment(low, segment, lx0, lx1, ltop0, ltop1, lbase0, lbase1));
        REQUIRE(chart->getAreaSegment(high, segment, x0, x1, top0, top1, base0, base1));
        CHECK(ltop0 == base0);
        CHECK(ltop1 == base1);
        CHECK(lbase0 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 0.0)));
    }

    REQUIRE(chart->getVertexCount(high) == 3);
    PointF vertex;

    REQUIRE(chart->getVertex(high, 1, vertex));
    CHECK(vertex.y == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 2.0)));
    // a point of a stacked area is at the top of its band
    CHECK(chart->getPointPosition(high, 1).y == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 8.0)));

    // negative values stack down, a NaN counts as 0
    const size_t below=chart->addSeries("below", ChartSeriesType::StackedArea);

    chart->setSeriesPoints(below, {{0, -2}, {1, NaN}, {2, -1}});
    scene.frame();
    CHECK(chart->getAreaStackBase(below, 0) == 0.0);
    CHECK(chart->getAreaStackTop(below, 0) == -2.0);
    CHECK(chart->getAreaStackBase(below, 1) == 2.0);
    CHECK(chart->getAreaStackTop(below, 1) == 2.0);
    chart->removeSeries(below);

    // the automatic range holds the stack's top
    chart->setYAutoRange(ChartAxisSide::Left);
    scene.frame();
    CHECK(chart->getYMax(ChartAxisSide::Left) >= 9.0);
    CHECK(chart->getYMin(ChartAxisSide::Left) <= 0.0);

    // hidden, the low one leaves the stack: its x at 1 and its band
    chart->setSeriesVisible(low, false);
    scene.frame();
    CHECK(chart->getAreaStackXs() == std::vector<double>{0.0, 2.0});
    CHECK(chart->getAreaStackBase(high, 0) == 0.0);
    CHECK(chart->getAreaStackTop(high, 1) == 5.0);
    CHECK(chart->getAreaSegmentCount(high) == 1);
    CHECK(!chart->getAreaSegment(low, 0, x0, x1, top0, top1, base0, base1));
    CHECK(!chart->getVertex(low, 0, vertex));
}

TEST_CASE("Chart areas: an area fills down to 0 held to its range, a step area holds its value to the next x")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXRange(-0.5, 3.5);
    chart->setYRange(ChartAxisSide::Left, 2.0, 10.0);

    const size_t area=chart->addSeries("area", ChartSeriesType::Area);
    const size_t step=chart->addSeries("step", ChartSeriesType::StepArea);

    chart->setSeriesPoints(area, {{0, 4}, {1, NaN}, {2, 6}, {3, 5}});
    chart->setSeriesPoints(step, {{0, 3}, {1, 4}, {2, 9}});
    scene.frame();

    // 0 is below the range: the area fills to the range's bottom
    CHECK(chart->getAreaBaseValue(ChartAxisSide::Left) == 2.0);

    double x0, x1, top0, top1, base0, base1;

    REQUIRE(chart->getAreaSegmentCount(area) == 3);
    // a NaN is a gap of the two segments beside it
    CHECK(!chart->getAreaSegment(area, 0, x0, x1, top0, top1, base0, base1));
    CHECK(!chart->getAreaSegment(area, 1, x0, x1, top0, top1, base0, base1));
    REQUIRE(chart->getAreaSegment(area, 2, x0, x1, top0, top1, base0, base1));
    CHECK(top0 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 6.0)));
    CHECK(top1 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 5.0)));
    CHECK(base0 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 2.0)));
    CHECK(base1 == base0);

    // the step: flat at the value of its first x, its line a step line's
    REQUIRE(chart->getAreaSegmentCount(step) == 2);
    REQUIRE(chart->getAreaSegment(step, 1, x0, x1, top0, top1, base0, base1));
    CHECK(x0 == doctest::Approx(chart->xPosition(1.0)));
    CHECK(x1 == doctest::Approx(chart->xPosition(2.0)));
    CHECK(top0 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 4.0)));
    CHECK(top1 == top0);
    REQUIRE(chart->getVertexCount(step) == 5);
    PointF riser;

    REQUIRE(chart->getVertex(step, 1, riser));
    CHECK(riser.x == doctest::Approx(chart->xPosition(1.0)));
    CHECK(riser.y == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 3.0)));

    // with 0 in the range, it fills to 0
    chart->setYRange(ChartAxisSide::Left, -5.0, 10.0);
    scene.frame();
    CHECK(chart->getAreaBaseValue(ChartAxisSide::Left) == 0.0);
    REQUIRE(chart->getAreaSegment(area, 2, x0, x1, top0, top1, base0, base1));
    CHECK(base0 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 0.0)));

    // a stacked step area: flat on the flat band below it
    const size_t first=chart->addSeries("first", ChartSeriesType::StackedStepArea);
    const size_t second=chart->addSeries("second", ChartSeriesType::StackedStepArea);

    chart->setSeriesPoints(first, {{0, 1}, {2, 2}});
    chart->setSeriesPoints(second, {{0, 1}, {1, 1}, {2, 1}});
    scene.frame();
    REQUIRE(chart->getAreaSegmentCount(second) == 2);
    REQUIRE(chart->getAreaSegment(second, 0, x0, x1, top0, top1, base0, base1));
    CHECK(top0 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 2.0)));
    CHECK(top1 == top0);
    CHECK(base0 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 1.0)));
    CHECK(base1 == base0);
    // at x = 1, where the first has no point, the second rests on 0
    REQUIRE(chart->getAreaSegment(second, 1, x0, x1, top0, top1, base0, base1));
    CHECK(base0 == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 0.0)));
    CHECK(chart->getVertexCount(second) == 5);
}

TEST_CASE("Chart renderer: an area's fill is a trapezoid per segment in its layer, its line in front of every fill")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXRange(-0.5, 4.5);
    chart->setYRange(ChartAxisSide::Left, 0.0, 10.0);

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);
    const size_t area=chart->addSeries("area", ChartSeriesType::StackedArea);
    const size_t step=chart->addSeries("step", ChartSeriesType::StepArea);

    chart->setSeriesPoints(line, {{0, 1}, {4, 9}});
    chart->setSeriesPoints(area, {{0, 2}, {1, 5}, {2, 3}, {3, 6}, {4, 4}});
    chart->setSeriesPoints(step, {{0, 1}, {2, 2}, {4, 1}});
    chart->addMarker(2.5);
    scene.frame();

    const int z=chart->getZIndex();
    const Rect bounds=chart->getRenderBounds();
    const int S=3;
    const int A=2;
    const size_t layers=const_cast<skin::Control *>(chart->getSkinControl())->depthLayersOf(chart);

    CHECK(layers == size_t(skin::textured::Chart::SeriesLayer+S+A+2));

    // the area's fill: a trapezoid per segment at getAreaSegment()'s edges, in
    // ChartArea<colour>, in its series' layer and the plot's clip
    const std::vector<size_t> fill=scene.fill(area);

    REQUIRE(fill.size() == 4);
    for(size_t i=0; i<fill.size(); ++i)
    {
        const test::StubRenderer::TrapezoidDraw *draw=scene.trapezoid(fill[i]);
        double x0, x1, top0, top1, base0, base1;

        CAPTURE(i);
        REQUIRE(draw != nullptr);
        REQUIRE(chart->getAreaSegment(area, i, x0, x1, top0, top1, base0, base1));
        CHECK(draw->x0 == doctest::Approx(x0+bounds.x));
        CHECK(draw->x1 == doctest::Approx(x1+bounds.x));
        CHECK(draw->top0 == doctest::Approx(top0+bounds.y));
        CHECK(draw->top1 == doctest::Approx(top1+bounds.y));
        CHECK(draw->bottom0 == doctest::Approx(base0+bounds.y));
        CHECK(draw->bottom1 == doctest::Approx(base1+bounds.y));
        CHECK((draw->color == tokyonight::chartArea(chart->getSeriesColorIndex(area)).top));
        CHECK(draw->zIndex == z+skin::textured::Chart::SeriesLayer+int(area));
        CHECK((scene.renderer.clips.at(fill[i]) == scene.plotClip()));
    }

    // the step area's fill: a rect per step in whole units, no trapezoid
    const std::vector<size_t> steps=scene.fill(step);

    REQUIRE(steps.size() == 2);
    for(size_t id : steps)
    {
        CHECK(scene.trapezoid(id) == nullptr);
        REQUIRE(scene.renderer.rects.count(id) == 1);
        CHECK(scene.renderer.zIndices.at(id) == z+skin::textured::Chart::SeriesLayer+int(step));
    }
    // the two steps meet at x = 2's unit, from the first's value and the second's
    double x0, x1, top0, top1, base0, base1;

    REQUIRE(chart->getAreaSegment(step, 0, x0, x1, top0, top1, base0, base1));
    CHECK(scene.renderer.rects.at(steps[0]).x+scene.renderer.rects.at(steps[0]).w
        == scene.renderer.rects.at(steps[1]).x);
    CHECK(scene.renderer.rects.at(steps[0]).y == int(std::lround(top0))+bounds.y);

    // each area's line in front of every fill, in its series' colour, its
    // slots after every fill's; the line series in its own layer
    std::vector<size_t> fills;

    for(size_t s=0; s<size_t(S); ++s)
    {
        const std::vector<size_t> ids=scene.fill(s);

        fills.insert(fills.end(), ids.begin(), ids.end());
    }
    const size_t lastFill=*std::max_element(fills.begin(), fills.end());
    int rank=0;

    for(size_t s : {area, step})
    {
        const std::vector<size_t> ids=scene.line(s);

        CAPTURE(s);
        REQUIRE(ids.size() == chart->getVertexCount(s)-1);
        for(size_t id : ids)
        {
            CHECK(id > lastFill);
            CHECK(scene.renderer.zIndices.at(id) == z+skin::textured::Chart::SeriesLayer+S+rank);
        }
        bool found=false;

        for(const test::StubRenderer::LineDraw &draw : scene.renderer.lines)
        {
            if(draw.id == ids[1])
            {
                found=true;
                CHECK((draw.color == tokyonight::chartSeries(chart->getSeriesColorIndex(s)).top));
                // drawn with its neighbours
                CHECK(draw.prev.has_value());
                CHECK(draw.next.has_value());
            }
        }
        CHECK(found);
        ++rank;
    }
    CHECK(scene.line(line).empty());
    for(size_t id : scene.fill(line))
        CHECK(scene.renderer.zIndices.at(id) == z+skin::textured::Chart::SeriesLayer+int(line));

    // the markers in front of the lines
    REQUIRE(scene.skinChart().markerPrimitives().size() == 1);
    CHECK(scene.renderer.zIndices.at(scene.skinChart().markerPrimitives()[0])
        == z+skin::textured::Chart::SeriesLayer+S+A);
}

TEST_CASE("Chart renderer: with fixed ranges a changed area point writes the trapezoids beside it and those stacked on it")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXRange(-0.5, 6.5);
    chart->setYRange(ChartAxisSide::Left, 0.0, 20.0);

    const size_t low=chart->addSeries("low", ChartSeriesType::StackedArea);
    const size_t high=chart->addSeries("high", ChartSeriesType::StackedArea);
    const size_t area=chart->addSeries("area", ChartSeriesType::Area);
    const size_t step=chart->addSeries("step", ChartSeriesType::StepArea);

    chart->setSeriesPoints(low, {{0, 1}, {1, 2}, {2, 3}, {3, 2}, {4, 1}, {5, 2}, {6, 3}});
    chart->setSeriesPoints(high, {{0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 2}});
    chart->setSeriesPoints(area, {{0, 5}, {1, 6}, {2, 7}, {3, 6}, {4, 5}, {5, 6}, {6, 7}});
    chart->setSeriesPoints(step, {{0, 1}, {2, 2}, {4, 3}, {6, 1}});
    scene.frame();
    scene.frame();
    CHECK(scene.written().empty());

    // the low one's x = 3: its two trapezoids and the high one's two there,
    // and both lines' segments around their vertex 3 (the two it ends and
    // the two holding it as a neighbour)
    chart->setPointValue(low, 3, 5.0);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    std::set<size_t> trapezoids;

    for(const test::StubRenderer::TrapezoidDraw &draw : scene.renderer.trapezoids)
        trapezoids.insert(draw.id);
    CHECK(trapezoids == joined({slots(scene.fill(low), 2, 4), slots(scene.fill(high), 2, 4)}));
    CHECK(scene.written() == joined({slots(scene.fill(low), 2, 4), slots(scene.fill(high), 2, 4),
        slots(scene.line(low), 1, 5), slots(scene.line(high), 1, 5)}));
    scene.frame();
    CHECK(scene.written().empty());

    // the top of the stack: its own two and its line's
    chart->setPointValue(high, 5, 4.0);
    scene.frame();
    CHECK(scene.written() == joined({slots(scene.fill(high), 4, 6), slots(scene.line(high), 3, 6)}));

    // an unstacked area: its two and its line's
    chart->setPointValue(area, 1, 9.0);
    scene.frame();
    CHECK(scene.written() == joined({slots(scene.fill(area), 0, 2), slots(scene.line(area), 0, 3)}));

    // a step area's point: its step's rect (the step before ends at its x,
    // at its own value) and its step line's segments around it
    chart->setPointValue(step, 1, 4.0);
    scene.frame();
    const std::set<size_t> written=scene.written();
    std::set<size_t> fillWritten;

    for(size_t id : scene.fill(step))
    {
        if(written.count(id))
            fillWritten.insert(id);
    }
    CHECK(fillWritten == std::set<size_t>{scene.fill(step)[1]});
    const std::vector<size_t> stepFill=scene.fill(step);
    const std::vector<size_t> stepLine=scene.line(step);

    for(size_t id : written)
        CHECK((std::count(stepFill.begin(), stepFill.end(), id) + std::count(stepLine.begin(), stepLine.end(), id)) == 1);

    scene.frame();
    CHECK(scene.written().empty());

    // a point far out of the fixed range: its trapezoids cut to the plot
    // along their edges, the far end at the box beside the plot
    chart->setSeriesPoints(area, {{0, 5}, {1, 6}, {1000, 7}});
    scene.frame();
    const std::vector<size_t> fill=scene.fill(area);

    REQUIRE(fill.size() == 2);
    const test::StubRenderer::TrapezoidDraw *cut=scene.trapezoid(fill[1]);
    const Rect plot=chart->getPlotBounds();
    const Rect bounds=chart->getRenderBounds();

    REQUIRE(cut != nullptr);
    CHECK(cut->x1 == doctest::Approx(bounds.x+plot.x+plot.w+2.0f));
    // the top edge keeps its slope: at the cut end it is on the line to (1000, 7)
    const double xs[2]={chart->xPosition(1.0), chart->xPosition(1000.0)};
    const double ys[2]={chart->yPosition(ChartAxisSide::Left, 6.0), chart->yPosition(ChartAxisSide::Left, 7.0)};
    const double t=(cut->x1-bounds.x-xs[0])/(xs[1]-xs[0]);

    CHECK(cut->top1 == doctest::Approx(bounds.y+ys[0]+(ys[1]-ys[0])*t).epsilon(1e-4));
}

TEST_CASE("Chart hover: a click inside an area's band or near its line picks its point, a legend click hides it")
{
    Scene scene(true);
    Chart *chart=scene.chart;

    chart->setXRange(-0.5, 4.5);
    chart->setYRange(ChartAxisSide::Left, 0.0, 20.0);

    const size_t low=chart->addSeries("low", ChartSeriesType::StackedArea);
    const size_t high=chart->addSeries("high", ChartSeriesType::StackedArea);

    chart->setSeriesPoints(low, {{0, 4}, {1, 6}, {2, 4}, {3, 6}, {4, 4}});
    chart->setSeriesPoints(high, {{0, 6}, {1, 6}, {2, 6}, {3, 6}, {4, 6}});
    scene.frame();
    scene.frame();

    // the hover finds an area's x
    ChartHover hover;

    REQUIRE(chart->hoverAt(scene.local(2.1, 1.0), hover));
    CHECK(hover.x == 2.0);
    CHECK(hover.points == std::vector<std::ptrdiff_t>{2, 2});

    // inside the low band near x = 1, inside the high band near x = 3, then
    // just under the high line's vertex at x = 2 (4 + 6 = 10)
    scene.click(scene.local(1.2, 2.0));
    scene.click(scene.local(2.8, 10.0));
    const PointF vertex=chart->getPointPosition(high, 2);
    scene.click(Point(int(vertex.x)+1, int(vertex.y)-3));
    // above every band: nothing
    scene.click(scene.local(0.5, 18.0));

    REQUIRE(scene.clicks.size() == 3);
    CHECK(scene.clicks[0] == Scene::Click{low, 1});
    CHECK(scene.clicks[1] == Scene::Click{high, 3});
    CHECK(scene.clicks[2] == Scene::Click{high, 2});

    // a legend click hides the low one: the high one drops onto 0, the low
    // one's slots hidden
    const std::vector<size_t> lowFill=scene.fill(low);
    controls::Base *row=reinterpret_cast<controls::Base *>(chart->getLegendRow(low));

    REQUIRE(row != nullptr);
    scene.clickAt(row->localPosToCanvas(Point(row->getWidth()/2, row->getHeight()/2)));
    CHECK(!chart->isSeriesVisible(low));
    CHECK(chart->getAreaStackBase(high, 1) == 0.0);
    CHECK(chart->getAreaStackTop(high, 1) == 6.0);

    // drawn on the frame of the click: every slot of the low one hidden
    scene.renderer.clear();
    chart->setSeriesVisible(low, true);
    scene.frame();
    chart->setSeriesVisible(low, false);
    scene.frame();
    for(size_t id : lowFill)
        CHECK(scene.renderer.hidden.count(id) == 1);
    for(size_t id : scene.line(low))
        CHECK(scene.renderer.hidden.count(id) == 1);
}
