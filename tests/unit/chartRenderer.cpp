/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's Chart renderer (skin::textured::Chart) on a recording
 *  StubRenderer, a StubControl for every other type: the frame at the chart's
 *  z; the grid's lines at z+1 in ChartGrid's colour at the shown value axis's
 *  ticks, and the line along the plot's bottom in ChartAxis's; one slot list
 *  per series in its own layer (z+3+i): a slot per bar, a stacked series'
 *  segments where getBarRect() puts them, a slot per segment of a line or
 *  step line, drawn in the series' colour and thickness with its neighbours
 *  (none past the ends or across a NaN); the grid and series drawn in the
 *  plot's clip region; depthLayersOf() 5 plus a layer per series, a child
 *  numbered after them, and a new series moving it. Then, with every range
 *  fixed: a settled frame writes no slot, a changed line point exactly the
 *  segments around it, a changed stacked point its bar and the bars stacked
 *  on it, an appended point its own slot and its joint's; a segment to a
 *  point far out of range cut to a box just outside the plot along the line
 *  (clipped, not clamped, and never culled); a hidden series' slots hidden
 *  and drawn again when shown; the tick labels in Colors.Chart.AxisText; and
 *  every id given back when a series goes and when the chart is deleted.
 *  Spans and markers: a span's rect at z+2 under the series, a marker's
 *  vertical line at z+3+S in front of them in ChartMarker's colour, both in
 *  the plot's clip and their slots taken back to front, the ones out of range
 *  hidden; a moved marker writing its slot alone and marking its caption
 *  (in Colors.Chart.MarkerText) and no other child, a moved span its slot.
 *  And a chart built hidden takes no slot until it is shown and laid out,
 *  then takes them back to front (grid, span, series, marker, crosshair), as
 *  one built shown does.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/chart.h>
#include <gweni/controls/text.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

using namespace gweni;
using controls::ChartAxisSide;
using controls::ChartSeriesType;
using controls::ChartXAxisType;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

//! Records each slot's rect draw (its rect and z) and the clip region every
//! draw was made in, besides StubRenderer's `lines`.
class ClipRecorder: public test::StubRenderer
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
        clips[primitiveId]=clipRegion();
        StubRenderer::drawLine(primitiveId, p0, p1, thickness, zIndex, prev, next);
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

struct Scene
{
    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        chart=canvas->newChild<controls::Chart>();
        chart->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        chart->setBounds(10, 20, 400, 240);
        chart->setXAxisType(ChartXAxisType::Value);
        // the legend's own tests are chartLegend.cpp's
        chart->setLegendVisible(false);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        delete canvas;
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

    const std::vector<size_t> &series(size_t index) const { return skinChart().seriesPrimitives(index); }

    std::set<size_t> written() const
    {
        const std::vector<size_t> &ids=renderer.writtenPrimitiveIds();

        return std::set<size_t>(ids.begin(), ids.end());
    }

    //! The plot's rect at the canvas's origin, the clip its slots are drawn in.
    Rect plotClip() const
    {
        const Rect plot=chart->getPlotBounds();
        const Point at=chart->localPosToCanvas(Point(plot.x, plot.y));

        return Rect(at.x, at.y, plot.w, plot.h);
    }

    const test::StubRenderer::LineDraw *line(size_t id) const
    {
        for(const test::StubRenderer::LineDraw &draw : renderer.lines)
        {
            if(draw.id == id)
                return &draw;
        }
        return nullptr;
    }

    ClipRecorder renderer;
    ChartSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Chart *chart=nullptr;
};

std::set<size_t> slots(const std::vector<size_t> &ids, size_t from, size_t to)
{
    return std::set<size_t>(ids.begin()+std::ptrdiff_t(from), ids.begin()+std::ptrdiff_t(to));
}

}//namespace

