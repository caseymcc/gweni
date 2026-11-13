/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Chart's hover and click, on the headless fixture with the
 *  textured Chart renderer (a StubControl for the rest), the mouse through the
 *  input handler. The hit test: the hovered x is the visible point's x
 *  nearest the mouse among every series (each searched in its own sorted x)
 *  and in the x range (a point past the range's edge never takes the hover
 *  from one inside it), each series' index there or -1 (none there, or
 *  hidden), nothing outside the plot; a click's target a bar holding the point, the upper of two
 *  stacked segments by its own rect, else the line point nearest the mouse
 *  within PointHitRadius, else nothing. The hover: a frame of it writes the
 *  crosshair's slot alone, drawn across the plot at the hovered x at the
 *  chart's z+4+S in ChartCrosshair's colour; the tooltip is the chart's own
 *  text (the x's label, on a time axis the point's date and its time of day,
 *  January 1st's too, and each series' value there), the app's text or the
 *  app's control filled for the hover, set on the chart before the canvas
 *  reads it and placed right of the crosshair at the plot's top
 *  (getToolTipAnchor()); a move within the same x marks and writes nothing; a
 *  new point under a resting mouse gives the tooltip its value; leaving the
 *  plot hides the crosshair and the tooltip. onPointClicked fires for a bar,
 *  a stacked segment and a point near the mouse with the series and index,
 *  and not for a press and release on two different points, nor for empty
 *  plot; a series removed between a press on a point and its release fires
 *  nothing and gives the mouse back on the release. A hover control replaced while the box shows it is given back by
 *  the box before the chart deletes it, and the box shows the new one.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/chart.h>
#include <gweni/controls/label.h>
#include <gweni/controls/toolTip.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <cmath>
#include <set>
#include <string>
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
    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        chart=canvas->newChild<Chart>();
        chart->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        chart->setBounds(10, 20, 400, 240);
        chart->setLegendVisible(false);
        chart->setXAxisType(ChartXAxisType::Value);
        chart->setXRange(-0.5, 9.5);
        chart->setYRange(ChartAxisSide::Left, 0.0, 10.0);
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
        renderer.lines.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! The chart's point (x in data, y in data on the left axis), in its own
    //! coordinates.
    Point local(double x, double y) const
    {
        return Point(int(std::floor(chart->xPosition(x))), int(std::floor(chart->yPosition(ChartAxisSide::Left, y))));
    }

    void moveTo(const Point &local)
    {
        const Point at=chart->localPosToCanvas(local);

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

    void clicked(event::Info info)
    {
        clicks.push_back({info.integer, size_t(info.point.x)});
    }

    std::set<size_t> written() const
    {
        const std::vector<size_t> &ids=renderer.writtenPrimitiveIds();

        return std::set<size_t>(ids.begin(), ids.end());
    }

    const skin::textured::Chart &skinChart() const
    {
        return *static_cast<const skin::textured::Chart *>(chart->getSkinControl());
    }

    struct Click
    {
        size_t series;
        size_t index;
        bool operator==(const Click &other) const { return series == other.series && index == other.index; }
    };

    test::StubRenderer renderer;
    ChartSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    Chart *chart=nullptr;
    float clock=10.0f;
    std::vector<Click> clicks;
};

}//namespace

