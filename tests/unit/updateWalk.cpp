/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The update phase enters only what changed. There is no per-control think()
 *  walk (animations run from anim::think(canvas), the active ones only), and
 *  Canvas::updateCanvas() renumbers the z indices only on a frame after the
 *  tree order changed: a child added, removed, reparented, deleted or moved
 *  among its siblings, or a skin Control replaced. UpdateStats::zVisited counts
 *  the controls the renumber entered (the path to what changed, a moved
 *  subtree whole, none for a removal), UpdateStats::zChanged those whose z it
 *  changed. After each change the z ranges are in tree order
 *  (test::zOrderViolation), exactly the controls whose z index changed carry
 *  StateChange_Geometry and are counted in zChanged, and an add or a removal
 *  changes the z of no control that was numbered before.
 *
 *  The first frame's renumber enters every control; a settled frame, a state
 *  change, a layout and a scale change enter none. The tree changes checked
 *  are bringToFront, sendToBack, bringNextToControl, an added, removed,
 *  reparented or delayed-deleted child and a replaced skin, each followed by
 *  one renumber. A cycling ProgressBar and a focused TextBox's caret still
 *  animate under a stepped platform::setTimeSource clock.
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

//! z index by control id, so a failed comparison prints readably
using ZMap=std::map<size_t, int>;

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

    //! Empty if the z ranges are in tree order (test::zOrderViolation).
    std::string zOrderViolation() const
    {
        return test::zOrderViolation(canvas, renderer.maxZ);
    }

    //! Runs the update phase after a tree-order change made on a settled scene
    //! and checks that the z ranges are in tree order again. \p before is
    //! zIndices() from before the change; \p visited the controls the renumber
    //! enters; \p fresh are the controls the change created, which carry
    //! StateChange_Created anyway; \p moved is false for a change that moves
    //! no control that was numbered (an add, a removal).
    void checkRenumber(const ZMap &before, size_t visited,
        const std::vector<controls::Base *> &fresh={}, bool moved=true)
    {
        CHECK(canvas->zOrderDirty());
        canvas->updateCanvas();
        const ZMap now=zIndices();

        CHECK(canvas->getUpdateStats().zVisited == visited);
        CHECK(zOrderViolation() == "");

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
        // a moved control takes a new z, and an add or a removal needs none
        // of the numbered ones to
        if(moved)
            CHECK(changed > 0);
        else
            CHECK(changed == 0);
        // the fresh ones go from the constructor's 0 to their number
        CHECK(canvas->getUpdateStats().zChanged == changed+fresh.size());

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
    CHECK(scene.canvas->getUpdateStats().zChanged == Scene::Controls+1);
    CHECK(scene.zOrderViolation() == "");

    scene.settle();
    CHECK(scene.frame().zVisited == 0);
    CHECK(scene.frame().zVisited == 0);
    CHECK(scene.canvas->getUpdateStats().zChanged == 0);
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
    CHECK(scene.zOrderViolation() == "");
}