TEST_CASE("Chart renderer: a slot list per series, each in its own layer, in the plot's clip")
{
    Scene scene;
    controls::Chart *chart=scene.chart;

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);
    const size_t step=chart->addSeries("step", ChartSeriesType::StepLine);
    const size_t bars=chart->addSeries("bars", ChartSeriesType::Bar);
    const size_t low=chart->addSeries("low", ChartSeriesType::StackedBar);
    const size_t high=chart->addSeries("high", ChartSeriesType::StackedBar);

    chart->setSeriesPoints(line, {{0, 1}, {1, 4}, {2, 2}, {3, 5}, {4, 3}});
    chart->setSeriesPoints(step, {{0, 2}, {1, 3}, {3, 1}, {4, 2}});
    chart->setSeriesPoints(bars, {{0, 1}, {2, 2}, {4, 3}});
    chart->setSeriesPoints(low, {{0, 1}, {2, 1}, {4, 1}});
    chart->setSeriesPoints(high, {{0, 2}, {2, 2}, {4, 2}});
    chart->setSeriesLineThickness(line, 2.0f);
    chart->setSeriesLineThickness(step, 3.0f);
    scene.frame();

    const int z=chart->getZIndex();
    const Rect plotClip=scene.plotClip();
    skin::textured::Chart &skinChart=const_cast<skin::textured::Chart &>(scene.skinChart());

    CHECK(skinChart.depthLayersOf(chart) == 10);
    // the first child (a label) is numbered after the chart's layers
    REQUIRE(!chart->getYLabels(ChartAxisSide::Left).empty());
    CHECK(chart->getYLabels(ChartAxisSide::Left)[0]->getZIndex() >= z+10);

    // the frame at the chart's z
    const size_t frame=chart->getSkinControl()->getPrimitives()[0];
    CHECK(scene.renderer.zIndices.at(frame) == z);
    CHECK((scene.renderer.rects.at(frame) == chart->getRenderBounds()));

    // the grid: a line per left tick but the bottom one, then the axis line
    const std::vector<size_t> &grid=skinChart.gridPrimitives();
    REQUIRE(grid.size() == chart->getYTicks(ChartAxisSide::Left).size());
    for(size_t i=0; i < grid.size(); ++i)
    {
        const test::StubRenderer::LineDraw *draw=scene.line(grid[i]);

        REQUIRE(draw != nullptr);
        CHECK(draw->zIndex == z+1);
        CHECK(draw->p0.y == draw->p1.y);
        CHECK((scene.renderer.clips.at(grid[i]) == plotClip));
        CHECK((draw->color == (i+1 < grid.size() ? tokyonight::ChartGrid.top : tokyonight::ChartAxis.top)));
    }

    // the line's segments, with their neighbours
    const std::vector<size_t> &lineIds=scene.series(line);
    REQUIRE(lineIds.size() == 4);
    for(size_t i=0; i < lineIds.size(); ++i)
    {
        const test::StubRenderer::LineDraw *draw=scene.line(lineIds[i]);
        PointF p0;
        PointF p1;

        REQUIRE(draw != nullptr);
        REQUIRE(chart->getVertex(line, i, p0));
        REQUIRE(chart->getVertex(line, i+1, p1));
        CHECK(draw->p0.x == doctest::Approx(chart->getRenderBounds().x+p0.x));
        CHECK(draw->p1.y == doctest::Approx(chart->getRenderBounds().y+p1.y));
        CHECK(draw->zIndex == z+3);
        CHECK(draw->thickness == 2.0f);
        CHECK((draw->color == tokyonight::ChartSeries0.top));
        CHECK(draw->prev.has_value() == (i > 0));
        CHECK(draw->next.has_value() == (i+1 < lineIds.size()));
        CHECK((scene.renderer.clips.at(lineIds[i]) == plotClip));
    }
    // a step line: 2n-1 vertices, so 2n-2 segments
    const std::vector<size_t> &stepIds=scene.series(step);
    REQUIRE(stepIds.size() == 6);
    const test::StubRenderer::LineDraw *riser=scene.line(stepIds[1]);
    REQUIRE(riser != nullptr);
    CHECK(riser->p0.x == riser->p1.x);   // the step, after the sample
    CHECK(riser->zIndex == z+4);
    CHECK(riser->thickness == 3.0f);
    CHECK((riser->color == tokyonight::ChartSeries1.top));

    // the bars, one slot per point, where getBarRect() puts them
    for(size_t series : {bars, low, high})
    {
        const std::vector<size_t> &ids=scene.series(series);

        REQUIRE(ids.size() == 3);
        for(size_t i=0; i < ids.size(); ++i)
        {
            Rect rect;

            REQUIRE(chart->getBarRect(series, i, rect));
            rect.x+=chart->getRenderBounds().x;
            rect.y+=chart->getRenderBounds().y;
            CHECK((scene.renderer.rects.at(ids[i]) == rect));
            CHECK(scene.renderer.zIndices.at(ids[i]) == z+3+int(series));
            CHECK((scene.renderer.clips.at(ids[i]) == plotClip));
        }
    }

    // a series more is a layer more, and the children move up
    const int labelZ=chart->getYLabels(ChartAxisSide::Left)[0]->getZIndex();
    chart->addSeries("more", ChartSeriesType::Line);
    scene.frame();
    CHECK(skinChart.depthLayersOf(chart) == 11);
    CHECK(chart->getYLabels(ChartAxisSide::Left)[0]->getZIndex() >= chart->getZIndex()+11);
    CHECK(chart->getYLabels(ChartAxisSide::Left)[0]->getZIndex() >= labelZ);
}