TEST_CASE("Chart hover: the hit test finds the nearest x, a bar, a stacked segment and a point")
{
    Scene scene;
    Chart *chart=scene.chart;

    const size_t odd=chart->addSeries("odd", ChartSeriesType::Line);
    const size_t even=chart->addSeries("even", ChartSeriesType::Line);
    const size_t hidden=chart->addSeries("hidden", ChartSeriesType::Line);

    chart->setSeriesPoints(odd, {{1, 2}, {3, 4}, {5, 6}, {7, 8}});
    chart->setSeriesPoints(even, {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}});
    chart->setSeriesPoints(hidden, {{3.4, 5}});
    chart->setSeriesVisible(hidden, false);
    scene.frame();

    ChartHover hover;

    // the nearest x among every visible series
    REQUIRE(chart->hoverAt(scene.local(2.9, 5), hover));
    CHECK(hover.x == 3.0);
    REQUIRE(hover.points.size() == 3);
    CHECK(hover.points[odd] == 1);
    CHECK(hover.points[even] == -1);
    CHECK(hover.points[hidden] == -1);
    REQUIRE(chart->hoverAt(scene.local(3.6, 5), hover));
    CHECK(hover.x == 4.0);
    CHECK(hover.points[even] == 2);
    CHECK(hover.points[odd] == -1);
    REQUIRE(chart->hoverAt(scene.local(9.4, 5), hover));
    CHECK(hover.x == 8.0);
    // nothing outside the plot
    const Rect plot=chart->getPlotBounds();
    CHECK(!chart->hoverAt(Point(plot.x-1, plot.y+5), hover));
    CHECK(!chart->hoverAt(Point(plot.x+5, plot.y+plot.h), hover));

    // a click: the line point within the radius, nothing further away
    size_t series=0;
    size_t index=0;
    const PointF on=chart->getPointPosition(odd, 2);

    REQUIRE(chart->pointAt(Point(int(on.x)+3, int(on.y)-3), series, index));
    CHECK(series == odd);
    CHECK(index == 2);
    CHECK(!chart->pointAt(Point(int(on.x), int(on.y)-Chart::PointHitRadius-4), series, index));

    // bars and a stack
    const size_t bars=chart->addSeries("bars", ChartSeriesType::Bar);
    const size_t low=chart->addSeries("low", ChartSeriesType::StackedBar);
    const size_t high=chart->addSeries("high", ChartSeriesType::StackedBar);

    chart->setSeriesPoints(bars, {{1, 3}, {5, 4}});
    chart->setSeriesPoints(low, {{1, 2}, {5, 1}});
    chart->setSeriesPoints(high, {{1, 3}, {5, 2}});
    scene.frame();

    Rect rect;

    REQUIRE(chart->getBarRect(bars, 1, rect));
    REQUIRE(chart->pointAt(Point(rect.x+1, rect.y+rect.h-2), series, index));
    CHECK(series == bars);
    CHECK(index == 1);

    // the stack at x 5: low 0..1, high 1..3
    REQUIRE(chart->getBarRect(high, 1, rect));
    REQUIRE(chart->pointAt(Point(rect.x+rect.w/2, rect.y+1), series, index));
    CHECK(series == high);
    CHECK(index == 1);
    REQUIRE(chart->getBarRect(low, 1, rect));
    REQUIRE(chart->pointAt(Point(rect.x+rect.w/2, rect.y+rect.h-1), series, index));
    CHECK(series == low);
    CHECK(index == 1);
    // above the stack, and away from any line point: nothing
    CHECK(!chart->pointAt(scene.local(5.1, 9.6), series, index));
}

