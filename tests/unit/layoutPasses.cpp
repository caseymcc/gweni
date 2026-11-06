/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas::layoutCanvas: a layout invalidation raised during a pass is kept
 *  and laid out by another pass in the same frame, at most
 *  Canvas::MaxLayoutPasses of them, and LayoutStats names the control that
 *  re-dirtied layout. Placement, the bounds a pass gives a control it has not
 *  reached yet, does not re-dirty.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

#include <cstring>

using namespace gweni;

namespace
{

//! Calls invalidate() from its own layout() the first `times` times.
class Redirtier: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Redirtier, controls::Base)
    {}

    int times=0;
    int layouts=0;

protected:
    void layout(skin::Base *skin) override
    {
        ++layouts;
        if(times > 0)
        {
            --times;
            invalidate();
        }
        ParentClass::layout(skin);
    }
};

//! Places its one child itself, from layout(), before the pass reaches it.
class Placer: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Placer, controls::Base)
    {}

    controls::Base *child=nullptr;

protected:
    void layout(skin::Base *skin) override
    {
        if(child)
            child->setBounds(5, 5, getWidth()/2, 20);
        ParentClass::layout(skin);
    }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
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
};

}//namespace

TEST_CASE("a settled canvas lays nothing out")
{
    Scene scene;
    scene.canvas->newChild<controls::Base>("panel")->setDock(DockPosition::Center);
    scene.settle();

    const controls::LayoutStats &stats=scene.frame();
    CHECK(stats.passes == 0);
    CHECK(stats.redirtiedType == nullptr);
    CHECK_FALSE(stats.renderLayout);
}

TEST_CASE("placement converges in one pass")
{
    Scene scene;
    Placer *placer=scene.canvas->newChild<Placer>("placer");
    placer->setBounds(0, 0, 200, 100);
    placer->child=placer->newChild<controls::Base>("child");
    scene.settle();

    // A new width for the placer: it re-places its child from layout(), and
    // that child has not been laid out yet in the pass.
    placer->setBounds(0, 0, 300, 100);
    const controls::LayoutStats &stats=scene.frame();
    CHECK(stats.passes == 1);
    CHECK(stats.redirtiedType == nullptr);
    CHECK_FALSE(stats.renderLayout);
}

TEST_CASE("an invalidate() raised during a pass runs another pass in the same frame")
{
    Scene scene;
    Redirtier *control=scene.canvas->newChild<Redirtier>("redirtier");
    control->setBounds(0, 0, 50, 50);
    scene.settle();

    control->times=1;
    control->invalidate();
    const int layoutsBefore=control->layouts;
    const controls::LayoutStats &stats=scene.frame();

    CHECK(stats.passes == 2);
    REQUIRE(stats.redirtiedType != nullptr);
    CHECK(std::strcmp(stats.redirtiedType, "Redirtier") == 0);
    CHECK(stats.redirtiedId == control->getControlId());
    CHECK_FALSE(stats.capped);
    CHECK_FALSE(stats.renderLayout);
    // the request was laid out, not dropped
    CHECK(control->layouts == layoutsBefore+2);
    CHECK_FALSE(control->needsLayout());

    CHECK(scene.frame().passes == 0);
}

TEST_CASE("a control that re-dirties every pass is capped at MaxLayoutPasses")
{
    Scene scene;
    Redirtier *control=scene.canvas->newChild<Redirtier>("redirtier");
    control->setBounds(0, 0, 50, 50);
    scene.settle();

    control->times=1000;
    control->invalidate();

    for(int frame=0; frame<3; ++frame)
    {
        const controls::LayoutStats &stats=scene.frame();
        CHECK(stats.passes == controls::Canvas::MaxLayoutPasses);
        CHECK(stats.capped);
        // what the cap left dirty is laid out by renderCanvas
        CHECK(stats.renderLayout);
    }

    control->times=0;
    scene.settle();
    CHECK(scene.frame().passes == 0);
}