TEST_CASE("Chart renderer: with fixed ranges a change writes the slots it moves and no other")
{
    Scene scene;
    controls::Chart *chart=scene.chart;

    chart->setXRange(-0.5, 10.5);
    chart->setYRange(ChartAxisSide::Left, 0.0, 20.0);

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);
    const size_t low=chart->addSeries("low", ChartSeriesType::StackedBar);
    const size_t high=chart->addSeries("high", ChartSeriesType::StackedBar);

    chart->setSeriesPoints(line, {{0, 1}, {1, 4}, {2, 2}, {3, 5}, {4, 3}, {5, 6}});
    chart->setSeriesPoints(low, {{0, 1}, {1, 2}, {2, 3}});
    chart->setSeriesPoints(high, {{0, 2}, {1, 2}, {2, 2}});
    scene.frame();
    scene.frame();
    CHECK(scene.written().empty());

    // a line point: the two segments it ends and the two holding it as a
    // neighbour
    chart->setPointValue(line, 2, 7.0);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(scene.written() == slots(scene.series(line), 0, 4));
    scene.frame();
    CHECK(scene.written().empty());

    // a stacked point: its bar and the one stacked on it
    chart->setPointValue(low, 1, 5.0);
    scene.frame();
    CHECK(scene.written() == std::set<size_t>{scene.series(low)[1], scene.series(high)[1]});

    // the top of a stack: its bar alone
    chart->setPointValue(high, 2, 4.0);
    scene.frame();
    CHECK(scene.written() == std::set<size_t>{scene.series(high)[2]});

    // an appended point: its segment and the joint before it
    chart->appendPoint(line, 6.0, 2.0);
    scene.frame();
    REQUIRE(scene.series(line).size() == 6);
    CHECK(scene.written() == slots(scene.series(line), 4, 6));

    // an appended bar: its slot
    chart->appendPoint(high, 3.0, 1.0);
    scene.frame();
    REQUIRE(scene.series(high).size() == 4);
    CHECK(scene.written() == std::set<size_t>{scene.series(high)[3]});

    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("Chart renderer: a segment to a point out of range is cut along it, not clamped")
{
    Scene scene;
    controls::Chart *chart=scene.chart;

    chart->setXRange(0.0, 4.0);
    chart->setYRange(ChartAxisSide::Left, 0.0, 10.0);

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);

    chart->setSeriesPoints(line, {{0, 5}, {1, 1e6}, {2, 5}, {3, 1e30}, {4, 5}});
    scene.frame();

    const Rect plotClip=scene.plotClip();
    const std::vector<size_t> &ids=scene.series(line);
    REQUIRE(ids.size() == 4);

    for(size_t id : ids)
    {
        const test::StubRenderer::LineDraw *draw=scene.line(id);

        REQUIRE(draw != nullptr);
        INFO("p0 ", draw->p0.x, ",", draw->p0.y, " p1 ", draw->p1.x, ",", draw->p1.y, " clip ", plotClip.x, ",", plotClip.y);
        CHECK(std::isfinite(draw->p0.y));
        CHECK(std::isfinite(draw->p1.y));
        // cut a few units above the plot, never at its edge
        CHECK(std::min(draw->p0.y, draw->p1.y) < float(plotClip.y));
        CHECK(std::min(draw->p0.y, draw->p1.y) > float(plotClip.y)-8.0f);
        CHECK((scene.renderer.clips.at(id) == plotClip));
    }

    // the first segment's cut end lies on the line to the far point
    const test::StubRenderer::LineDraw *first=scene.line(ids[0]);
    const PointF a=chart->getPointPosition(line, 0);
    const PointF b=chart->getPointPosition(line, 1);
    const Point origin(chart->getRenderBounds().x, chart->getRenderBounds().y);
    const float t=(first->p1.y-(a.y+origin.y))/(b.y-a.y);

    CHECK(first->p1.x == doctest::Approx(a.x+origin.x+(b.x-a.x)*t).epsilon(1e-4));
    // and the cut ends are no joint: drawn without that neighbour
    CHECK(!first->next.has_value());
    CHECK(!scene.line(ids[1])->prev.has_value());
}

