/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  tools::DepthView on a real Canvas, its camera handed to a MirrorOnlyRenderer
 *  (a SlotRenderer whose backend does nothing): open() sets the renderer's view
 *  transform and the canvas's input capture and calls onOpenChangedCaller, and
 *  close() (or Escape) gives both back and calls it again; on a renderer that
 *  is no SlotRenderer it opens nothing, and a second canvas cannot take a
 *  renderer one view is on. At reset, on a landscape and a portrait view, the
 *  default spacing puts the highest z half the view's width in front and the
 *  camera is 1 + spacing*highest/focal focal lengths away: in perspective the
 *  highest z is drawn where the front view draws it and a lower one smaller
 *  about the view's centre, z 0 at focal/(focal + spacing*highest) of its size,
 *  and the orthographic projection draws every z at that z 0 size; in both,
 *  every corner of the canvas at z 0, half the highest and the highest is
 *  inside the view rect, a higher z is nearer the camera (a larger z index out)
 *  and every z index the canvas uses is inside the renderer's range. The mouse
 *  and keys move the camera as the class says: a left drag orbits, a right or
 *  middle drag pans by its device px, the wheel and W/S dolly from the reset
 *  distance by DollyStep a notch, the arrows and A/D move, P switches the
 *  projection, + and - space the labels, R resets; a move with no button down
 *  changes nothing.
 *  Deleting the canvas under an open view closes it without a call.
 */
#include "headlessFixture.h"
#include "mirrorOnlyRenderer.h"

#ifdef GWENI_TEST_SLOT_RENDERER

#include <gweni/controls/button.h>
#include <gweni/tools/depthView.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace gweni;

namespace
{

struct Point3
{
    float x, y, z;
};

//! Where the renderer's view transform puts (x, y, z), after the divide by w
Point3 project(const renderer::SlotRenderer &renderer, float x, float y, float z)
{
    const float *m=renderer.viewTransform();
    const float px=m[0]*x+m[4]*y+m[8]*z+m[12];
    const float py=m[1]*x+m[5]*y+m[9]*z+m[13];
    const float pz=m[2]*x+m[6]*y+m[10]*z+m[14];
    const float pw=m[3]*x+m[7]*y+m[11]*z+m[15];

    return { px/pw, py/pw, pz/pw };
}

class Listener: public event::Handler
{
public:
    void changed(event::Info info)
    {
        calls.push_back(info.integer);
        canvas=info.control;
    }

    std::vector<int> calls;
    controls::Base *canvas=nullptr;
};

struct Scene
{
    test::StubRenderer stub;
    test::StubSkin skin{&stub};
    test::MirrorOnlyRenderer slots;
    controls::Canvas *canvas=nullptr;
    Listener listener;

    //! A canvas filling a \p width x \p height view
    explicit Scene(int width=640, int height=480)
    {
        slots.setViewRect(Rect(0, 0, width, height));
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, height);
        for(int i=0; i<20; ++i)
        {
            controls::Button *button=canvas->newChild<controls::Button>();

            button->setBounds(10+i*20, 10+i*10, 100, 20);
        }
        canvas->updateCanvas();
        canvas->renderCanvas();
        tools::DepthView::onOpenChangedCaller.add(&listener, &Listener::changed);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        tools::DepthView::close(canvas);
        tools::DepthView::onOpenChangedCaller.removeHandler(&listener);
        delete canvas;
    }
};

}//namespace

