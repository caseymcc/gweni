/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Chart's legend, on the headless fixture with the textured
 *  Chart and ChartLegendRow renderers (a StubControl for the rest), clicked
 *  through the input handler: one row per series in a column at the chart's
 *  right, each its series' name, the plot ending LegendGap before it; a click
 *  on a row hides its series (an automatic value axis rescaled, the stacks
 *  worked out again, the row never selected, its name in
 *  Colors.Selectable.Disabled and its swatch faded) and fires onSeriesToggled
 *  once with the series and the row, a second click showing it again; an
 *  app's setSeriesVisible() firing nothing; the rows following a series
 *  removed and renamed; a legend taller than the chart scrolling, as high as
 *  the chart's inner height and wider by the bar's thickness, a third of the
 *  chart's width at most; setLegendVisible(false), and no series at all,
 *  giving the plot the column back; every row wholly inside the legend's
 *  viewport; and a series added, a toggle, a longer name and two resizes each
 *  laid out in one pass that layout verification's full pass agrees with.
 *  An onSeriesToggled handler removing the clicked row's series leaves the
 *  row alive and hidden until the frame's delayed deletes, so its own press
 *  never runs on a deleted row. A series removed redraws the rows that keep
 *  their index and now show another series: each name in its new series'
 *  shown or hidden colour, each swatch in its colour, faded when hidden.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/chart.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/text.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

using namespace gweni;
using controls::Chart;
using controls::ChartAxisSide;
using controls::ChartSeriesType;
using controls::ChartXAxisType;
using controls::internal::ChartLegendRow;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

//! Each slot's last rect draw: its rect and fill colour.
class SwatchRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top, Color bottom,
        Color borderColor, int zIndex) override
    {
        rects[primitiveId]=rect;
        fills[primitiveId]=top;
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
    }

    std::map<size_t, Rect> rects;
    std::map<size_t, Color> fills;
};

