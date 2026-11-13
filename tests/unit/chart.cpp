/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Chart's data model and layout, on the headless fixture (a
 *  StubControl for every type, so no renderer): stacked bars stacking by
 *  equal x in series order, positive values up and negative ones down from
 *  0, a series with no point at an x adding nothing there, a hidden series
 *  leaving the stack, and the segments of a stack abutting; unstacked bar
 *  series side by side with the stack as one group, in the order of their
 *  first series, centred on their x; the automatic x range (the visible
 *  series' extent, widened by half the bar interval with a bar series) and
 *  value ranges (nice steps, with or without 0, per side), and fixed ones;
 *  the plot the control less its padding, the value labels' columns and the
 *  x labels' row, a right axis only for a series that names it; the tick
 *  labels pooled, one per tick, showing its label, placed beside the plot,
 *  and not set again by a change that keeps the ticks; a new point laying
 *  nothing out when every range is fixed and the chart once when one is
 *  automatic; a step line's vertices and a NaN's gap; a point outside the
 *  range placed outside the plot and a bar cut to it; the time axis's labels
 *  in the chart's zone, and category labels thinned on a narrow chart, a
 *  tick per category alone however far a point or the range reaches; and
 *  markers and spans: a marker's line across the plot at its x, none outside
 *  the range, its caption right of the line at the plot's top (left of it at
 *  the right edge), hidden out of range and back, clipped to the plot (the
 *  tick labels not), gone with its marker; a span's rect cut to the plot;
 *  neither moving the automatic x range.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/chart.h>
#include <gweni/controls/text.h>

#include <doctest/doctest.h>

#include <cmath>
#include <set>
#include <vector>

using namespace gweni;
using controls::Chart;
using controls::ChartAxisSide;
using controls::ChartPoint;
using controls::ChartSeriesType;
using controls::ChartXAxisType;

namespace
{

struct Scene
{
    explicit Scene(int width=400, int height=240)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        chart=canvas->newChild<Chart>();
        chart->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        chart->setBounds(10, 20, width, height);
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
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    Rect bar(size_t series, size_t index) const
    {
        Rect rect;

        REQUIRE(chart->getBarRect(series, index, rect));
        return rect;
    }

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    Chart *chart=nullptr;
};

double utc(int64_t year, int month, int day, int hour=0)
{
    return static_cast<double>(controls::axis::daysFromCivil(year, month, day))*86400.0+hour*3600.0;
}

}//namespace

TEST_CASE("Chart: stacked bars stack by equal x, a missing x counting as 0")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXAxisType(ChartXAxisType::Value);
    chart->setXRange(-0.5, 2.5);
    chart->setYRange(ChartAxisSide::Left, -5.0, 20.0);

    const size_t a=chart->addSeries("a", ChartSeriesType::StackedBar);
    const size_t b=chart->addSeries("b", ChartSeriesType::StackedBar);
    const size_t c=chart->addSeries("c", ChartSeriesType::StackedBar);
    const size_t d=chart->addSeries("d", ChartSeriesType::StackedBar);

    chart->setSeriesPoints(a, {{0, 1}, {1, 2}, {2, 3}});
    chart->setSeriesPoints(b, {{0, 4}, {2, 5}});      // none at x 1
    chart->setSeriesPoints(c, {{0, 1}, {1, 1}, {2, 1}});
    chart->setSeriesPoints(d, {{1, -2}});
    scene.frame();

    CHECK(chart->getBarGroupCount() == 1);
    CHECK(chart->getStackBase(a, 0) == 0.0);
    CHECK(chart->getStackBase(a, 2) == 0.0);
    CHECK(chart->getStackBase(b, 0) == 1.0);
    CHECK(chart->getStackBase(b, 1) == 3.0);
    CHECK(chart->getStackBase(c, 0) == 5.0);
    CHECK(chart->getStackBase(c, 1) == 2.0);  // a's 2, b's missing 0
    CHECK(chart->getStackBase(c, 2) == 8.0);
    CHECK(chart->getStackBase(d, 0) == 0.0);  // the negative stack starts at 0

    // the segments of a stack abut, the same columns
    const Rect a1=scene.bar(a, 1);
    const Rect c1=scene.bar(c, 1);
    const Rect d1=scene.bar(d, 0);

    CHECK(c1.y+c1.h == a1.y);
    CHECK(d1.y == a1.y+a1.h);
    CHECK(a1.x == c1.x);
    CHECK(a1.w == c1.w);
    CHECK(a1.y+a1.h == static_cast<int>(std::lround(chart->yPosition(ChartAxisSide::Left, 0.0))));
    CHECK(c1.y == static_cast<int>(std::lround(chart->yPosition(ChartAxisSide::Left, 3.0))));

    // a hidden series leaves the stack
    chart->setSeriesVisible(b, false);
    scene.frame();
    CHECK(chart->getStackBase(c, 0) == 1.0);
    CHECK(chart->getStackBase(c, 2) == 3.0);
    Rect hidden;
    CHECK(!chart->getBarRect(b, 0, hidden));
    CHECK(chart->getBarGroup(b) == -1);
}