TEST_CASE("Chart hover: the crosshair's slot, the tooltip, and nothing for a move within one x")
{
    Scene scene;
    Chart *chart=scene.chart;

    const size_t cpu=chart->addSeries("cpu", ChartSeriesType::Line);
    const size_t mem=chart->addSeries("mem", ChartSeriesType::StepLine);

    chart->setSeriesPoints(cpu, {{1, 2}, {3, 4}, {5, 6}});
    chart->setSeriesPoints(mem, {{1, 7}, {5, 8}});
    chart->addMarker(4.0, "m");
    scene.frame();
    scene.frame();
    CHECK(scene.written().empty());
    CHECK(!chart->hasToolTip());

    const std::vector<size_t> &crosshair=scene.skinChart().crosshairPrimitives();
    REQUIRE(crosshair.size() == 1);

    // onto x 3: the crosshair's slot alone
    scene.moveTo(scene.local(3.1, 5));
    CHECK(chart->isHovering());
    CHECK(chart->getHover().x == 3.0);
    CHECK(scene.written() == std::set<size_t>{crosshair[0]});
    REQUIRE(scene.renderer.lines.size() == 1);
    const test::StubRenderer::LineDraw &line=scene.renderer.lines[0];
    CHECK(line.p0.x == line.p1.x);
    CHECK(line.p0.x == doctest::Approx(chart->getRenderBounds().x+chart->xPosition(3.0)));
    CHECK(line.zIndex == chart->getZIndex()+4+2);
    CHECK((line.color == tokyonight::ChartCrosshair.top));
    CHECK(chart->getToolTipText() == "3  cpu: 4");

    // the box goes right of the crosshair, at the plot's top
    const Rect anchor=chart->getToolTipAnchor();
    const Point top=chart->localPosToCanvas(Point(0, chart->getPlotBounds().y));
    CHECK(anchor.y == top.y);
    CHECK(anchor.x > chart->localPosToCanvas(Point(int(chart->xPosition(3.0)), 0)).x);

    // within the same x: nothing marked, nothing written
    const Point other=scene.local(2.8, 2);
    const Point at=chart->localPosToCanvas(other);
    scene.canvas->inputMouseMoved(at.x, at.y, 0, 0);
    CHECK(chart->getStateChange() == controls::StateChange_Nothing);
    scene.frame();
    CHECK(scene.written().empty());

    // to x 5: both series there
    scene.moveTo(scene.local(4.9, 5));
    CHECK(scene.written() == std::set<size_t>{crosshair[0]});
    CHECK(chart->getToolTipText() == "5  cpu: 6  mem: 8");

    // a value changed under the resting mouse: the tooltip follows
    chart->setPointValue(cpu, 2, 6.5);
    scene.frame();
    CHECK(chart->getToolTipText() == "5  cpu: 6.5  mem: 8");

    // off the plot: no crosshair, no tooltip
    scene.moveTo(Point(chart->getPlotBounds().x-2, chart->getPlotBounds().y+3));
    CHECK(!chart->isHovering());
    CHECK(!chart->hasToolTip());
    CHECK(scene.written() == std::set<size_t>{crosshair[0]});
    PointF a;
    PointF b;
    CHECK(!chart->getCrosshairLine(a, b));
}

TEST_CASE("Chart hover: the app's text or control fills the tooltip for each hover")
{
    Scene scene;
    Chart *chart=scene.chart;

    const size_t cpu=chart->addSeries("cpu", ChartSeriesType::Line);
    const size_t mem=chart->addSeries("mem", ChartSeriesType::Line);

    chart->setSeriesPoints(cpu, {{1, 2}, {3, 4}});
    chart->setSeriesPoints(mem, {{3, 5}});

    std::vector<ChartHover> asked;

    chart->setHoverText([&asked](const Chart &, const ChartHover &hover)
        {
            asked.push_back(hover);
            return String("at ")+std::to_string(int(hover.x));
        });
    scene.frame();

    scene.moveTo(scene.local(1.2, 3));
    REQUIRE(asked.size() == 1);
    CHECK(asked[0].x == 1.0);
    CHECK(asked[0].points == std::vector<std::ptrdiff_t>{0, -1});
    CHECK(chart->getToolTipText() == "at 1");
    scene.moveTo(scene.local(1.3, 6));
    CHECK(asked.size() == 1);
    scene.moveTo(scene.local(3.0, 6));
    REQUIRE(asked.size() == 2);
    CHECK(asked[1].points == std::vector<std::ptrdiff_t>{1, 0});

    // a control, filled per hover, as the tooltip
    controls::Label *label=chart->newChild<controls::Label>();
    std::vector<double> filled;

    chart->setHoverControl(label, [&filled](const Chart &, const ChartHover &hover, controls::Base *control)
        {
            filled.push_back(hover.x);
            static_cast<controls::Label *>(control)->setText("x "+std::to_string(int(hover.x)));
        });
    CHECK(chart->getToolTip() == label);
    REQUIRE(filled.size() == 1);
    CHECK(filled[0] == 3.0);
    scene.moveTo(scene.local(1.0, 6));
    REQUIRE(filled.size() == 2);
    CHECK(label->getText() == "x 1");
    CHECK(chart->getToolTip() == label);

    scene.moveTo(Point(chart->getPlotBounds().x-2, chart->getPlotBounds().y+3));
    CHECK(chart->getToolTip() == nullptr);
    CHECK(!chart->hasToolTip());
}