TEST_CASE("Depth view: open and close set and give back the view transform and the input capture")
{
    Scene scene;

    REQUIRE(scene.canvas->highestZIndex() > 0);
    CHECK(tools::DepthView::find(scene.canvas) == nullptr);

    // the canvas skin's renderer is no SlotRenderer
    CHECK(tools::DepthView::open(scene.canvas) == nullptr);
    CHECK(scene.canvas->inputCapture() == nullptr);
    CHECK(scene.listener.calls.empty());

    tools::DepthView *view=tools::DepthView::open(scene.canvas, &scene.slots);

    REQUIRE(view != nullptr);
    CHECK(tools::DepthView::find(scene.canvas) == view);
    CHECK(tools::DepthView::open(scene.canvas, &scene.slots) == view);
    CHECK((view->canvas() == scene.canvas));
    CHECK(scene.canvas->inputCapture() == view);
    CHECK(scene.slots.hasViewTransform());
    CHECK(scene.canvas->needsRedraw());
    CHECK(scene.listener.calls == std::vector<int>{ 1 });
    CHECK((scene.listener.canvas == scene.canvas));

    // another canvas cannot take the renderer the view is on
    {
        controls::Canvas *other=new controls::Canvas(&scene.skin);

        CHECK(tools::DepthView::open(other, &scene.slots) == nullptr);
        CHECK(other->inputCapture() == nullptr);
        delete other;
    }

    tools::DepthView::close(scene.canvas);
    CHECK(tools::DepthView::find(scene.canvas) == nullptr);
    CHECK(scene.canvas->inputCapture() == nullptr);
    CHECK_FALSE(scene.slots.hasViewTransform());
    CHECK(scene.listener.calls == std::vector<int>{ 1, 0 });

    // toggle, and Escape in the view
    CHECK(tools::DepthView::toggle(scene.canvas, &scene.slots));
    CHECK(tools::DepthView::find(scene.canvas) != nullptr);
    CHECK(scene.canvas->inputModifierKey(keys::Escape, true));
    CHECK(tools::DepthView::find(scene.canvas) == nullptr);
    CHECK_FALSE(scene.slots.hasViewTransform());
    CHECK(scene.canvas->inputCapture() == nullptr);
    CHECK(scene.listener.calls == std::vector<int>{ 1, 0, 1, 0 });
    // the release reaches the canvas again, which ignores a key that is up
    CHECK_FALSE(scene.canvas->inputModifierKey(keys::Escape, false));
}

TEST_CASE("Depth view: at reset the highest z is drawn as in the front view and the whole canvas is on the view")
{
    for(const Point &size:{ Point(640, 480), Point(480, 640) })
    {
        CAPTURE(size.x);
        CAPTURE(size.y);

        Scene scene(size.x, size.y);
        tools::DepthView *view=tools::DepthView::open(scene.canvas, &scene.slots);

        REQUIRE(view != nullptr);

        const float w=(float)size.x, h=(float)size.y;
        const float cx=0.5f*w, cy=0.5f*h;
        const float high=(float)scene.canvas->highestZIndex();
        const float k=(float)renderer::SlotRenderer::kMaxZIndex;
        const float focal=tools::DepthView::FocalViews*std::max(w, h);
        const float s=view->camera().zSpacing;
        // z 0's size against the front view's
        const float back=focal/(focal+s*high);
        // a corner at the view's edge may land a rounding error outside it
        const float slack=1e-3f;

        REQUIRE(high > 1.0f);
        CHECK(s == doctest::Approx(0.5f*w/high));
        CHECK(view->camera().perspective);
        CHECK(view->camera().distance == doctest::Approx(1.0f+s*high/focal));
        CHECK(back < 1.0f);

        for(bool perspective:{ true, false })
        {
            CAPTURE(perspective);

            tools::DepthView::Camera camera=view->resetCamera();

            camera.perspective=perspective;
            view->setCamera(camera);

            for(const Point3 &p:{ Point3{ 0, 0, 0 }, Point3{ w, h, 0 }, Point3{ 100, 0.8f*h, 0 } })
            {
                // z 0 the same smaller size in either projection
                const Point3 out=project(scene.slots, p.x, p.y, 0.0f);

                CHECK(out.x == doctest::Approx(cx+(p.x-cx)*back).epsilon(1e-5));
                CHECK(out.y == doctest::Approx(cy+(p.y-cy)*back).epsilon(1e-5));

                // the highest z where the front view draws it in perspective,
                // at z 0's size in orthographic
                const Point3 front=project(scene.slots, p.x, p.y, high);
                const float frontSize=perspective?1.0f:back;

                CHECK(front.x == doctest::Approx(cx+(p.x-cx)*frontSize).epsilon(1e-5));
                CHECK(front.y == doctest::Approx(cy+(p.y-cy)*frontSize).epsilon(1e-5));

                // a layer between them between their sizes in perspective
                if(perspective && p.x != cx)
                {
                    const float middle=(project(scene.slots, p.x, p.y, 0.5f*high).x-cx)/(p.x-cx);

                    CHECK(middle > back);
                    CHECK(middle < 1.0f);
                }
            }

            float previous=-1.0f;

            for(float z:{ 0.0f, 1.0f, high*0.5f, high })
            {
                CAPTURE(z);
                for(const Point3 &corner:{ Point3{ 0, 0, z }, Point3{ w, 0, z }, Point3{ 0, h, z },
                    Point3{ w, h, z } })
                {
                    const Point3 out=project(scene.slots, corner.x, corner.y, corner.z);

                    CHECK(out.x >= -slack);
                    CHECK(out.x <= w+slack);
                    CHECK(out.y >= -slack);
                    CHECK(out.y <= h+slack);
                    CHECK(out.z >= 0.0f);
                    CHECK(out.z <= k);
                }

                const float depth=project(scene.slots, cx, cy, z).z;

                CHECK(depth > previous);
                previous=depth;
            }
        }
    }
}