TEST_CASE("Chart: unstacked bars sit side by side, the stack as one group")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXAxisType(ChartXAxisType::Value);
    chart->setYRange(ChartAxisSide::Left, 0.0, 10.0);

    const size_t a=chart->addSeries("a", ChartSeriesType::Bar);
    const size_t s1=chart->addSeries("s1", ChartSeriesType::StackedBar);
    const size_t b=chart->addSeries("b", ChartSeriesType::Bar);
    const size_t s2=chart->addSeries("s2", ChartSeriesType::StackedBar);

    for(size_t series : {a, s1, b, s2})
        chart->setSeriesPoints(series, {{0, 2}, {1, 3}, {2, 4}, {3, 5}});
    scene.frame();

    CHECK(chart->getBarGroupCount() == 3);
    CHECK(chart->getBarGroup(a) == 0);
    CHECK(chart->getBarGroup(s1) == 1);
    CHECK(chart->getBarGroup(b) == 2);
    CHECK(chart->getBarGroup(s2) == 1);
    CHECK(chart->getBarInterval() == 1.0);
    // half an interval each way for the bars
    CHECK(chart->getXMin() == -0.5);
    CHECK(chart->getXMax() == 3.5);

    const Rect ra=scene.bar(a, 1);
    const Rect rs=scene.bar(s1, 1);
    const Rect rb=scene.bar(b, 1);
    const Rect rs2=scene.bar(s2, 1);

    CHECK(ra.x+ra.w < rs.x);  // a unit between them
    CHECK(rs.x+rs.w < rb.x);
    CHECK(rs2.x == rs.x);
    CHECK(rs2.y+rs2.h == rs.y);
    // the groups centred on their x, within a unit
    CHECK(std::fabs((ra.x+rb.x+rb.w)/2.0-chart->xPosition(1.0)) <= 1.0);
    // and inside their interval
    const double band=chart->xPosition(1.5)-chart->xPosition(0.5);
    CHECK(rb.x+rb.w-ra.x <= band);

    chart->setSeriesVisible(a, false);
    scene.frame();
    CHECK(chart->getBarGroupCount() == 2);
    CHECK(chart->getBarGroup(s1) == 0);
    CHECK(chart->getBarGroup(b) == 1);
}

TEST_CASE("Chart: automatic and fixed ranges")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXAxisType(ChartXAxisType::Value);

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);

    chart->setSeriesPoints(line, {{6, 9}, {2, 5}, {4, 15}});   // sorted when set
    scene.frame();

    REQUIRE(chart->getSeriesPoints(line).size() == 3);
    CHECK(chart->getSeriesPoints(line)[0].x == 2.0);
    CHECK(chart->getXMin() == 2.0);
    CHECK(chart->getXMax() == 6.0);
    CHECK(chart->getYMin(ChartAxisSide::Left) == 0.0);
    CHECK(chart->getYMax(ChartAxisSide::Left) >= 15.0);

    const double step=chart->getYStep(ChartAxisSide::Left);
    CHECK(std::fabs(std::remainder(chart->getYMax(ChartAxisSide::Left), step)) < 1e-9);

    chart->setYIncludesZero(ChartAxisSide::Left, false);
    scene.frame();
    CHECK(chart->getYMin(ChartAxisSide::Left) > 0.0);
    CHECK(chart->getYMin(ChartAxisSide::Left) <= 5.0);
    CHECK(chart->getYMax(ChartAxisSide::Left) >= 15.0);

    // a bar series widens the x range by half its interval
    const size_t bars=chart->addSeries("bars", ChartSeriesType::Bar);
    chart->setSeriesPoints(bars, {{2, 1}, {4, 1}});
    scene.frame();
    CHECK(chart->getBarInterval() == 2.0);
    CHECK(chart->getXMin() == 1.0);
    CHECK(chart->getXMax() == 7.0);
    // and brings 0 into its axis
    CHECK(chart->getYMin(ChartAxisSide::Left) == 0.0);

    // the right axis has its own range
    CHECK(!chart->isYAxisShown(ChartAxisSide::Right));
    const size_t right=chart->addSeries("right", ChartSeriesType::Line, ChartAxisSide::Right);
    chart->setSeriesPoints(right, {{3, 100}, {5, 300}});
    scene.frame();
    CHECK(chart->isYAxisShown(ChartAxisSide::Right));
    CHECK(chart->getYMax(ChartAxisSide::Right) >= 300.0);
    CHECK(chart->getYMax(ChartAxisSide::Left) < 100.0);

    // fixed
    chart->setXRange(0.0, 10.0);
    chart->setYRange(ChartAxisSide::Left, -1.0, 4.0);
    scene.frame();
    CHECK(chart->getXMin() == 0.0);
    CHECK(chart->getXMax() == 10.0);
    CHECK(chart->getYMin(ChartAxisSide::Left) == -1.0);
    CHECK(chart->getYMax(ChartAxisSide::Left) == 4.0);
    for(const controls::axis::Tick &tick : chart->getYTicks(ChartAxisSide::Left))
    {
        CHECK(tick.value >= -1.0);
        CHECK(tick.value <= 4.0);
    }

    chart->setXAutoRange();
    scene.frame();
    CHECK(chart->getXMin() == 1.0);
}

