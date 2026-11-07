/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CrossSplitter places its three bars and four panels from its split
 *  fractions, in its own coordinates, whatever its margin: the bars at
 *  round((W-b)*h) / round((H-b)*v) and the panels in the quadrants between
 *  them. A resize keeps the fractions, zoom() shows one panel over the whole
 *  splitter with the bars hidden, a splitter no bigger than its bar gives
 *  nothing a negative size, and a settled frame lays nothing out.
 */
#include "headlessFixture.h"

#include <gweni/controls/crossSplitter.h>
#include <gweni/controls/layout/layout.h>

#include <doctest/doctest.h>

#include <cmath>
#include <iterator>
#include <string>

using namespace gweni;

namespace
{

//! A plain panel that counts its layouts.
class CountingPanel: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(CountingPanel, controls::Base)
    {}

    int layouts=0;

protected:
    void layout(skin::Base *skin) override
    {
        ++layouts;
        ParentClass::layout(skin);
    }
};

constexpr int BarSize=5;

//! "(x, y, w, h)": bounds are compared as these, so a failure prints both.
std::string rect(const Rect &r)
{
    return "("+std::to_string(r.x)+", "+std::to_string(r.y)+", "+
        std::to_string(r.w)+", "+std::to_string(r.h)+")";
}

std::string rect(controls::Base *control)
{
    return rect(control->getBounds());
}

//! LabelMultilineDemo's arrangement: a host with a VerticalLayout docked in
//! the canvas, holding a CrossSplitter docked Center with margin 10 and four
//! panels.
struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *host;
    controls::CrossSplitter *splitter;
    CountingPanel *panels[4];

    Scene(int width=640, int height=480)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, height);

        host=canvas->newChild<controls::Base>("host");
        host->setLayout(new controls::VerticalLayout());
        host->setDock(DockPosition::Center);

        splitter=host->newChild<controls::CrossSplitter>("splitter");
        splitter->setDock(DockPosition::Center);
        splitter->setMargin(Margin(10, 10, 10, 10));

        for(int i=0; i<4; ++i)
        {
            panels[i]=splitter->newChild<CountingPanel>("panel"+std::to_string(i));
            splitter->setPanel(i, panels[i]);
        }
    }

    ~Scene()
    {
        delete canvas;
    }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    //! Frames until one lays nothing out, so a case starts from a settled tree.
    void settle()
    {
        for(int i=0; i<10 && frame().passes != 0; ++i)
            ;
    }

    //! The bars are the splitter's first three children, in the order its
    //! constructor makes them.
    controls::Base *bar(int i)
    {
        return *std::next(splitter->getChildren().begin(), i);
    }
    controls::Base *verticalBar() { return bar(0); }   //!< full width, splits top/bottom
    controls::Base *horizontalBar() { return bar(1); } //!< full height, splits left/right
    controls::Base *centerBar() { return bar(2); }
};

int barPos(int extent, float fraction)
{
    return static_cast<int>(std::lround((extent-BarSize)*fraction));
}

//! Checks criteria 1 and 2 of the placement: bars and panels from the
//! fractions h, v at the splitter's current size.
void checkPlacement(Scene &scene, float h, float v)
{
    const int W=scene.splitter->getWidth();
    const int H=scene.splitter->getHeight();
    const int b=BarSize;
    const int hx=barPos(W, h);
    const int vy=barPos(H, v);
    INFO("splitter ", W, "x", H, " hx=", hx, " vy=", vy);

    CHECK(rect(scene.verticalBar()) == rect(Rect(0, vy, W, b)));
    CHECK(rect(scene.horizontalBar()) == rect(Rect(hx, 0, b, H)));
    CHECK(rect(scene.centerBar()) == rect(Rect(hx, vy, b, b)));

    CHECK(rect(scene.panels[0]) == rect(Rect(0, 0, hx, vy)));
    CHECK(rect(scene.panels[1]) == rect(Rect(hx+b, 0, W-hx-b, vy)));
    CHECK(rect(scene.panels[2]) == rect(Rect(0, vy+b, hx, H-vy-b)));
    CHECK(rect(scene.panels[3]) == rect(Rect(hx+b, vy+b, W-hx-b, H-vy-b)));

    for(int i=0; i<4; ++i)
    {
        for(int j=i+1; j<4; ++j)
        {
            const Rect &a=scene.panels[i]->getBounds();
            const Rect &c=scene.panels[j]->getBounds();
            const bool overlap=a.left() < c.right() && c.left() < a.right() &&
                a.top() < c.bottom() && c.top() < a.bottom();
            INFO("panels ", i, " and ", j);
            CHECK_FALSE(overlap);
        }
    }
}

}//namespace