TEST_CASE("Chart hover: a click on a bar, a stacked segment and near a point fires onPointClicked")
{
    Scene scene;
    Chart *chart=scene.chart;

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);
    const size_t low=chart->addSeries("low", ChartSeriesType::StackedBar);
    const size_t high=chart->addSeries("high", ChartSeriesType::StackedBar);

    chart->setSeriesPoints(line, {{1, 8}, {3, 9}, {5, 8}});
    chart->setSeriesPoints(low, {{2, 2}, {4, 1}});
    chart->setSeriesPoints(high, {{2, 3}, {4, 2}});
    scene.frame();

    Rect rect;

    REQUIRE(chart->getBarRect(high, 0, rect));
    scene.click(Point(rect.x+rect.w/2, rect.y+1));
    REQUIRE(chart->getBarRect(low, 1, rect));
    scene.click(Point(rect.x+rect.w/2, rect.y+rect.h-2));
    const PointF point=chart->getPointPosition(line, 1);
    scene.click(Point(int(point.x)-2, int(point.y)+2));

    REQUIRE(scene.clicks.size() == 3);
    CHECK(scene.clicks[0] == Scene::Click{high, 0});
    CHECK(scene.clicks[1] == Scene::Click{low, 1});
    CHECK(scene.clicks[2] == Scene::Click{line, 1});
    CHECK(chart->getClickedSeries() == line);
    CHECK(chart->getClickedIndex() == 1);

    // empty plot: nothing
    scene.click(scene.local(6.0, 4.0));
    CHECK(scene.clicks.size() == 3);

    // pressed on one point and let go on another: nothing
    const PointF first=chart->getPointPosition(line, 0);
    const PointF last=chart->getPointPosition(line, 2);
    scene.moveTo(Point(int(first.x), int(first.y)));
    scene.canvas->inputMouseButton(0, true);
    scene.moveTo(Point(int(last.x), int(last.y)));
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(scene.clicks.size() == 3);
}

TEST_CASE("Chart hover: a series removed between a press and its release gives the mouse back")
{
    Scene scene;
    Chart *chart=scene.chart;

    const size_t line=chart->addSeries("line", ChartSeriesType::Line);
    const size_t other=chart->addSeries("other", ChartSeriesType::Line);

    chart->setSeriesPoints(line, {{1, 8}, {3, 9}, {5, 8}});
    chart->setSeriesPoints(other, {{2, 2}});
    scene.frame();

    // the press on a point holds the mouse until its release
    const PointF point=chart->getPointPosition(line, 1);
    scene.moveTo(Point(int(point.x), int(point.y)));
    scene.canvas->inputMouseButton(0, true);
    REQUIRE(MouseFocus == chart);

    // the app drops a series meanwhile: no click, and the mouse is the
    // canvas's again, so a press elsewhere goes where it is
    chart->removeSeries(other);
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(scene.clicks.empty());
    CHECK(MouseFocus == nullptr);

    // and so does clearSeries() between a press and its release
    scene.moveTo(Point(int(point.x), int(point.y)));
    scene.canvas->inputMouseButton(0, true);
    REQUIRE(MouseFocus == chart);
    chart->clearSeries();
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(scene.clicks.empty());
    CHECK(MouseFocus == nullptr);
}

