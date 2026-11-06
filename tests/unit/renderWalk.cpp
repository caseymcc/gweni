/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The render walk enters only the controls with a pending state change and
 *  their ancestors: addStateChange() marks the path to the root, attaching a
 *  pending control marks its new parent's, and doRender() skips a subtree with
 *  nothing pending in it. ChangeStats::visited counts the controls entered.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <functional>
#include <set>
#include <vector>

using namespace gweni;

namespace
{

//! What the walk did in one frame: the controls it entered (renderUnder() runs
//! once per control renderRecursive() enters) and the ones render() found a
//! state change on.
struct WalkLog
{
    std::vector<controls::Base *> entered;
    std::vector<controls::Base *> rendered;

    void clear()
    {
        entered.clear();
        rendered.clear();
    }

    bool wasRendered(controls::Base *control) const
    {
        return std::find(rendered.begin(), rendered.end(), control) != rendered.end();
    }
};

//! A plain control that logs the walk, and can run a hook from its render().
class Probe: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Probe, controls::Base)
    {}

    WalkLog *log=nullptr;
    std::function<void()> onRender;

    void renderUnder(skin::Base *skin) override
    {
        if(log)
            log->entered.push_back(this);
        ParentClass::renderUnder(skin);
    }

    void render(skin::Base *skin) override
    {
        if(log && getStateChange() != controls::StateChange_Nothing)
            log->rendered.push_back(this);
        if(onRender)
            onRender();
        ParentClass::render(skin);
    }
};

//! A canvas with three branches of four children of three leaves each: 51
//! Probes below the canvas, 52 controls in all.
struct Scene
{
    static constexpr int Branches=3;
    static constexpr int Children=4;
    static constexpr int Leaves=3;
    static constexpr size_t Controls=Branches+Branches*Children+Branches*Children*Leaves;

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    WalkLog log;
    std::vector<Probe *> branches;
    std::vector<Probe *> children;
    std::vector<Probe *> leaves;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        for(int b=0; b<Branches; ++b)
        {
            Probe *branch=add(canvas, b);
            branches.push_back(branch);

            for(int c=0; c<Children; ++c)
            {
                Probe *child=add(branch, c);
                children.push_back(child);

                for(int l=0; l<Leaves; ++l)
                    leaves.push_back(add(child, l));
            }
        }
    }

    ~Scene()
    {
        delete canvas;
    }

    Probe *add(controls::Base *parent, int index)
    {
        Probe *probe=parent->newChild<Probe>();
        probe->log=&log;
        probe->setBounds(index*5, index*5, 50, 20);
        return probe;
    }

    const controls::ChangeStats &frame()
    {
        log.clear();
        controls::resetChangeStats();
        canvas->updateCanvas();
        canvas->renderCanvas();
        return controls::getChangeStats();
    }

    //! Frames until one changes nothing, so a case starts from a settled tree.
    void settle()
    {
        for(int i=0; i<10 && frame().controls != 0; ++i)
            ;
    }

    //! control and each of its ancestors below the canvas
    static std::set<controls::Base *> path(controls::Base *control)
    {
        std::set<controls::Base *> result;
        for(; control && control->getParent(); control=control->getParent())
            result.insert(control);
        return result;
    }

    //! the controls rendered this frame and their ancestors below the canvas:
    //! what the walk should have entered
    std::set<controls::Base *> renderedPaths() const
    {
        std::set<controls::Base *> result;
        for(controls::Base *control : log.rendered)
        {
            const std::set<controls::Base *> controlPath=path(control);
            result.insert(controlPath.begin(), controlPath.end());
        }
        return result;
    }

    std::set<controls::Base *> entered() const
    {
        return std::set<controls::Base *>(log.entered.begin(), log.entered.end());
    }
};

void checkSettled(const controls::ChangeStats &stats)
{
    CHECK(stats.visited <= 1);
    CHECK(stats.controls == 0);
    CHECK(stats.created == 0);
    CHECK(stats.visibility == 0);
    CHECK(stats.geometry == 0);
    CHECK(stats.background == 0);
    CHECK(stats.text == 0);
}

}//namespace

TEST_CASE("the first frame visits every control, a settled one at most the canvas")
{
    Scene scene;

    const controls::ChangeStats &first=scene.frame();
    CHECK(first.visited == Scene::Controls+1);
    CHECK(scene.log.entered.size() == Scene::Controls);
    // the canvas's own render() does not count itself
    CHECK(first.created == Scene::Controls);

    scene.settle();
    checkSettled(scene.frame());
    CHECK(scene.log.entered.empty());
}

TEST_CASE("a change on a deep leaf visits the leaf and its ancestors only")
{
    Scene scene;
    scene.settle();

    Probe *leaf=scene.leaves[17];
    leaf->addStateChange(controls::StateChange_Background);

    const controls::ChangeStats &stats=scene.frame();
    // leaf, child, branch and the canvas
    CHECK(stats.visited == 4);
    CHECK(stats.controls == 1);
    CHECK(stats.background == 1);
    CHECK(scene.entered() == Scene::path(leaf));
    CHECK(scene.log.wasRendered(leaf));

    checkSettled(scene.frame());
}

TEST_CASE("addStateChangeRecursive visits the whole subtree and its ancestors")
{
    Scene scene;
    scene.settle();

    Probe *child=scene.children[5];
    child->addStateChangeRecursive(controls::StateChange_Geometry);

    const controls::ChangeStats &stats=scene.frame();
    std::set<controls::Base *> expected=Scene::path(child);
    for(controls::Base *leaf : child->getChildren())
        expected.insert(leaf);

    CHECK(scene.entered() == expected);
    CHECK(stats.visited == expected.size()+1);
    CHECK(stats.controls == 1+Scene::Leaves);
    CHECK(stats.geometry == 1+Scene::Leaves);

    checkSettled(scene.frame());
}