TEST_CASE("Chart renderer: a hidden series' slots are hidden, and every id is given back")
{
    const test::IdBankCounts before=test::IdBankCounts::now();
    {
        Scene scene;
        controls::Chart *chart=scene.chart;
        const size_t baseline=scene.renderer.livePrimitives.size();

        chart->setXRange(-0.5, 2.5);
        chart->setYRange(ChartAxisSide::Left, 0.0, 5.0);

        const size_t line=chart->addSeries("line", ChartSeriesType::Line);
        const size_t bars=chart->addSeries("bars", ChartSeriesType::Bar);

        chart->setSeriesPoints(line, {{0, 1}, {1, 4}, {2, 2}});
        chart->setSeriesPoints(bars, {{0, 1}, {1, 2}, {2, 3}});
        scene.frame();

        // the tick labels in the skin's axis colour
        for(controls::internal::Text *label : chart->getXLabels())
            CHECK((label->TextColor() == scene.skin.Colors.Chart.AxisText));

        const std::vector<size_t> barIds=scene.series(bars);

        chart->setSeriesVisible(bars, false);
        scene.frame();
        for(size_t id : barIds)
            CHECK(scene.renderer.hidden.count(id) == 1);
        // and the line, its range unchanged, is not drawn again
        for(size_t id : scene.series(line))
            CHECK(scene.line(id) == nullptr);

        chart->setSeriesVisible(bars, true);
        scene.frame();
        for(size_t id : barIds)
            CHECK(scene.renderer.rects.count(id) == 1);

        // a series removed gives its slots back
        const size_t live=scene.renderer.livePrimitives.size();

        chart->removeSeries(bars);
        scene.frame();
        // its legend row (hidden) goes too
        CHECK(live-scene.renderer.livePrimitives.size() >= barIds.size());
        for(size_t id : barIds)
            CHECK(scene.renderer.livePrimitives.count(id) == 0);

        delete chart;
        scene.chart=nullptr;
        scene.frame();
        CHECK(scene.renderer.livePrimitives.size() == baseline);
    }
    CHECK(test::IdBankCounts::now().livePrimitives() == before.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == before.liveTexts());
}

