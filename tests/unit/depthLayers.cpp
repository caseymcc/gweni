/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Base::renumberZRecursive numbers the controls depth-first, advancing the z
 *  index by each control's skin::Control::depthLayers(), so a control whose
 *  skin draws an overlay at getZIndex()+1 keeps that depth to itself: its
 *  first child comes after its layers, its next sibling after its subtree.
 *  ProgressBar is such a control (the fill over the track), and setProgress
 *  raises StateChange_Background only when the value changes.
 *
 *  The Control reporting depthLayers() 2 comes from StubSkin::depthLayers, by
 *  type name: it puts its first child at its z+2; controls at 1 step by 1. The
 *  value setProgress compares is the clamped one, and its animation goes
 *  through the same check.
 */
#include "headlessFixture.h"

#include <gweni/controls/progressBar.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

//! A control type the test gives its own depthLayers() count.
class Layered: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Layered, controls::Base)
    {}
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

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

}//namespace

TEST_CASE("a skin Control's depthLayers() reserves z values before its children and siblings")
{
    Scene scene;
    scene.skin.depthLayers[Layered::getTypeNameStatic()]=2;

    controls::Base *before=scene.canvas->newChild<controls::Base>("before");
    controls::Base *layered=scene.canvas->newChild<Layered>("layered");
    controls::Base *first=layered->newChild<controls::Base>("first");
    controls::Base *grandchild=first->newChild<controls::Base>("grandchild");
    controls::Base *nested=layered->newChild<Layered>("nested");
    controls::Base *nestedChild=nested->newChild<controls::Base>("nestedChild");
    controls::Base *after=scene.canvas->newChild<controls::Base>("after");

    scene.frame();

    const int canvasZ=scene.canvas->getZIndex();

    // one layer each: +1 steps
    CHECK(before->getZIndex() == canvasZ+1);
    CHECK(layered->getZIndex() == before->getZIndex()+1);
    // two layers: the first child after both
    CHECK(first->getZIndex() == layered->getZIndex()+2);
    CHECK(grandchild->getZIndex() == first->getZIndex()+1);
    CHECK(nested->getZIndex() == grandchild->getZIndex()+1);
    CHECK(nestedChild->getZIndex() == nested->getZIndex()+2);
    // the next sibling after the whole subtree
    CHECK(after->getZIndex() == nestedChild->getZIndex()+1);

    SUBCASE("a count changed after the first frame renumbers on the next update")
    {
        // the stub's count changes under its live Controls, which a real skin's
        // never does, so the canvas has to be told
        scene.skin.depthLayers[Layered::getTypeNameStatic()]=3;
        scene.canvas->invalidateZOrder();
        const int layeredZ=layered->getZIndex();

        scene.frame();

        CHECK(layered->getZIndex() == layeredZ);
        CHECK(first->getZIndex() == layeredZ+3);
        CHECK(nestedChild->getZIndex() == nested->getZIndex()+3);
        CHECK(after->getZIndex() == nestedChild->getZIndex()+1);
    }
}

TEST_CASE("with every Control at one layer the numbering is one z per control, depth-first")
{
    Scene scene;

    controls::Base *a=scene.canvas->newChild<controls::Base>("a");
    controls::Base *a1=a->newChild<controls::Base>("a1");
    controls::Base *a2=a->newChild<controls::Base>("a2");
    controls::Base *b=scene.canvas->newChild<controls::Base>("b");

    scene.frame();

    const int canvasZ=scene.canvas->getZIndex();

    CHECK(canvasZ == 1);
    CHECK(a->getZIndex() == canvasZ+1);
    CHECK(a1->getZIndex() == canvasZ+2);
    CHECK(a2->getZIndex() == canvasZ+3);
    CHECK(b->getZIndex() == canvasZ+4);
}

TEST_CASE("ProgressBar::setProgress raises StateChange_Background only for a new value")
{
    for(const bool autoLabel : {true, false})
    {
        CAPTURE(autoLabel);
        Scene scene;

        controls::ProgressBar *bar=scene.canvas->newChild<controls::ProgressBar>("bar");
        bar->setAutoLabel(autoLabel);
        bar->setProgress(0.25f);

        scene.frame();
        scene.frame();
        REQUIRE(bar->getStateChange() == controls::StateChange_Nothing);

        bar->setProgress(0.5f);
        CHECK((bar->getStateChange()&controls::StateChange_Background) != 0);
        CHECK(bar->getProgress() == 0.5f);

        scene.frame();
        REQUIRE(bar->getStateChange() == controls::StateChange_Nothing);

        bar->setProgress(0.5f);
        CHECK(bar->getStateChange() == controls::StateChange_Nothing);

        // a value outside 0..1 is clamped before it is compared
        bar->setProgress(1.0f);
        scene.frame();
        bar->setProgress(2.0f);
        CHECK(bar->getStateChange() == controls::StateChange_Nothing);
        CHECK(bar->getProgress() == 1.0f);
    }
}

TEST_CASE("an animated ProgressBar raises StateChange_Background and its auto label follows")
{
    Scene scene;

    controls::ProgressBar *bar=scene.canvas->newChild<controls::ProgressBar>("bar");
    bar->setProgress(0.9f);
    bar->setCycleSpeed(0.5f);

    scene.frame();
    scene.frame();
    bar->setCycleSpeed(0.0f);
    scene.frame();
    REQUIRE(bar->getStateChange() == controls::StateChange_Nothing);
    bar->setCycleSpeed(0.5f);

    // 0.9+0.5*0.3 wraps around to 0.05
    const float start=bar->getProgress();
    bar->cycleThink(0.3f);
    const float expected=start+0.15f-(start+0.15f > 1.0f?1.0f:0.0f);

    CHECK(bar->getProgress() == doctest::Approx(expected));
    CHECK((bar->getStateChange()&controls::StateChange_Background) != 0);
    CHECK(bar->getText() == std::to_string((int)(bar->getProgress()*100))+"%");
}