TEST_CASE("Chart: the plot is the control less its labels, which are pooled")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXAxisType(ChartXAxisType::Value);

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);

    chart->setSeriesPoints(line, {{0, 0}, {50, 80}, {100, 40}});
    scene.frame();

    const Rect plot=chart->getPlotBounds();
    const Padding &padding=chart->getPadding();

    CHECK(plot.x > padding.left);
    CHECK(plot.y > padding.top);
    CHECK(plot.x+plot.w == chart->getWidth()-padding.right);
    CHECK(plot.y+plot.h < chart->getHeight()-padding.bottom);

    const std::vector<controls::internal::Text *> &yLabels=chart->getYLabels(ChartAxisSide::Left);
    const std::vector<controls::axis::Tick> &yTicks=chart->getYTicks(ChartAxisSide::Left);

    REQUIRE(yTicks.size() >= 2);
    REQUIRE(yLabels.size() == yTicks.size());
    int widest=0;
    for(size_t i=0; i < yTicks.size(); ++i)
    {
        CHECK(yLabels[i]->getText() == yTicks[i].label);
        CHECK(yLabels[i]->getBounds().x+yLabels[i]->getBounds().w == plot.x-Chart::TickGap);
        widest=std::max(widest, yLabels[i]->getBounds().w);
    }
    CHECK(plot.x == padding.left+widest+Chart::TickGap);
    // the top tick at the plot's top, the bottom one at its bottom
    CHECK(yTicks.front().position == doctest::Approx(plot.y+plot.h));
    CHECK(yTicks.back().position == doctest::Approx(plot.y));

    const std::vector<controls::internal::Text *> &xLabels=chart->getXLabels();
    const std::vector<controls::axis::Tick> &xTicks=chart->getXTicks();

    REQUIRE(xTicks.size() >= 2);
    REQUIRE(xLabels.size() == xTicks.size());
    for(size_t i=0; i < xTicks.size(); ++i)
    {
        CHECK(xLabels[i]->getText() == xTicks[i].label);
        CHECK(xLabels[i]->getBounds().y == plot.y+plot.h+Chart::TickGap);
        if(i > 0)
            CHECK(xLabels[i]->getBounds().x >= xLabels[i-1]->getBounds().x+xLabels[i-1]->getBounds().w+Chart::XLabelGap);
    }

    // a change that keeps the ticks sets no label again
    const std::vector<controls::internal::Text *> before=yLabels;

    chart->setPointValue(line, 1, 79.0);
    scene.canvas->updateCanvas();
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    CHECK(chart->getYLabels(ChartAxisSide::Left) == before);
    for(controls::internal::Text *label : chart->getYLabels(ChartAxisSide::Left))
        CHECK((label->getStateChange()&controls::StateChange_Text) == 0);
    for(controls::internal::Text *label : chart->getXLabels())
        CHECK((label->getStateChange()&controls::StateChange_Text) == 0);
    scene.canvas->renderCanvas();

    // one that moves the range sets the labels it changes
    chart->setPointValue(line, 1, 800.0);
    scene.canvas->updateCanvas();
    CHECK(chart->getYMax(ChartAxisSide::Left) >= 800.0);
    bool set=false;
    for(controls::internal::Text *label : chart->getYLabels(ChartAxisSide::Left))
        set=set || (label->getStateChange()&controls::StateChange_Text) != 0;
    CHECK(set);
    scene.canvas->renderCanvas();

    // a right axis takes its column off the plot
    const size_t right=chart->addSeries("right", ChartSeriesType::Line, ChartAxisSide::Right);

    chart->setSeriesPoints(right, {{0, 1}, {100, 2}});
    scene.frame();
    CHECK(chart->getPlotBounds().x+chart->getPlotBounds().w < chart->getWidth()-padding.right);
    REQUIRE(!chart->getYLabels(ChartAxisSide::Right).empty());
    CHECK(chart->getYLabels(ChartAxisSide::Right)[0]->getBounds().x
          == chart->getPlotBounds().x+chart->getPlotBounds().w+Chart::TickGap);
}

