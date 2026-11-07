/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The update phase enters only what changed. There is no per-control think()
 *  walk (animations run from anim::think(), keyed by control), and
 *  Canvas::updateCanvas() renumbers the z indices only on a frame after the
 *  tree order changed: a child added, removed, reparented, deleted or moved
 *  among its siblings, or a skin Control replaced. UpdateStats::zVisited counts
 *  the controls the renumber entered. After each change the z indices are the
 *  ones a full depth-first numbering gives, and exactly the controls whose z
 *  index changed carry StateChange_Geometry.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/progressBar.h>
#include <gweni/controls/textBox.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <vector>

using namespace gweni;

namespace
{

//! A control type the test gives two depth layers.
class Layered: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Layered, controls::Base)
    {}
};

//! A TextBox that shows its caret colour, which its animation toggles.
class CaretBox: public controls::TextBox
{
public:
    GWENI_CONTROL_INLINE(CaretBox, controls::TextBox)
    {}

    Color caretColor() const { return m_caretColor; }
};

//! z index by control id, so a failed comparison prints readably
using ZMap=std::map<size_t, int>;

//! The z index each control gets from the canvas's numbering: depth-first from
//! 1, each control taking its skin Control's depthLayers(). The walk
//! Canvas::updateCanvas() used to run every frame.
void fullNumbering(controls::Base *control, int &zIndex, ZMap &result)
{
    result[control->getControlId()]=zIndex;
    const skin::Control *skinControl=control->getSkinControl();
    zIndex+=skinControl?(int)const_cast<skin::Control *>(skinControl)->depthLayers():1;

    for(controls::Base *child : control->getActualChildren())
        fullNumbering(child, zIndex, result);
}

//! A canvas with three branches of four children of two leaves each (36
//! controls below the canvas); every second child is Layered.
struct Scene
{
    static constexpr int Branches=3;
    static constexpr int Children=4;
    static constexpr int Leaves=2;
    static constexpr size_t Controls=Branches+Branches*Children+Branches*Children*Leaves;

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    std::vector<controls::Base *> branches;
    std::vector<controls::Base *> children;
    float clock=10.0f;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        skin.depthLayers[Layered::getTypeNameStatic()]=2;

        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        for(int b=0; b<Branches; ++b)
        {
            controls::Base *branch=add(canvas, b, false);
            branches.push_back(branch);

            for(int c=0; c<Children; ++c)
            {
                controls::Base *child=add(branch, c, (c%2) == 1);
                children.push_back(child);

                for(int l=0; l<Leaves; ++l)
                    add(child, l, false);
            }
        }
    }

    ~Scene()
    {
        KeyboardFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    static controls::Base *add(controls::Base *parent, int index, bool layered)
    {
        controls::Base *control=layered
            ? static_cast<controls::Base *>(parent->newChild<Layered>())
            : parent->newChild<controls::Base>();
        control->setBounds(index*5, index*5, 50, 20);
        return control;
    }

    const controls::UpdateStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getUpdateStats();
    }

    void settle()
    {
        for(int i=0; i<10; ++i)
        {
            controls::resetChangeStats();
            frame();
            if(controls::getChangeStats().controls == 0)
                break;
        }
    }

    //! every control on the canvas, depth-first
    std::vector<controls::Base *> all() const
    {
        std::vector<controls::Base *> result;
        collect(canvas, result);
        return result;
    }

    static void collect(controls::Base *control, std::vector<controls::Base *> &result)
    {
        result.push_back(control);
        for(controls::Base *child : control->getActualChildren())
            collect(child, result);
    }

    ZMap zIndices() const
    {
        ZMap result;
        for(controls::Base *control : all())
            result[control->getControlId()]=control->getZIndex();
        return result;
    }

    ZMap expected() const
    {
        ZMap result;
        int zIndex=1;
        fullNumbering(canvas, zIndex, result);
        return result;
    }

    //! Runs the update phase after a tree-order change made on a settled scene
    //! and checks it against a full numbering. \p before is zIndices() from
    //! before the change; \p fresh are the controls the change created, which
    //! carry StateChange_Created anyway.
    void checkRenumber(const ZMap &before, const std::vector<controls::Base *> &fresh={})
    {
        CHECK(canvas->zOrderDirty());
        canvas->updateCanvas();
        const ZMap now=zIndices();

        CHECK(canvas->getUpdateStats().zVisited == now.size());
        CHECK(now == expected());

        size_t changed=0;
        for(controls::Base *control : all())
        {
            if(std::find(fresh.begin(), fresh.end(), control) != fresh.end())
                continue;

            CAPTURE(control->getControlId());
            const auto old=before.find(control->getControlId());
            REQUIRE(old != before.end());
            const bool zChanged=old->second != control->getZIndex();
            const bool geometry=(control->getStateChange()&controls::StateChange_Geometry) != 0;
            CHECK(geometry == zChanged);
            changed+=zChanged?1:0;
        }
        // every case moves something past something else
        CHECK(changed > 0);

        canvas->renderCanvas();
        CHECK_FALSE(canvas->zOrderDirty());
        CHECK(frame().zVisited == 0);
    }
};

}//namespace

TEST_CASE("the first frame numbers every control, a settled one enters none")
{
    Scene scene;

    CHECK(scene.frame().zVisited == Scene::Controls+1);
    CHECK(scene.zIndices() == scene.expected());

    scene.settle();
    CHECK(scene.frame().zVisited == 0);
    CHECK(scene.frame().zVisited == 0);
}