TEST_CASE("an order change renumbers once, as a full numbering does")
{
    Scene scene;
    scene.settle();
    const ZMap before=scene.zIndices();

    SUBCASE("bringToFront on the first of several siblings")
    {
        scene.children[0]->bringToFront();
        // the canvas, the branch, the child and its two leaves
        scene.checkRenumber(before, 3+Scene::Leaves);
    }

    SUBCASE("sendToBack on the last of several siblings")
    {
        scene.children[Scene::Children-1]->sendToBack();
        scene.checkRenumber(before, 3+Scene::Leaves);
    }

    SUBCASE("bringNextToControl")
    {
        controls::Base *branch=scene.branches[1];
        controls::Base *first=branch->getActualChildren().front();
        controls::Base *last=branch->getActualChildren().back();
        first->bringNextToControl(last, false);
        scene.checkRenumber(before, 3+Scene::Leaves);
    }

    SUBCASE("bringToFront on a whole branch")
    {
        scene.branches[0]->bringToFront();
        // the canvas and the whole branch
        scene.checkRenumber(before, 2+Scene::Children*(1+Scene::Leaves));
    }

    SUBCASE("a child added in the middle")
    {
        controls::Base *added=Scene::add(scene.children[2], 2, true);
        // the canvas, the branch, the child and the new one
        scene.checkRenumber(before, 4, {added}, false);
    }

    SUBCASE("a child removed")
    {
        controls::Base *removed=scene.children[1];
        removed->setParent(nullptr);
        // a removal leaves a gap: nothing to enter
        scene.checkRenumber(before, 0, {}, false);
        // deleting it once it is off the canvas changes nothing there
        delete removed;
        CHECK_FALSE(scene.canvas->zOrderDirty());
        CHECK(scene.frame().zVisited == 0);
    }

    SUBCASE("a child reparented to another branch")
    {
        scene.children[0]->setParent(scene.branches[2]);
        // the canvas, the new branch, the child and its leaves; the old
        // branch only lost a child
        scene.checkRenumber(before, 3+Scene::Leaves);
    }

    SUBCASE("a delayed delete")
    {
        scene.children[4]->delayedDelete();
        // nothing is renumbered until the delete is processed
        CHECK_FALSE(scene.canvas->zOrderDirty());
        scene.canvas->updateCanvas();
        CHECK(scene.canvas->getUpdateStats().zVisited == 0);
        CHECK(scene.canvas->getUpdateStats().zChanged == 0);
        CHECK(scene.zOrderViolation() == "");
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
    CHECK(scene.zOrderViolation() == "");
    CHECK(scene.frame().zVisited == 0);

    // given back before `other` goes
    scene.children[6]->setSkin(&scene.skin, true);
    scene.frame();
    CHECK(scene.zOrderViolation() == "");
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

    controls::TextBox *box=scene.canvas->newChild<controls::TextBox>("box");
    box->setBounds(0, 100, 100, 20);
    box->focus();
    REQUIRE(box->isFocussed());
    scene.frame();
    scene.frame();
    // the caret stays lit for 1.5 s after the focus arrives
    scene.clock+=1.0f;
    scene.canvas->updateCanvas();
    CHECK(box->isCaretVisible());
    CHECK(box->getStateChange() == controls::StateChange_Nothing);
    scene.canvas->renderCanvas();
    // from then on it blinks every 0.5 s, each edge a StateChange_Caret alone
    scene.clock+=1.0f;
    scene.frame();

    for(int i=0; i<4; ++i)
    {
        CAPTURE(i);
        const bool caret=box->isCaretVisible();
        scene.clock+=0.6f;

        scene.canvas->updateCanvas();
        CHECK(scene.canvas->getUpdateStats().zVisited == 0);
        CHECK(box->isCaretVisible() != caret);
        CHECK(box->getStateChange() == controls::StateChange_Caret);
        scene.canvas->renderCanvas();

        // nothing between two edges
        scene.clock+=0.1f;
        scene.canvas->updateCanvas();
        CHECK(box->getStateChange() == controls::StateChange_Nothing);
        scene.canvas->renderCanvas();
    }

    // a cursor move lights it and holds it lit
    box->moveCaretToStart();
    CHECK(box->isCaretVisible());
    scene.frame();
    scene.clock+=1.0f;
    scene.canvas->updateCanvas();
    CHECK(box->isCaretVisible());
    CHECK(box->getStateChange() == controls::StateChange_Nothing);
    scene.canvas->renderCanvas();

    // losing the focus is one state change; unfocused, the box has none at all
    box->unfocus();
    CHECK(box->getStateChange() == controls::StateChange_Background);
    scene.frame();
    for(int i=0; i<3; ++i)
    {
        scene.clock+=0.6f;
        scene.canvas->updateCanvas();
        CHECK(box->getStateChange() == controls::StateChange_Nothing);
        scene.canvas->renderCanvas();
    }
}