TEST_CASE("Chart: a new point lays nothing out when every range is fixed")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXAxisType(ChartXAxisType::Value);
    chart->setXRange(0.0, 100.0);
    chart->setYRange(ChartAxisSide::Left, 0.0, 10.0);
    chart->setYRange(ChartAxisSide::Right, 0.0, 10.0);

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);

    chart->setSeriesPoints(line, {{0, 1}, {1, 2}});
    scene.frame();
    scene.frame();

    chart->appendPoint(line, 2.0, 3.0);
    scene.canvas->updateCanvas();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK((chart->getStateChange()&controls::StateChange_Background) != 0);
    scene.canvas->renderCanvas();
    CHECK(chart->getSeriesPoints(line).size() == 3);
    CHECK(chart->getPointPosition(line, 2).x == doctest::Approx(chart->xPosition(2.0)));

    // an automatic range lays the chart out again
    chart->setYAutoRange(ChartAxisSide::Left);
    scene.frame();
    chart->appendPoint(line, 3.0, 4.0);
    scene.canvas->updateCanvas();
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    scene.canvas->renderCanvas();

    // a point out of order goes in its place
    chart->appendPoint(line, 1.5, 9.0);
    REQUIRE(chart->getSeriesPoints(line).size() == 5);
    CHECK(chart->getSeriesPoints(line)[2] == ChartPoint(1.5, 9.0));
}

TEST_CASE("Chart: a step line holds each value to the next x")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXAxisType(ChartXAxisType::Value);

    const size_t step=chart->addSeries("step", ChartSeriesType::StepLine);
    const size_t line=chart->addSeries("line", ChartSeriesType::Line);

    chart->setSeriesPoints(step, {{0, 1}, {1, 3}, {2, NAN}, {3, 2}});
    chart->setSeriesPoints(line, {{0, 1}, {3, 2}});
    scene.frame();

    REQUIRE(chart->getVertexCount(step) == 7);
    CHECK(chart->getVertexCount(line) == 2);

    PointF point;

    REQUIRE(chart->getVertex(step, 1, point));
    CHECK(point.x == doctest::Approx(chart->xPosition(1.0)));
    CHECK(point.y == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 1.0)));
    REQUIRE(chart->getVertex(step, 2, point));
    CHECK(point.y == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 3.0)));
    REQUIRE(chart->getVertex(step, 3, point));
    CHECK(point.x == doctest::Approx(chart->xPosition(2.0)));
    // the NaN's vertices are a gap
    CHECK(!chart->getVertex(step, 4, point));
    CHECK(!chart->getVertex(step, 5, point));
    CHECK(chart->getVertex(step, 6, point));
    CHECK(!chart->getVertex(step, 7, point));
}

TEST_CASE("Chart: a point outside the range is outside the plot, a bar cut to it")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXAxisType(ChartXAxisType::Value);
    chart->setXRange(0.0, 10.0);
    chart->setYRange(ChartAxisSide::Left, 5.0, 10.0);

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);
    const size_t bars=chart->addSeries("bars", ChartSeriesType::Bar);

    chart->setSeriesPoints(line, {{-5, 20}, {5, 7}});
    chart->setSeriesPoints(bars, {{2, 20}, {4, 3}});
    scene.frame();

    const Rect plot=chart->getPlotBounds();
    const PointF far=chart->getPointPosition(line, 0);

    CHECK(far.y < plot.y);
    CHECK(far.x < plot.x);
    CHECK(far.y == doctest::Approx(chart->yPosition(ChartAxisSide::Left, 20.0)));

    const Rect tall=scene.bar(bars, 0);
    CHECK(tall.y == plot.y);
    CHECK(tall.y+tall.h == plot.y+plot.h);
    Rect below;
    CHECK(!chart->getBarRect(bars, 1, below));    // 0..3, under the range
}

