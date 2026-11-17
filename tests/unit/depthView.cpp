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

//! A skin Control that draws its control's bounds as one filled slot at its
//! z, hidden while the control is not visible(), so the renderer's layer table
//! counts the shown controls
class SlotControl: public skin::Control
{
public:
    std::string getTypeName() override { return "Slot"; }
    size_t requiredPrimitives() override { return 1; }

    void update(renderer::Base *render, controls::Base *control) override
    {
        if(m_primitives.empty())
            return;
        if(!control->visible())
        {
            render->hidePrimitive(m_primitives[0]);
            return;
        }

        const Rect &bounds=control->getBounds();

        render->setDrawColor(Color(10, 20, 30, 255));
        render->drawFilledRect(m_primitives[0], Rect(0, 0, bounds.w, bounds.h), control->getZIndex());
    }
};

//! A skin drawing every control as a SlotControl on its renderer
class DrawingSkin: public skin::Base
{
public:
    explicit DrawingSkin(renderer::Base *renderer): Base(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string) override
    {
        return std::make_unique<SlotControl>();
    }
    void drawControl(controls::Base *) override {}
};

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
    DrawingSkin drawing{&slots};
    controls::Canvas *canvas=nullptr;
    Listener listener;

    //! A canvas filling a \p width x \p height view, its skin the stub's, or
    //! with \p drawn one drawing each control as a slot on `slots`
    explicit Scene(int width=640, int height=480, bool drawn=false)
    {
        slots.setViewRect(Rect(0, 0, width, height));
        canvas=new controls::Canvas(drawn?static_cast<skin::Base *>(&drawing):&skin);
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

        Scene scene(size.x, size.y, true);
        tools::DepthView *view=tools::DepthView::open(scene.canvas, &scene.slots);

        REQUIRE(view != nullptr);

        const float w=(float)size.x, h=(float)size.y;
        const float cx=0.5f*w, cy=0.5f*h;
        // the highest shown layer: the canvas and its 20 buttons' slots, the
        // buttons' own parts drawing too
        const float high=view->highestZ();

        CHECK(high == (float)(scene.slots.shownLayerCount()-1));
        CHECK(high < (float)scene.canvas->highestZIndex());
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
    Scene scene(640, 480, true);
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

    // L: raw labels and back, the stack's depth (spacing times its highest z)
    // kept, so its front layer stays
    const float shownHigh=view->highestZ();

    CHECK(scene.slots.layerRemap());
    canvas->inputCharacter('l');
    CHECK(view->rawLabels());
    CHECK_FALSE(scene.slots.layerRemap());
    CHECK(view->highestZ() == (float)canvas->highestZIndex());
    CHECK(view->highestZ() > shownHigh);
    CHECK(view->camera().zSpacing*view->highestZ() == doctest::Approx(reset.zSpacing*shownHigh));
    CHECK(view->camera().distance == reset.distance);
    canvas->inputCharacter('L');
    CHECK_FALSE(view->rawLabels());
    CHECK(scene.slots.layerRemap());
    CHECK(view->highestZ() == shownHigh);
    CHECK(view->camera().zSpacing == doctest::Approx(reset.zSpacing));

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

namespace
{

//! A drawn canvas: a shown control, a container of \p hidden controls hidden
//! from the start, and another shown control after it
struct HiddenSubtree
{
    test::MirrorOnlyRenderer slots;
    DrawingSkin skin{&slots};
    controls::Canvas *canvas=nullptr;
    controls::Base *container=nullptr;

    explicit HiddenSubtree(int hidden)
    {
        slots.setViewRect(Rect(0, 0, 640, 480));
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);

        controls::Base *first=canvas->newChild<controls::Base>();

        first->setBounds(10, 10, 100, 20);
        container=canvas->newChild<controls::Base>();
        container->setBounds(10, 40, 300, 300);
        container->hide();
        for(int i=0; i<hidden; ++i)
            container->newChild<controls::Base>()->setBounds(i%30, i/30, 10, 10);

        controls::Base *last=canvas->newChild<controls::Base>();

        last->setBounds(10, 400, 100, 20);
        frame();
    }

    HiddenSubtree(const HiddenSubtree &)=delete;
    HiddenSubtree &operator=(const HiddenSubtree &)=delete;

    ~HiddenSubtree()
    {
        tools::DepthView::close(canvas);
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

}//namespace

TEST_CASE("Depth view: a hidden subtree takes no depth and leaves the reset camera as it is")
{
    HiddenSubtree none(0);
    tools::DepthView *noneView=tools::DepthView::open(none.canvas, &none.slots);

    REQUIRE(noneView != nullptr);
    CHECK(none.slots.layerTable());
    CHECK(none.slots.layerRemap());

    // the two shown controls (the canvas is at z 0, layer 0)
    const float shownHigh=noneView->highestZ();
    const tools::DepthView::Camera reference=noneView->camera();

    CHECK(shownHigh == 2.0f);

    for(int hidden:{ 1, 10, 1000 })
    {
        CAPTURE(hidden);

        HiddenSubtree scene(hidden);
        tools::DepthView *view=tools::DepthView::open(scene.canvas, &scene.slots);

        REQUIRE(view != nullptr);
        // the hidden controls have z labels of their own, and no depth
        CHECK(scene.canvas->highestZIndex() > none.canvas->highestZIndex());
        CHECK(view->highestZ() == shownHigh);
        CHECK(view->camera() == reference);
        CHECK(view->camera() == view->resetCamera());

        // shown, every control of it is a layer; hidden again, none is
        scene.container->show();
        scene.frame();
        CHECK(view->highestZ() == shownHigh+1.0f+(float)hidden);
        // the camera at reset follows them, fitted in the flush
        CHECK(view->camera() == view->resetCamera());
        CHECK(view->camera().zSpacing == doctest::Approx(
            std::min(0.5f*640.0f/view->highestZ(), tools::DepthView::MaxZSpacing)));
        scene.container->hide();
        scene.frame();
        CHECK(view->highestZ() == shownHigh);
        CHECK(view->camera() == reference);

        // in raw labels it is the canvas's z range
        view->setRawLabels(true);
        CHECK(view->highestZ() == (float)scene.canvas->highestZIndex());
    }

    // closed, the renderer keeps no table
    tools::DepthView::close(none.canvas);
    CHECK_FALSE(none.slots.layerTable());
    CHECK_FALSE(none.slots.layerRemap());
}

TEST_CASE("Depth view: opened before the canvas's first frame, the camera fits the first table built")
{
    test::MirrorOnlyRenderer slots;
    DrawingSkin skin{&slots};
    controls::Canvas *canvas=new controls::Canvas(&skin);

    slots.setViewRect(Rect(0, 0, 640, 480));
    canvas->setSize(640, 480);
    for(int i=0; i<12; ++i)
        canvas->newChild<controls::Base>()->setBounds(10+i*20, 10+i*10, 100, 20);
    // as capture::openDepthViewOnce does: after the update, before the render
    canvas->updateCanvas();

    tools::DepthView *view=tools::DepthView::open(canvas, &slots);

    REQUIRE(view != nullptr);
    // nothing drawn yet: the least stack
    CHECK(view->highestZ() == 1.0f);

    canvas->renderCanvas();
    // the 12 controls, fitted in that flush
    CHECK(view->highestZ() == 12.0f);
    CHECK(view->camera() == view->resetCamera());
    CHECK(view->camera().zSpacing == doctest::Approx(0.5f*640.0f/12.0f));
    for(int i=0; i<16; ++i)
        CHECK(slots.viewTransform()[i] == view->transform()[i]);

    // a camera the user moved keeps itself, its depth range following the stack
    canvas->inputMouseButton(0, true);
    canvas->inputMouseMoved(310, 205, 10, 5);
    canvas->inputMouseButton(0, false);

    const tools::DepthView::Camera turned=view->camera();
    const std::vector<float> before(view->transform(), view->transform()+16);

    canvas->newChild<controls::Base>()->setBounds(300, 300, 50, 50);
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(view->highestZ() == 13.0f);
    CHECK(view->camera() == turned);
    CHECK_FALSE(std::equal(before.begin(), before.end(), view->transform()));
    for(int i=0; i<16; ++i)
        CHECK(slots.viewTransform()[i] == view->transform()[i]);

    delete canvas;
    CHECK_FALSE(slots.layerTable());
}

#endif//GWENI_TEST_SLOT_RENDERER