TEST_CASE("a state change, a layout or a scale change does not renumber")
{
    Scene scene;
    scene.settle();

    scene.children[3]->addStateChange(controls::StateChange_Background);
    CHECK(scene.frame().zVisited == 0);

    scene.children[5]->setBounds(20, 20, 80, 30);
    CHECK(scene.frame().zVisited == 0);

    scene.canvas->setScale(2.0f);
    CHECK(scene.frame().zVisited == 0);
    CHECK(scene.zIndices() == scene.expected());
}

TEST_CASE("an order change renumbers once, as a full numbering does")
{
    Scene scene;
    scene.settle();
    const ZMap before=scene.zIndices();

    SUBCASE("bringToFront on the first of several siblings")
    {
        scene.children[0]->bringToFront();
        scene.checkRenumber(before);
    }

    SUBCASE("sendToBack on the last of several siblings")
    {
        scene.children[Scene::Children-1]->sendToBack();
        scene.checkRenumber(before);
    }

    SUBCASE("bringNextToControl")
    {
        controls::Base *branch=scene.branches[1];
        controls::Base *first=branch->getActualChildren().front();
        controls::Base *last=branch->getActualChildren().back();
        first->bringNextToControl(last, false);
        scene.checkRenumber(before);
    }

    SUBCASE("bringToFront on a whole branch")
    {
        scene.branches[0]->bringToFront();
        scene.checkRenumber(before);
    }

    SUBCASE("a child added in the middle")
    {
        controls::Base *added=Scene::add(scene.children[2], 2, true);
        scene.checkRenumber(before, {added});
    }

    SUBCASE("a child removed")
    {
        controls::Base *removed=scene.children[1];
        removed->setParent(nullptr);
        scene.checkRenumber(before);
        // deleting it once it is off the canvas changes nothing there
        delete removed;
        CHECK_FALSE(scene.canvas->zOrderDirty());
        CHECK(scene.frame().zVisited == 0);
    }

    SUBCASE("a child reparented to another branch")
    {
        scene.children[0]->setParent(scene.branches[2]);
        scene.checkRenumber(before);
    }

    SUBCASE("a delayed delete")
    {
        scene.children[4]->delayedDelete();
        // nothing is renumbered until the delete is processed
        CHECK_FALSE(scene.canvas->zOrderDirty());
        scene.canvas->updateCanvas();
        CHECK(scene.canvas->getUpdateStats().zVisited == Scene::Controls+1-(1+Scene::Leaves));
        CHECK(scene.zIndices() == scene.expected());
        scene.canvas->renderCanvas();
        CHECK(scene.frame().zVisited == 0);
    }
}

TEST_CASE("a replaced skin Control renumbers, for its depthLayers() may differ")
{
    Scene scene;
    scene.settle();

    test::StubSkin other{&scene.renderer};
    other.depthLayers[controls::Base::getTypeNameStatic()]=3;

    scene.children[6]->setSkin(&other, true);
    CHECK(scene.canvas->zOrderDirty());
    scene.frame();
    CHECK(scene.zIndices() == scene.expected());
    CHECK(scene.frame().zVisited == 0);

    // given back before `other` goes
    scene.children[6]->setSkin(&scene.skin, true);
    scene.frame();
    CHECK(scene.zIndices() == scene.expected());
}

TEST_CASE("an animated ProgressBar runs without a tree walk")
{
    Scene scene;

    controls::ProgressBar *bar=scene.canvas->newChild<controls::ProgressBar>("bar");
    bar->setProgress(0.1f);
    bar->setCycleSpeed(0.5f);
    scene.frame();
    scene.frame();

    for(int i=0; i<4; ++i)
    {
        CAPTURE(i);
        const float progress=bar->getProgress();
        scene.clock+=0.2f;

        scene.canvas->updateCanvas();
        CHECK(scene.canvas->getUpdateStats().zVisited == 0);
        // 0.5 per second
        CHECK(bar->getProgress() == doctest::Approx(progress+0.1f));
        CHECK((bar->getStateChange()&controls::StateChange_Background) != 0);
        scene.canvas->renderCanvas();
    }
}

TEST_CASE("a focused TextBox's caret blinks without a tree walk")
{
    Scene scene;

    CaretBox *box=scene.canvas->newChild<CaretBox>("box");
    box->setBounds(0, 100, 100, 20);
    box->focus();
    REQUIRE(box->isFocussed());
    scene.frame();
    scene.frame();
    // past the 1.5 s the caret stays lit after the box is laid out; from then
    // on it blinks every 0.5 s
    scene.clock+=2.0f;
    scene.frame();

    for(int i=0; i<4; ++i)
    {
        CAPTURE(i);
        const Color caret=box->caretColor();
        scene.clock+=0.6f;

        scene.canvas->updateCanvas();
        CHECK(scene.canvas->getUpdateStats().zVisited == 0);
        CHECK_FALSE((box->caretColor() == caret));
        CHECK((box->getStateChange()&controls::StateChange_Background) != 0);
        scene.canvas->renderCanvas();
    }

    // an unfocused box keeps its caret
    box->unfocus();
    scene.frame();
    const Color caret=box->caretColor();
    for(int i=0; i<3; ++i)
    {
        scene.clock+=0.6f;
        scene.frame();
    }
    CHECK((box->caretColor() == caret));
}