TEST_CASE("Chart: time labels in the chart's zone, category labels thinned")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXAxisType(ChartXAxisType::Time);
    chart->setUtcOffset(controls::axis::fixedUtcOffset(0));
    chart->setXRange(utc(2024, 3, 1), utc(2024, 3, 2));

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);

    chart->setSeriesPoints(line, {{utc(2024, 3, 1, 3), 1}, {utc(2024, 3, 1, 20), 2}});
    scene.frame();

    const std::vector<controls::axis::Tick> &ticks=chart->getXTicks();

    REQUIRE(ticks.size() >= 3);
    CHECK(ticks.front().label == "Mar 1");
    CHECK(ticks.back().label == "Mar 2");
    CHECK(chart->getTimeStep() < controls::axis::TimeStep::Day1);
    CHECK(ticks.front().position == doctest::Approx(chart->getPlotBounds().x));

    // an hour east: the ticks on that zone's whole hours, its midnight at
    // 23:00 UTC labelled with the new day
    chart->setUtcOffset(controls::axis::fixedUtcOffset(3600));
    scene.frame();
    REQUIRE(!chart->getXTicks().empty());
    bool midnight=false;
    for(const controls::axis::Tick &tick : chart->getXTicks())
    {
        CHECK(std::fmod(tick.value+3600.0, 3600.0) == 0.0);
        if(tick.label == "Mar 2")
        {
            CHECK(tick.value == utc(2024, 3, 1, 23));
            midnight=true;
        }
    }
    CHECK(midnight);
    REQUIRE(chart->getXLabels().size() == chart->getXTicks().size());
    CHECK(chart->getXLabels().back()->getText() == chart->getXTicks().back().label);

    // categories, one tick each, thinned when the chart is narrow
    chart->setXAxisType(ChartXAxisType::Category);
    chart->setXAutoRange();
    chart->setCategories({"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"});
    chart->setSeriesPoints(line, {{0, 1}, {6, 2}});
    scene.frame();
    CHECK(chart->getXMin() == -0.5);
    CHECK(chart->getXMax() == 6.5);
    REQUIRE(chart->getXTicks().size() == 7);
    CHECK(chart->getXTicks()[3].label == "Thursday");
    CHECK(chart->getXTicks()[3].position == doctest::Approx(chart->xPosition(3.0)));

    chart->setBounds(10, 20, 160, 120);
    scene.frame();
    const size_t shown=chart->getXTicks().size();
    CHECK(shown < 7);
    CHECK(shown > 0);
    CHECK(chart->getXLabels().size() == shown);
    CHECK(chart->getXTicks()[0].label == "Monday");

    // a tick per category alone, however far a point or the range reaches
    chart->setBounds(10, 20, 500, 240);
    chart->setCategories({"a", "b", "c"});
    chart->setSeriesPoints(line, {{0, 1}, {100000, 2}});
    scene.frame();
    CHECK(chart->getXMax() == 100000.5);
    CHECK(chart->getXTicks().size() <= 3);
    chart->setSeriesPoints(line, {{0, 1}, {2, 2}});
    chart->setXRange(-1e5, 1e5);
    scene.frame();
    CHECK(chart->getXTicks().size() <= 3);
    chart->setCategories({});
    scene.frame();
    CHECK(chart->getXTicks().empty());
}

TEST_CASE("Chart: a resize, an axis change and new data lay out in one pass, as a full pass does")
{
    const bool verify=controls::layoutVerifyEnabled();
    const size_t failures=controls::layoutVerifyFailures();

    controls::setLayoutVerify(true);
    {
        Scene scene;
        Chart *chart=scene.chart;

        chart->setXAxisType(ChartXAxisType::Value);
        chart->setYTitle(ChartAxisSide::Left, "MB");
        chart->setXTitle("seconds");

        const size_t line=chart->addSeries("line", ChartSeriesType::Line);
        const size_t right=chart->addSeries("right", ChartSeriesType::StepLine, ChartAxisSide::Right);

        chart->setSeriesPoints(line, {{0, 1}, {10, 5}, {20, 3}});
        chart->setSeriesPoints(right, {{0, 100}, {20, 900}});

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
        const size_t labels=chart->getYLabels(ChartAxisSide::Left).size();

        chart->setBounds(10, 20, 600, 400);
        onePass();
        // a taller plot: more ticks
        CHECK(chart->getYLabels(ChartAxisSide::Left).size() > labels);

        chart->setBounds(10, 20, 300, 160);
        onePass();
        chart->setPointValue(right, 1, 5000.0);
        onePass();
        chart->setYTitle(ChartAxisSide::Right, "requests");
        onePass();
        chart->setXAxisType(ChartXAxisType::Time);
        onePass();
        chart->setXAxisType(ChartXAxisType::Category);
        chart->setCategories({"a", "b", "c"});
        onePass();
    }
    CHECK(controls::layoutVerifyFailures() == failures);
    controls::setLayoutVerify(verify);
}

