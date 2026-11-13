/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured Chart renderer's slots taken after its first draw, drawn
 *  offscreen with the textured skin and read back: slots draw in id order,
 *  and a partly covered pixel of a slot in front writes depth a slot behind
 *  it, drawn later, cannot pass, so a list that grew must still come before
 *  the lists in front of it. Two 2-unit lines crossing, fixed ranges: the
 *  one behind given its points one appendPoint() a frame from 2 to 21 (its
 *  list taken again three times) draws, pixel for pixel, what one built
 *  with the 21 does; and a second series, a marker and a span (behind the
 *  series) added after the first frame draw what a chart built with them
 *  does.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/chart.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace
{
using namespace gweni;
using controls::ChartAxisSide;
using controls::ChartPoint;
using controls::ChartSeriesType;

struct Scene
{
    skin::textured::TexturedSkin *skin;
    controls::Canvas *canvas;
    controls::Chart *chart;

    Scene()
    {
        const Point viewSize=test::viewSize();

        skin=new skin::textured::TexturedSkin(&test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);
        canvas=new controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);
        chart=canvas->newChild<controls::Chart>();
        chart->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        chart->setBounds(10, 10, 300, 200);
        chart->setLegendVisible(false);
        chart->setXAxisType(controls::ChartXAxisType::Value);
        chart->setXRange(0, 10);
        chart->setYRange(ChartAxisSide::Left, 0, 10);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        delete canvas;
        delete skin;
        // the released slots' bytes are flushed
        test::gpuRenderer().begin();
        test::gpuRenderer().end();
    }

    std::vector<uint8_t> frame()
    {
        std::vector<uint8_t> pixels;

        test::clearView(Color(0, 0, 0, 255));
        canvas->updateCanvas();
        canvas->renderCanvas();
        test::readView(pixels);
        return pixels;
    }

    size_t line(const char *name)
    {
        const size_t series=chart->addSeries(name, ChartSeriesType::Line);

        chart->setSeriesLineThickness(series, 2.0f);
        return series;
    }
};

// x = i/2 for i in 0..20: the rising line crosses the flat falling one near
// i = 19, in a segment the rising one takes only once it has grown past
// several capacities
constexpr int Points=21;

double rising(int i)
{
    return i*0.45+0.1;
}

std::vector<ChartPoint> points(int count, bool up)
{
    std::vector<ChartPoint> result;

    for(int i=0; i<count; ++i)
        result.push_back({i*0.5, up ? rising(i) : 9.0-i*0.02});
    return result;
}

//! The pixels whose colour differs, and the largest channel difference.
void compare(const std::vector<uint8_t> &built, const std::vector<uint8_t> &grown, size_t &differ, int &largest)
{
    differ=0;
    largest=0;
    REQUIRE(built.size() == grown.size());
    for(size_t i=0; i+3 < built.size(); i+=4)
    {
        int delta=0;

        for(size_t c=0; c<3; ++c)
            delta=std::max(delta, std::abs(int(built[i+c])-int(grown[i+c])));
        if(delta)
        {
            ++differ;
            largest=std::max(largest, delta);
        }
    }
}

}//namespace

TEST_CASE("Chart slots: points appended a frame at a time draw what a chart built with them draws")
{
    std::vector<uint8_t> grown;
    {
        Scene scene;
        const size_t a=scene.line("a");
        const size_t b=scene.line("b");

        scene.chart->setSeriesPoints(a, points(2, true));
        scene.chart->setSeriesPoints(b, points(Points, false));
        scene.frame();
        for(int i=2; i<Points; ++i)
        {
            scene.chart->appendPoint(a, i*0.5, rising(i));
            scene.frame();
        }
        grown=scene.frame();
    }
    std::vector<uint8_t> built;
    {
        Scene scene;
        const size_t a=scene.line("a");
        const size_t b=scene.line("b");

        scene.chart->setSeriesPoints(a, points(Points, true));
        scene.chart->setSeriesPoints(b, points(Points, false));
        scene.frame();
        built=scene.frame();
    }

    size_t differ;
    int largest;

    compare(built, grown, differ, largest);
    CHECK(differ == 0);
    CHECK(largest == 0);
}

TEST_CASE("Chart slots: a series, a marker and a span added after the first frame draw as built with them")
{
    std::vector<uint8_t> grown;
    {
        Scene scene;
        const size_t a=scene.line("a");

        scene.chart->setSeriesPoints(a, points(Points, true));
        scene.frame();

        const size_t b=scene.line("b");

        scene.chart->setSeriesPoints(b, points(Points, false));
        scene.frame();
        scene.chart->addMarker(5.0, "m");
        scene.frame();
        // last, so no later growth takes its list again: under the series
        scene.chart->addSpan(2.0, 4.0);
        scene.frame();
        grown=scene.frame();
    }
    std::vector<uint8_t> built;
    {
        Scene scene;
        const size_t a=scene.line("a");
        const size_t b=scene.line("b");

        scene.chart->setSeriesPoints(a, points(Points, true));
        scene.chart->setSeriesPoints(b, points(Points, false));
        scene.chart->addSpan(2.0, 4.0);
        scene.chart->addMarker(5.0, "m");
        scene.frame();
        built=scene.frame();
    }

    size_t differ;
    int largest;

    compare(built, grown, differ, largest);
    CHECK(differ == 0);
    CHECK(largest == 0);
}