TEST_CASE("a child added to a settled tree is generated on the next frame")
{
    Scene scene;
    scene.settle();

    Probe *parent=scene.children[9];
    Probe *added=scene.add(parent, 3);
    CHECK(scene.renderer.generatedBy.count(added) == 0);

    scene.frame();
    CHECK(scene.renderer.generatedBy.count(added) == 1);
    CHECK(scene.log.wasRendered(added));
    CHECK(scene.entered().count(parent) == 1);

    scene.settle();
    checkSettled(scene.frame());
}

TEST_CASE("a change made while a subtree is detached is rendered once it is back")
{
    Scene scene;
    scene.settle();

    // the last child of the last branch, so re-attaching it to the end of the
    // same branch leaves every z index as it was: only the marked path brings
    // the walk down to its leaf
    Probe *branch=scene.branches.back();
    Probe *child=scene.children.back();
    Probe *leaf=static_cast<Probe *>(child->getChildren().front());

    child->setParent(nullptr);
    scene.frame();
    leaf->addStateChange(controls::StateChange_Background);
    scene.frame();
    CHECK_FALSE(scene.log.wasRendered(leaf));

    // re-attaching lays the branch out again, which changes more than the
    // leaf; the walk still enters only what changed and its ancestors
    child->setParent(branch);
    scene.frame();
    CHECK(scene.log.wasRendered(leaf));
    CHECK(scene.entered() == scene.renderedPaths());

    checkSettled(scene.frame());
}

TEST_CASE("a settled control moved under another branch is rendered on the next frame")
{
    Scene scene;
    scene.settle();

    // the first branch's first leaf, to the last branch's last child: its z
    // index changes, so it has a geometry change to render in its new place
    Probe *leaf=scene.leaves.front();
    Probe *target=scene.children.back();
    leaf->setParent(target);

    const controls::ChangeStats &stats=scene.frame();
    CHECK(scene.log.wasRendered(leaf));
    CHECK(stats.geometry >= 1);
    CHECK(scene.entered().count(target) == 1);

    // and one with a pending change, moved before the frame that renders it
    Probe *other=scene.leaves[4];
    other->addStateChange(controls::StateChange_Background);
    other->setParent(scene.children[0]);
    scene.frame();
    CHECK(scene.log.wasRendered(other));

    scene.settle();
    checkSettled(scene.frame());
}

TEST_CASE("a change raised during the walk is rendered this frame or the next")
{
    Scene scene;
    scene.settle();

    // raised from the middle branch's render() on a leaf the walk has already
    // passed, and on one it has not reached yet
    Probe *passed=scene.leaves[2];
    Probe *ahead=scene.leaves.back();
    Probe *raiser=scene.branches[1];
    bool raise=true;
    raiser->onRender=[&]()
        {
            if(!raise)
                return;
            raise=false;
            passed->addStateChange(controls::StateChange_Background);
            ahead->addStateChange(controls::StateChange_Background);
        };
    raiser->addStateChange(controls::StateChange_Background);

    scene.frame();
    const bool passedNow=scene.log.wasRendered(passed);
    CHECK(scene.log.wasRendered(ahead));

    scene.frame();
    CHECK((passedNow || scene.log.wasRendered(passed)));
    CHECK_FALSE(scene.log.wasRendered(ahead));

    scene.settle();
    checkSettled(scene.frame());
}

TEST_CASE("a change on a child a sibling's render() moves to the back is not lost")
{
    Scene scene;
    scene.settle();

    // a render() that reorders its parent's children (sendToBack(), as some
    // controls do from a state change) and raises a change on a sibling
    Probe *branch=scene.branches[0];
    Probe *mover=static_cast<Probe *>(branch->getChildren().back());
    Probe *sibling=static_cast<Probe *>(branch->getChildren().front());
    Probe *leaf=static_cast<Probe *>(sibling->getChildren().front());
    bool once=true;
    mover->onRender=[&]()
        {
            if(!once)
                return;
            once=false;
            mover->sendToBack();
            leaf->addStateChange(controls::StateChange_Text);
        };
    mover->addStateChange(controls::StateChange_Background);

    scene.frame();
    bool rendered=scene.log.wasRendered(leaf);
    scene.frame();
    rendered=rendered || scene.log.wasRendered(leaf);
    CHECK(rendered);

    scene.settle();
    checkSettled(scene.frame());
}

TEST_CASE("a canvas scale change lays out and renders every control once, then settles")
{
    Scene scene;
    scene.settle();

    scene.canvas->setScale(2.0f);
    CHECK(scene.renderer.scale() == 2.0f);

    const controls::ChangeStats &stats=scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    CHECK(stats.visited == Scene::Controls+1);
    CHECK(scene.log.entered.size() == Scene::Controls);
    CHECK(scene.log.rendered.size() == Scene::Controls);
    // one Geometry change each (the canvas's own render() does not count itself)
    CHECK(stats.geometry == Scene::Controls);
    CHECK(stats.created == 0);

    checkSettled(scene.frame());

    // the same scale again is no change
    scene.canvas->setScale(2.0f);
    checkSettled(scene.frame());

    scene.canvas->setScale(1.0f);
    scene.frame();
    CHECK(scene.log.rendered.size() == Scene::Controls);
    checkSettled(scene.frame());
}