TEST_CASE("Chart: markers and spans sit on the plot at their x and count in no range")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setXAxisType(ChartXAxisType::Value);

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);

    chart->setSeriesPoints(line, {{0, 1}, {10, 2}});
    const size_t middle=chart->addMarker(5.0, "reset");
    const size_t edge=chart->addMarker(9.9, "late");
    const size_t outside=chart->addMarker(20.0, "never");
    const size_t bare=chart->addMarker(2.0);
    const size_t span=chart->addSpan(8.0, 3.0);    // either order
    const size_t wide=chart->addSpan(-50.0, 1.0);
    const size_t gone=chart->addSpan(11.0, 12.0);
    scene.frame();

    // the x range is the series' alone
    CHECK(chart->getXMin() == 0.0);
    CHECK(chart->getXMax() == 10.0);

    const Rect plot=chart->getPlotBounds();
    PointF top;
    PointF bottom;

    REQUIRE(chart->getMarkerLine(middle, top, bottom));
    CHECK(top.x == doctest::Approx(chart->xPosition(5.0)));
    CHECK(top.y == float(plot.y));
    CHECK(bottom.x == top.x);
    CHECK(bottom.y == float(plot.y+plot.h));
    CHECK(!chart->getMarkerLine(outside, top, bottom));

    // captions: right of the line at the plot's top, left of it at the
    // right edge, none outside the range or without a caption
    controls::internal::Text *reset=chart->getMarkerLabel(middle);
    REQUIRE(reset != nullptr);
    CHECK(reset->getText() == "reset");
    CHECK(reset->visible());
    CHECK(reset->getBounds().x == int(std::floor(chart->xPosition(5.0)))+Chart::CaptionGap);
    CHECK(reset->getBounds().y == plot.y+Chart::CaptionInset);
    controls::internal::Text *late=chart->getMarkerLabel(edge);
    REQUIRE(late != nullptr);
    CHECK(late->getBounds().x+late->getBounds().w == int(std::floor(chart->xPosition(9.9)))-Chart::CaptionGap);
    REQUIRE(chart->getMarkerLabel(outside) != nullptr);
    CHECK(!chart->getMarkerLabel(outside)->visible());
    CHECK(chart->getMarkerLabel(bare) == nullptr);

    // a caption is clipped to the plot, a tick label is not
    REQUIRE((chart->getChildClipRect(reset) != nullptr));
    CHECK((*chart->getChildClipRect(reset) == plot));
    REQUIRE(!chart->getXLabels().empty());
    CHECK((chart->getChildClipRect(chart->getXLabels()[0]) == nullptr));

    // spans: whole units across the plot's height, cut to it
    Rect rect;

    REQUIRE(chart->getSpanRect(span, rect));
    CHECK(chart->getSpanStart(span) == 3.0);
    CHECK(rect.x == int(std::lround(chart->xPosition(3.0))));
    CHECK(rect.x+rect.w == int(std::lround(chart->xPosition(8.0))));
    CHECK(rect.y == plot.y);
    CHECK(rect.h == plot.h);
    REQUIRE(chart->getSpanRect(wide, rect));
    CHECK(rect.x == plot.x);
    CHECK(!chart->getSpanRect(gone, rect));

    // a caption moved out of range hides, and comes back
    chart->setMarkerX(middle, -3.0);
    scene.frame();
    CHECK(!reset->visible());
    chart->setMarkerX(middle, 5.0);
    scene.frame();
    CHECK(reset->visible());

    // removed, its caption goes
    const size_t labels=chart->getChildren().size();
    chart->removeMarker(middle);
    scene.frame();
    CHECK(chart->getChildren().size() == labels-1);
    CHECK(chart->getMarkerCount() == 3);
    CHECK(chart->getMarkerCaption(0) == "late");
}
