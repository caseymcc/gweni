/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A ZoomPanel's children render in a space of their own: the render drain
 *  gives every control under it the renderer::RenderTransform the panel makes
 *  (its origin on the whole device px translate() puts the panel's origin at,
 *  plus the pan in device px, with a fraction kept apart; the canvas's scale
 *  times the zoom) and, as its device clip, the panel's region in device px,
 *  with the render offset 0 at the panel's origin and the clip region in the
 *  space's units. It gives the same on both of its paths: the whole-tree walk
 *  of a first frame, and the sorted queue of a frame with one control queued,
 *  which rebuilds the state down the control's ancestors. After the drain the
 *  renderer is back at the canvas's own transform (the identity at its scale).
 *  A new zoom or pan queues every control under the panel and lays nothing
 *  out.
 */
#include "headlessFixture.h"

#include <gweni/controls/zoomPanel.h>

#include <doctest/doctest.h>

#include <map>

using namespace gweni;

namespace
{

bool samePoint(const Point &a, const Point &b)
{
    return (a.x == b.x) && (a.y == b.y);
}

struct Seen
{
    Point offset;
    Rect clip;
    renderer::RenderTransform transform;
};

//! A plain control that records the state its render() ran in.
class TransformProbe: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(TransformProbe, controls::Base)
    {}

    std::map<controls::Base *, Seen> *seen=nullptr;

    void render(skin::Base *skin) override
    {
        renderer::Base *renderer=skin->getRenderer();

        if(seen)
            (*seen)[this]={renderer->getRenderOffset(), renderer->clipRegion(),
                renderer->renderTransform()};
        ParentClass::render(skin);
    }
};

struct ZoomScene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::ZoomPanel *panel;
    TransformProbe *outer;
    TransformProbe *inner;
    std::map<controls::Base *, Seen> seen;

    ZoomScene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setScale(1.5f);
        canvas->setSize(400, 300);

        panel=canvas->newChild<controls::ZoomPanel>();
        panel->setBounds(10, 20, 200, 100);
        panel->setZoom(2.0f);
        panel->setPan(PointF(3.25f, -1.0f));

        outer=panel->newChild<TransformProbe>();
        outer->setBounds(5, 5, 50, 20);
        outer->seen=&seen;
        inner=outer->newChild<TransformProbe>();
        inner->setBounds(2, 3, 10, 10);
        inner->seen=&seen;
    }

    ~ZoomScene()
    {
        delete canvas;
    }

    const controls::ChangeStats &frame()
    {
        seen.clear();
        controls::resetChangeStats();
        canvas->updateCanvas();
        canvas->renderCanvas();
        return controls::getChangeStats();
    }

    void settle()
    {
        for(int i=0; i<10 && frame().controls != 0; ++i)
            ;
    }
};

void checkSpace(const renderer::RenderTransform &t)
{
    // the panel's origin (10, 20) at scale 1.5 is device (15, 30); the pan
    // (3.25, -1) is (4.875, -1.5) device px
    CHECK(t.originX == 19);
    CHECK(t.fractionX == doctest::Approx(0.875f));
    CHECK(t.originY == 28);
    CHECK(t.fractionY == doctest::Approx(0.5f));
    CHECK(t.scale == 3.0f);
    CHECK(t.bounded);
    // the panel's bounds in device px: (10, 20)-(210, 120) at 1.5
    CHECK((t.deviceClip == Rect(15, 30, 300, 150)));
}

}//namespace

TEST_CASE("ZoomPanel: its children render in its space on both of the drain's paths")
{
    ZoomScene scene;

    scene.frame();
    REQUIRE(scene.seen.count(scene.outer) == 1);
    REQUIRE(scene.seen.count(scene.inner) == 1);

    const Seen walkedOuter=scene.seen[scene.outer];
    const Seen walkedInner=scene.seen[scene.inner];

    checkSpace(walkedOuter.transform);
    checkSpace(walkedInner.transform);
    // offset 0 at the panel's origin, inside the inner panel (content 200x100)
    CHECK(samePoint(walkedOuter.offset, Point(0, 0)));
    CHECK((walkedOuter.clip == Rect(0, 0, 200, 100)));
    CHECK(samePoint(walkedInner.offset, Point(5, 5)));
    CHECK((walkedInner.clip == Rect(5, 5, 50, 20)));

    // the canvas's own space again
    CHECK_FALSE(scene.renderer.renderTransform().bounded);
    CHECK(scene.renderer.renderTransform().originX == 0);
    CHECK(scene.renderer.renderTransform().scale == 1.5f);

    // one control queued: the sorted path builds the same state
    scene.settle();
    scene.inner->addStateChange(controls::StateChange_Geometry);
    scene.frame();
    REQUIRE(scene.seen.count(scene.inner) == 1);
    CHECK(scene.seen.count(scene.outer) == 0);

    const Seen sortedInner=scene.seen[scene.inner];

    checkSpace(sortedInner.transform);
    CHECK(samePoint(sortedInner.offset, walkedInner.offset));
    CHECK((sortedInner.clip == walkedInner.clip));
    CHECK_FALSE(scene.renderer.renderTransform().bounded);
}

TEST_CASE("ZoomPanel: a new zoom or pan renders its content again and lays nothing out")
{
    ZoomScene scene;

    scene.settle();

    const auto &zoomed=(scene.panel->setZoom(3.0f), scene.frame());

    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(scene.seen.count(scene.outer) == 1);
    CHECK(scene.seen.count(scene.inner) == 1);
    CHECK(zoomed.controls >= 2);
    CHECK(scene.seen[scene.inner].transform.scale == 4.5f);

    scene.settle();
    scene.panel->setPan(PointF(0.0f, 0.0f));
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(scene.seen.count(scene.inner) == 1);
    CHECK(scene.seen[scene.inner].transform.originX == 15);
    CHECK(scene.seen[scene.inner].transform.fractionX == 0.0f);

    // the range clamps
    scene.panel->setZoom(100.0f);
    CHECK(scene.panel->zoom() == 4.0f);
    scene.panel->setZoom(0.01f);
    CHECK(scene.panel->zoom() == 0.25f);
}