TEST_CASE("Depth view: the mouse and the keys move the camera")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;
    tools::DepthView *view=tools::DepthView::open(canvas, &scene.slots);

    REQUIRE(view != nullptr);

    const tools::DepthView::Camera reset=view->resetCamera();

    CHECK(view->camera() == reset);

    // no button down: nothing
    canvas->inputMouseMoved(300, 200, 10, 5);
    CHECK(view->camera() == reset);

    // a left drag orbits
    canvas->inputMouseButton(0, true);
    canvas->inputMouseMoved(310, 205, 10, 5);
    canvas->inputMouseButton(0, false);
    CHECK(view->camera().yaw == doctest::Approx(10*tools::DepthView::OrbitDegreesPerUnit));
    CHECK(view->camera().pitch == doctest::Approx(5*tools::DepthView::OrbitDegreesPerUnit));
    CHECK(view->camera().panX == 0.0f);

    // the pitch stops at 90
    canvas->inputMouseButton(0, true);
    canvas->inputMouseMoved(310, 905, 0, 700);
    canvas->inputMouseButton(0, false);
    CHECK(view->camera().pitch == 90.0f);

    // a right and a middle drag pan
    view->reset();
    canvas->inputMouseButton(1, true);
    canvas->inputMouseMoved(320, 210, 10, 5);
    canvas->inputMouseButton(1, false);
    canvas->inputMouseButton(2, true);
    canvas->inputMouseMoved(330, 215, 10, 5);
    canvas->inputMouseButton(2, false);
    CHECK(view->camera().panX == 20.0f);
    CHECK(view->camera().panY == 10.0f);
    CHECK(view->camera().yaw == 0.0f);

    // the wheel and W/S dolly: away from the user is in
    view->reset();
    canvas->inputMouseWheel(60);
    CHECK(view->camera().distance == doctest::Approx(reset.distance*tools::DepthView::DollyStep));
    canvas->inputMouseWheel(-60);
    CHECK(view->camera().distance == doctest::Approx(reset.distance));
    canvas->inputCharacter('w');
    CHECK(view->camera().distance < reset.distance);
    canvas->inputCharacter('S');
    CHECK(view->camera().distance == doctest::Approx(reset.distance));

    // the arrows and A/D move the camera, the picture the other way
    view->reset();
    canvas->inputModifierKey(keys::Right, true);
    canvas->inputModifierKey(keys::Right, false);
    CHECK(view->camera().panX == -tools::DepthView::KeyMoveUnits);
    canvas->inputCharacter('a');
    CHECK(view->camera().panX == 0.0f);
    canvas->inputModifierKey(keys::Down, true);
    CHECK(view->camera().panY == -tools::DepthView::KeyMoveUnits);
    canvas->inputModifierKey(keys::Up, true);
    CHECK(view->camera().panY == 0.0f);

    // P, + and -, R
    canvas->inputCharacter('p');
    CHECK_FALSE(view->camera().perspective);
    canvas->inputCharacter('+');
    CHECK(view->camera().zSpacing == doctest::Approx(reset.zSpacing*tools::DepthView::ZSpacingStep));
    canvas->inputCharacter('-');
    canvas->inputCharacter('-');
    CHECK(view->camera().zSpacing == doctest::Approx(reset.zSpacing/tools::DepthView::ZSpacingStep));
    canvas->inputCharacter('R');
    CHECK(view->camera() == reset);

    // the transform handed over is the one the view built
    for(int i=0; i<16; ++i)
        CHECK(scene.slots.viewTransform()[i] == view->transform()[i]);
}

TEST_CASE("Depth view: a canvas deleted under its view closes it, with no call")
{
    test::StubRenderer stub;
    test::StubSkin skin{&stub};
    test::MirrorOnlyRenderer slots;
    Listener listener;
    controls::Canvas *canvas=new controls::Canvas(&skin);

    REQUIRE(tools::DepthView::open(canvas, &slots) != nullptr);
    tools::DepthView::onOpenChangedCaller.add(&listener, &Listener::changed);
    delete canvas;
    CHECK_FALSE(slots.hasViewTransform());
    CHECK(listener.calls.empty());

    // and the renderer takes another canvas's view
    controls::Canvas *other=new controls::Canvas(&skin);

    CHECK(tools::DepthView::open(other, &slots) != nullptr);
    tools::DepthView::close(other);
    delete other;
    tools::DepthView::onOpenChangedCaller.removeHandler(&listener);
}

#endif//GWENI_TEST_SLOT_RENDERER