TEST_CASE("Chart hover: a new hover control replaces the one the tooltip box shows, which may then go")
{
    Scene scene;
    Chart *chart=scene.chart;

    const size_t cpu=chart->addSeries("cpu", ChartSeriesType::Line);
    chart->setSeriesPoints(cpu, {{1, 2}, {3, 4}, {5, 6}});

    controls::Label *first=chart->newChild<controls::Label>();
    first->setText("first");
    first->sizeToContents();
    chart->setHoverControl(first, [](const Chart &, const ChartHover &, controls::Base *) {});
    scene.frame();

    // shown in the canvas's box once the delay has passed
    scene.moveTo(scene.local(3, 4));
    scene.clock+=scene.canvas->getToolTipDelay()+0.1f;
    scene.frame();
    controls::ToolTip *box=scene.canvas->toolTipControl();
    REQUIRE(box != nullptr);
    REQUIRE(scene.canvas->toolTipShown());
    REQUIRE(box->content() == first);
    const size_t firstId=first->getControlId();

    // the chart deletes the one it replaces: the box gives it back first
    controls::Label *second=chart->newChild<controls::Label>();
    second->setText("second");
    second->sizeToContents();
    chart->setHoverControl(second, [](const Chart &, const ChartHover &, controls::Base *) {});
    CHECK(controls::Base::getControlById(firstId) == nullptr);
    REQUIRE(box->content() == nullptr);

    // and shows the new one at the next frame
    scene.frame();
    CHECK(scene.canvas->toolTipShown());
    CHECK(box->content() == second);
}

TEST_CASE("Chart hover: only points in the x range are hovered, so the plot's edges have no dead band")
{
    Scene scene;
    Chart *chart=scene.chart;

    // a sliding window: a point either side of the range [10, 20]
    const size_t line=chart->addSeries("line", ChartSeriesType::Line);

    chart->setSeriesPoints(line, {{9.6, 1}, {11, 2}, {19, 3}, {20.4, 4}});
    chart->setXRange(10.0, 20.0);
    scene.frame();

    ChartHover hover;

    // 9.6 is nearer, but outside the range: the hover is 11's
    REQUIRE(chart->hoverAt(scene.local(10.1, 5), hover));
    CHECK(hover.x == 11.0);
    CHECK(hover.points[line] == 1);
    // and at the right edge 19's, not 20.4's
    REQUIRE(chart->hoverAt(scene.local(19.9, 5), hover));
    CHECK(hover.x == 19.0);
    CHECK(hover.points[line] == 2);

    // a series with only points outside the range gives no hover
    chart->setSeriesPoints(line, {{9.6, 1}, {20.4, 4}});
    scene.frame();
    CHECK(!chart->hoverAt(scene.local(10.1, 5), hover));
    CHECK(!chart->hoverAt(scene.local(19.9, 5), hover));
}

TEST_CASE("Chart hover: a time axis's tooltip is the point's date and time of day, January 1st included")
{
    Scene scene;
    Chart *chart=scene.chart;
    // 2027-01-01 00:00 UTC
    const double newYear=1798761600.0;

    chart->setXAxisType(ChartXAxisType::Time);
    chart->setUtcOffset([](double) { return 0; });
    chart->setXRange(newYear-86400.0, newYear+2*86400.0);

    const size_t cpu=chart->addSeries("cpu", ChartSeriesType::Line);

    chart->setSeriesPoints(cpu, {{newYear-43200.0, 1}, {newYear+43200.0, 2}, {newYear+86400.0, 3},
        {newYear+86400.0+3723.0, 4}});
    scene.frame();

    scene.moveTo(scene.local(newYear-43200.0, 5));
    CHECK(chart->getToolTipText() == "Dec 31 12:00  cpu: 1");
    // not the year alone, as the axis's tick at January 1st's midnight is
    scene.moveTo(scene.local(newYear+43200.0, 5));
    CHECK(chart->getToolTipText() == "Jan 1 12:00  cpu: 2");
    scene.moveTo(scene.local(newYear+86400.0, 5));
    CHECK(chart->getToolTipText() == "Jan 2  cpu: 3");
    scene.moveTo(scene.local(newYear+86400.0+3723.0, 5));
    CHECK(chart->getToolTipText() == "Jan 2 01:02:03  cpu: 4");
}