class LegendSkin: public test::StubSkin
{
public:
    explicit LegendSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Chart")
            return std::make_unique<skin::textured::Chart>();
        if(typeName == "ChartLegendRow")
            return std::make_unique<skin::textured::ChartLegendRow>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene: public event::Handler
{
    Scene()
    {
        // a second apart per click, so two clicks are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        chart=canvas->newChild<Chart>();
        chart->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        chart->setBounds(10, 20, 500, 240);
        chart->setXAxisType(ChartXAxisType::Value);
        chart->onSeriesToggled.add(this, &Scene::toggled);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button state are process-global
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

    void click(controls::Base *control)
    {
        clock+=1.0f;
        const Point at=control->localPosToCanvas();

        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    void toggled(event::Info info)
    {
        toggles.push_back({info.integer, info.control});
    }

    struct Toggle
    {
        size_t series;
        controls::Base *row;
    };

    SwatchRecorder renderer;
    LegendSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    Chart *chart=nullptr;
    float clock=10.0f;
    std::vector<Toggle> toggles;
};

}//namespace

TEST_CASE("Chart legend: a row per series, a click toggling its series")
{
    Scene scene;
    Chart *chart=scene.chart;

    const size_t low=chart->addSeries("low", ChartSeriesType::StackedBar);
    const size_t high=chart->addSeries("high", ChartSeriesType::StackedBar);
    const size_t line=chart->addSeries("a long line name", ChartSeriesType::Line);

    chart->setSeriesPoints(low, {{0, 1}, {1, 2}});
    chart->setSeriesPoints(high, {{0, 1}, {1, 2}});
    chart->setSeriesPoints(line, {{0, 50}, {1, 90}});
    scene.frame();

    controls::ScrollControl *legend=chart->getLegend();
    REQUIRE(legend != nullptr);
    CHECK(legend->visible());
    REQUIRE(chart->getLegendRow(2) != nullptr);
    CHECK(chart->getLegendRow(3) == nullptr);
    for(size_t i=0; i < 3; ++i)
    {
        CHECK(chart->getLegendRow(i)->getText() == chart->getSeriesName(i));
        CHECK(chart->getLegendRow(i)->getSeries() == i);
    }

    // a column at the right, the plot ending LegendGap before it
    const Rect &bounds=legend->getBounds();
    CHECK(bounds.x+bounds.w == chart->getWidth()-chart->getPadding().right);
    CHECK(bounds.y == chart->getPadding().top);
    CHECK(chart->getPlotBounds().x+chart->getPlotBounds().w <= bounds.x-Chart::LegendGap);
    // as high as its rows, each a line and its padding high
    const int rowHeight=chart->getLegendRow(0)->getHeight();
    CHECK(rowHeight > 0);
    CHECK(bounds.h == legend->getPreferredSize().height);
    CHECK(bounds.h >= 3*rowHeight);
    CHECK(chart->getLegendRow(1)->getBounds().y == chart->getLegendRow(0)->getBounds().y+rowHeight);
    // every row wholly in the legend's viewport, so a click lands on it
    for(size_t i=0; i < 3; ++i)
    {
        const Point at=chart->getLegendRow(i)->localPosToCanvas();
        controls::internal::ChartLegendRow *row=chart->getLegendRow(i);

        CHECK(scene.canvas->getControlAt(at.x+1, at.y+row->getHeight()-1) == row);
        CHECK(scene.canvas->getControlAt(at.x+row->getWidth()-1, at.y+1) == row);
    }

    // the swatch in the series' colour
    const size_t swatch=chart->getLegendRow(line)->getSkinControl()->getPrimitives()[1];
    CHECK((scene.renderer.fills.at(swatch) == tokyonight::chartSeries(2).top));

    CHECK(chart->getYMax(ChartAxisSide::Left) >= 90.0);
    CHECK(chart->getStackBase(high, 1) == 2.0);

    // a click hides it: the axis rescales, the stacks again, the event fires
    scene.click(chart->getLegendRow(line));
    CHECK(!chart->isSeriesVisible(line));
    REQUIRE(scene.toggles.size() == 1);
    CHECK(scene.toggles[0].series == line);
    CHECK(scene.toggles[0].row == chart->getLegendRow(line));
    CHECK(!chart->getLegendRow(line)->isSelected());
    CHECK(chart->getYMax(ChartAxisSide::Left) < 50.0);
    CHECK((chart->getLegendRow(line)->getTextColor() == scene.skin.Colors.Selectable.Disabled));
    CHECK(scene.renderer.fills.at(swatch).a == tokyonight::chartSeries(2).top.a/4);

    scene.click(chart->getLegendRow(low));
    CHECK(!chart->isSeriesVisible(low));
    CHECK(chart->getStackBase(high, 1) == 0.0);
    REQUIRE(scene.toggles.size() == 2);
    CHECK(scene.toggles[1].series == low);

    // a second click shows it again
    scene.click(chart->getLegendRow(line));
    CHECK(chart->isSeriesVisible(line));
    CHECK(chart->getYMax(ChartAxisSide::Left) >= 90.0);
    CHECK(!(chart->getLegendRow(line)->getTextColor() == scene.skin.Colors.Selectable.Disabled));
    CHECK(scene.renderer.fills.at(swatch).a == tokyonight::chartSeries(2).top.a);
    REQUIRE(scene.toggles.size() == 3);

    // the app's own toggle fires nothing
    chart->setSeriesVisible(low, true);
    scene.frame();
    CHECK(scene.toggles.size() == 3);
    CHECK(chart->getStackBase(high, 1) == 2.0);

    // the rows follow the series
    chart->removeSeries(low);
    chart->setSeriesName(0, "renamed");
    scene.frame();
    REQUIRE(chart->getLegendRow(1) != nullptr);
    CHECK(chart->getLegendRow(2) == nullptr);
    CHECK(chart->getLegendRow(0)->getText() == "renamed");
    CHECK(chart->getLegendRow(1)->getText() == "a long line name");
    CHECK(chart->getLegendRow(1)->getSeries() == 1);
    scene.click(chart->getLegendRow(1));
    CHECK(!chart->isSeriesVisible(1));
    CHECK(scene.toggles.back().series == 1);
}

TEST_CASE("Chart legend: a long legend scrolls, a hidden one gives the plot its room")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->setBounds(10, 20, 500, 120);
    for(int i=0; i < 30; ++i)
    {
        const size_t series=chart->addSeries("series "+std::to_string(i), ChartSeriesType::Line);

        chart->setSeriesPoints(series, {{0, double(i)}, {1, double(i+1)}});
    }
    scene.frame();

    controls::ScrollControl *legend=chart->getLegend();
    const int innerHeight=chart->getHeight()-chart->getPadding().top-chart->getPadding().bottom;
    const int innerWidth=chart->getWidth()-chart->getPadding().left-chart->getPadding().right;

    CHECK(legend->getBounds().h == innerHeight);
    CHECK(legend->getBounds().w <= innerWidth/3);
    // its bar shows, and the rows are not cut by it
    CHECK(legend->canScrollVertical());
    CHECK(legend->getBounds().w == std::min(innerWidth/3, legend->getPreferredSize().width
          +scene.skin.Metrics.ScrollBarThickness));
    const int widest=scene.renderer.measureText(scene.skin.getDefaultFont(), "series 29").x;
    CHECK(legend->getPreferredSize().width >= ChartLegendRow::PaddingLeft+widest+ChartLegendRow::PaddingRight);

    const int withLegend=chart->getPlotBounds().w;

    chart->setLegendVisible(false);
    scene.frame();
    CHECK(!legend->visible());
    CHECK(chart->getPlotBounds().w > withLegend);
    CHECK(chart->getPlotBounds().x+chart->getPlotBounds().w == chart->getWidth()-chart->getPadding().right);

    chart->setLegendVisible(true);
    scene.frame();
    CHECK(legend->visible());
    CHECK(chart->getPlotBounds().w == withLegend);

    // no series, no legend
    chart->clearSeries();
    scene.frame();
    CHECK(!legend->visible());
}

TEST_CASE("Chart legend: a toggle, a rename, a series added and a resize lay out in one pass")
{
    const bool verify=controls::layoutVerifyEnabled();
    const size_t failures=controls::layoutVerifyFailures();

    controls::setLayoutVerify(true);
    {
        Scene scene;
        Chart *chart=scene.chart;
        auto onePass=[&]()
        {
            scene.canvas->updateCanvas();
            const controls::LayoutStats &stats=scene.canvas->getLayoutStats();
            CHECK(stats.passes == 1);
            CHECK(stats.redirtiedType == nullptr);
            // the chart computes the legend's bounds: no request
            CHECK(stats.requests == 0);
            scene.canvas->renderCanvas();
            CHECK(!scene.canvas->getLayoutStats().renderLayout);
        };

        const size_t a=chart->addSeries("a", ChartSeriesType::Bar);
        chart->setSeriesPoints(a, {{0, 1}, {1, 3}});
        onePass();

        const size_t b=chart->addSeries("b", ChartSeriesType::Line, ChartAxisSide::Right);
        chart->setSeriesPoints(b, {{0, 10}, {1, 30}});
        onePass();
        scene.click(chart->getLegendRow(a));
        CHECK(!chart->isSeriesVisible(a));
        chart->setSeriesName(b, "a much longer name than before");
        onePass();
        chart->setBounds(10, 20, 300, 80);
        onePass();
        chart->setBounds(10, 20, 600, 300);
        onePass();
    }
    CHECK(controls::layoutVerifyFailures() == failures);
    controls::setLayoutVerify(verify);
}

TEST_CASE("Chart legend: a toggle handler that removes the series keeps the clicked row until the frame ends")
{
    Scene scene;
    Chart *chart=scene.chart;

    chart->addSeries("a", ChartSeriesType::Line);
    const size_t b=chart->addSeries("b", ChartSeriesType::Line);
    chart->setSeriesPoints(0, {{0, 1}, {1, 2}});
    chart->setSeriesPoints(b, {{0, 2}, {1, 3}});
    scene.frame();

    controls::Base *row=chart->getLegendRow(b);
    REQUIRE(row != nullptr);
    const size_t rowId=row->getControlId();
    const test::IdBankCounts before=test::IdBankCounts::now();

    // the handler removes the series its row stands for, inside the row's press
    struct Remover: public event::Handler
    {
        Chart *chart=nullptr;
        size_t rowId=0;
        bool rowAlive=false;
        bool rowHidden=false;

        void toggled(event::Info info)
        {
            chart->removeSeries(size_t(info.integer));
            controls::Base *row=controls::Base::getControlById(rowId);

            // checked here, before the row's press goes on
            CHECK(row != nullptr);
            rowAlive=(row != nullptr);
            rowHidden=row && row->hidden();
        }
    } remover;

    remover.chart=chart;
    remover.rowId=rowId;
    chart->onSeriesToggled.add(&remover, &Remover::toggled);
    scene.click(row);

    CHECK(chart->getSeriesCount() == 1);
    CHECK(chart->getLegendRow(b) == nullptr);
    // the row its press is still running in is not deleted under it
    CHECK(remover.rowAlive);
    CHECK(remover.rowHidden);
    // and is gone after the frame, its slots with it
    scene.frame();
    CHECK(controls::Base::getControlById(rowId) == nullptr);
    CHECK(test::IdBankCounts::now().livePrimitives() < before.livePrimitives());
}

TEST_CASE("Chart legend: a series removed redraws the rows that now show another series")
{
    Scene scene;
    Chart *chart=scene.chart;

    // told apart (the stub skin's colours are all its defaults)
    scene.skin.Colors.Selectable.Normal=Color(200, 200, 200, 255);
    scene.skin.Colors.Selectable.Disabled=Color(90, 90, 90, 255);
    for(const char *name: {"aa", "bb", "cc"})
    {
        const size_t series=chart->addSeries(name, ChartSeriesType::Bar);

        chart->setSeriesPoints(series, {{0, 1}, {1, 2}});
    }
    chart->setSeriesVisible(2, false);
    scene.frame();

    const Color shownText=chart->getLegendRow(0)->getTextColor();
    const Color hiddenText=scene.skin.Colors.Selectable.Disabled;

    REQUIRE(!(shownText == hiddenText));
    REQUIRE((chart->getLegendRow(2)->getTextColor() == hiddenText));

    // the rows keep their index: row 0 now shows bb (shown), row 1 cc (hidden)
    chart->removeSeries(0);
    scene.frame();

    REQUIRE(chart->getSeriesCount() == 2);
    REQUIRE(chart->getLegendRow(1) != nullptr);
    CHECK(chart->getLegendRow(0)->getText() == "bb");
    CHECK((chart->getLegendRow(0)->getTextColor() == shownText));
    CHECK(chart->getLegendRow(1)->getText() == "cc");
    CHECK((chart->getLegendRow(1)->getTextColor() == hiddenText));

    // the swatches in their series' colours, the hidden one faded
    for(size_t i=0; i < 2; ++i)
    {
        const size_t swatch=chart->getLegendRow(i)->getSkinControl()->getPrimitives()[1];
        const Color colour=tokyonight::chartSeries(chart->getSeriesColorIndex(i)).top;

        CHECK(scene.renderer.fills.at(swatch).r == colour.r);
        CHECK(scene.renderer.fills.at(swatch).a == (i == 0 ? colour.a : colour.a/4));
    }
}