TEST_CASE("CrossSplitter places its bars and panels from its fractions, whatever its margin")
{
    Scene scene;
    scene.settle();

    REQUIRE(scene.splitter->getWidth() > BarSize);
    REQUIRE(scene.splitter->getHeight() > BarSize);
    checkPlacement(scene, 0.5f, 0.5f);

    // the same with no margin
    scene.splitter->setMargin(Margin());
    scene.settle();
    checkPlacement(scene, 0.5f, 0.5f);
}

TEST_CASE("CrossSplitter keeps its fractions across a resize, in one frame")
{
    Scene scene;
    scene.settle();
    const int width=scene.splitter->getWidth();
    const int height=scene.splitter->getHeight();

    scene.canvas->setSize(1023, 767);
    const controls::LayoutStats &stats=scene.frame();
    CHECK(stats.passes == 1);
    CHECK_FALSE(stats.capped);
    CHECK(scene.splitter->getWidth() != width);
    CHECK(scene.splitter->getHeight() != height);
    checkPlacement(scene, 0.5f, 0.5f);

    scene.canvas->setSize(640, 480);
    scene.frame();
    CHECK(scene.splitter->getWidth() == width);
    CHECK(scene.splitter->getHeight() == height);
    checkPlacement(scene, 0.5f, 0.5f);
}

TEST_CASE("CrossSplitter::zoom shows one panel over the whole splitter and hides the bars")
{
    Scene scene;
    scene.settle();
    const int W=scene.splitter->getWidth();
    const int H=scene.splitter->getHeight();

    for(int zoomed=0; zoomed<4; ++zoomed)
    {
        INFO("zoomed panel ", zoomed);
        scene.splitter->zoom(zoomed);
        scene.frame();
        CHECK(scene.splitter->isZoomed());
        CHECK(rect(scene.panels[zoomed]) == rect(Rect(0, 0, W, H)));
        CHECK_FALSE(scene.panels[zoomed]->hidden());

        for(int i=0; i<4; ++i)
        {
            if(i != zoomed)
                CHECK(scene.panels[i]->hidden());
        }
        for(int i=0; i<3; ++i)
            CHECK(scene.bar(i)->hidden());
    }

    scene.splitter->unZoom();
    scene.frame();
    CHECK_FALSE(scene.splitter->isZoomed());
    for(int i=0; i<4; ++i)
        CHECK_FALSE(scene.panels[i]->hidden());
    for(int i=0; i<3; ++i)
        CHECK_FALSE(scene.bar(i)->hidden());
    checkPlacement(scene, 0.5f, 0.5f);
}

TEST_CASE("CrossSplitter no bigger than its bar gives nothing a negative size")
{
    // 0x0 on the first frame, then each axis on its own no bigger than the
    // bar, then a usable size again: the fractions survive all of it.
    const Point sizes[]={{0, 0}, {BarSize-1, 480}, {BarSize, 480}, {640, BarSize-1},
        {640, BarSize}};

    Scene scene(0, 0);
    scene.splitter->setMargin(Margin());
    for(const Point &size: sizes)
    {
        INFO("canvas ", size.x, "x", size.y);
        scene.canvas->setSize(size.x, size.y);
        scene.settle();

        for(int i=0; i<4; ++i)
        {
            INFO("panel ", i);
            CHECK(scene.panels[i]->getX() >= 0);
            CHECK(scene.panels[i]->getY() >= 0);
            CHECK(scene.panels[i]->getWidth() >= 0);
            CHECK(scene.panels[i]->getHeight() >= 0);
        }
        for(int i=0; i<3; ++i)
        {
            INFO("bar ", i);
            CHECK(scene.bar(i)->getX() >= 0);
            CHECK(scene.bar(i)->getY() >= 0);
            CHECK(scene.bar(i)->getWidth() >= 0);
            CHECK(scene.bar(i)->getHeight() >= 0);
        }
    }

    scene.canvas->setSize(640, 480);
    scene.settle();
    checkPlacement(scene, 0.5f, 0.5f);
}

TEST_CASE("a settled CrossSplitter lays nothing out")
{
    Scene scene;
    scene.settle();

    int layouts[4];
    for(int i=0; i<4; ++i)
        layouts[i]=scene.panels[i]->layouts;

    for(int frame=0; frame<3; ++frame)
    {
        const controls::LayoutStats &stats=scene.frame();
        CHECK(stats.passes == 0);
        CHECK_FALSE(stats.renderLayout);
    }
    for(int i=0; i<4; ++i)
        CHECK(scene.panels[i]->layouts == layouts[i]);
}