TEST_CASE("Chart renderer: spans under the series, markers in front, a moved marker its slot alone")
{
    Scene scene;
    controls::Chart *chart=scene.chart;

    chart->setXRange(0.0, 10.0);
    chart->setYRange(ChartAxisSide::Left, 0.0, 10.0);

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);
    const size_t bars=chart->addSeries("bars", ChartSeriesType::Bar);

    chart->setSeriesPoints(line, {{1, 2}, {5, 8}, {9, 4}});
    chart->setSeriesPoints(bars, {{2, 3}, {6, 5}});
    const size_t span=chart->addSpan(3.0, 7.0);
    chart->addSpan(20.0, 30.0);
    const size_t marker=chart->addMarker(4.0, "reset");
    chart->addMarker(-1.0, "before");
    scene.frame();
    scene.frame();

    const skin::textured::Chart &skinChart=scene.skinChart();
    const int z=chart->getZIndex();
    const Rect plotClip=scene.plotClip();
    const std::vector<size_t> &spans=skinChart.spanPrimitives();
    const std::vector<size_t> &markers=skinChart.markerPrimitives();

    REQUIRE(spans.size() == 2);
    REQUIRE(markers.size() == 2);

    // drawn on the first frame: the span at z+2, the marker in front of both
    // series, both in the plot's clip; the ones out of range hidden
    scene.chart->addStateChange(controls::StateChange_Geometry);
    scene.frame();
    Rect rect;
    REQUIRE(chart->getSpanRect(span, rect));
    rect.x+=chart->getRenderBounds().x;
    rect.y+=chart->getRenderBounds().y;
    CHECK((scene.renderer.rects.at(spans[0]) == rect));
    CHECK(scene.renderer.zIndices.at(spans[0]) == z+2);
    CHECK((scene.renderer.clips.at(spans[0]) == plotClip));
    CHECK(scene.renderer.hidden.count(spans[1]) == 1);

    const test::StubRenderer::LineDraw *draw=scene.line(markers[0]);
    REQUIRE(draw != nullptr);
    CHECK(draw->zIndex == z+3+2);
    CHECK(draw->p0.x == draw->p1.x);
    CHECK(draw->p0.x == doctest::Approx(chart->getRenderBounds().x+chart->xPosition(4.0)));
    CHECK((draw->color == tokyonight::ChartMarker.top));
    CHECK((scene.renderer.clips.at(markers[0]) == plotClip));
    CHECK(scene.renderer.hidden.count(markers[1]) == 1);
    // the slots were taken back to front
    CHECK(spans[0] < scene.series(line)[0]);
    CHECK(scene.series(bars).back() < markers[0]);

    scene.frame();
    CHECK(scene.written().empty());

    // a moved marker: its slot, and its caption alone marked
    controls::internal::Text *caption=chart->getMarkerLabel(marker);
    REQUIRE(caption != nullptr);
    CHECK((caption->TextColor() == scene.skin.Colors.Chart.MarkerText));
    const Rect before=caption->getBounds();

    chart->setMarkerX(marker, 6.0);
    scene.renderer.clear();
    scene.canvas->updateCanvas();
    CHECK(caption->getBounds().x > before.x);
    CHECK(caption->getStateChange() != controls::StateChange_Nothing);
    for(controls::Base *child : chart->getChildren())
    {
        if(child != caption)
            CHECK(child->getStateChange() == controls::StateChange_Nothing);
    }
    scene.canvas->renderCanvas();
    CHECK(scene.written() == std::set<size_t>{markers[0]});

    // a moved span: its slot
    chart->setSpan(span, 2.0, 7.0);
    scene.frame();
    CHECK(scene.written() == std::set<size_t>{spans[0]});
    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("Chart renderer: a chart built hidden takes its slots back to front once shown")
{
    Scene scene;
    controls::Chart *chart=scene.chart;

    // hidden before its first frame, with series, a span and a marker
    chart->setHidden(true);
    chart->setXRange(0.0, 10.0);

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);

    chart->setSeriesPoints(line, {{1, 2}, {5, 8}, {9, 4}});
    chart->addSpan(3.0, 7.0);
    chart->addMarker(4.0);
    scene.frame();
    scene.frame();

    chart->setHidden(false);
    scene.frame();

    const skin::textured::Chart &skinChart=scene.skinChart();
    const std::vector<size_t> &grid=skinChart.gridPrimitives();
    const std::vector<size_t> &spans=skinChart.spanPrimitives();
    const std::vector<size_t> &series=scene.series(line);
    const std::vector<size_t> &markers=skinChart.markerPrimitives();

    REQUIRE(!grid.empty());
    REQUIRE(spans.size() == 1);
    REQUIRE(series.size() == 2);
    REQUIRE(markers.size() == 1);
    // what is behind is drawn first: the grid, the span, the line, the marker
    CHECK(*std::max_element(grid.begin(), grid.end()) < spans[0]);
    CHECK(spans[0] < *std::min_element(series.begin(), series.end()));
    CHECK(*std::max_element(series.begin(), series.end()) < markers[0]);
    CHECK(markers[0] < skinChart.crosshairPrimitives().at(0));
}
