/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The render walk enters only the controls with a pending state change and
 *  their ancestors: addStateChange() marks the path to the root, attaching a
 *  pending control marks its new parent's, and doRender() skips a subtree with
 *  nothing pending in it. ChangeStats::visited counts the controls entered.
 *
 *  The tree is 51 logging controls on a real Canvas. The walk enters all of
 *  them on the first frame, none on a settled one, only a changed leaf and its
 *  ancestors, a whole addStateChangeRecursive subtree, a newly added or
 *  re-attached control, a change raised from a render() during the walk
 *  (rendered that frame or the next), and on a Canvas::setScale every control,
 *  laid out in one pass and rendered once, then settled.
 *
 *  The walk allocates nothing per child: one changed child of a container of
 *  1 000 costs the render phase as many heap allocations as one of a container
 *  of 10 (it once copied each entered control's child list, one node per
 *  sibling), while ChangeStats::childrenScanned counts the siblings its loop
 *  still passes over. The allocations are counted by this file's replacement
 *  of the global operator new, which is the whole gweni_unit_tests process's:
 *  malloc and free underneath, counted only while a case sets the flag. The
 *  aligned forms are left to the runtime (they pair among themselves), so an
 *  over-aligned allocation is not counted; gweni makes none.
 *
 *  The walk only draws, and a tree change made from it is reported: a render()
 *  that calls sendToBack() on itself, or adds a child to another control,
 *  prints one stderr line naming both controls and is recorded in
 *  renderWalkMutations() once per (operation, type) pair, until the record is
 *  reset. The same changes made between frames, a delayed delete processed in
 *  the update phase, and the line children a wrapped Text builds in
 *  renderPhaseLayout() (layout, before the walk) record nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/text.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <new>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! The replacement operator new counts while this is set. Only the main
//! thread sets it, around the part of a case it measures.
bool g_countAllocations=false;
size_t g_allocations=0;

void *allocate(std::size_t size)
{
    if(g_countAllocations)
        ++g_allocations;
    return std::malloc(size ? size : 1);
}

// Out of line, so GCC never sees a free() of what its inlined operator new
// returned (-Wmismatched-new-delete cannot tell that new is malloc here).
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void deallocate(void *memory) noexcept
{
    std::free(memory);
}

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

void *operator new(std::size_t size)
{
    if(void *memory=allocate(size))
        return memory;
    throw std::bad_alloc();
}

void *operator new[](std::size_t size)
{
    if(void *memory=allocate(size))
        return memory;
    throw std::bad_alloc();
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    return allocate(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    return allocate(size);
}

void operator delete(void *memory) noexcept { deallocate(memory); }
void operator delete[](void *memory) noexcept { deallocate(memory); }
void operator delete(void *memory, std::size_t) noexcept { deallocate(memory); }
void operator delete[](void *memory, std::size_t) noexcept { deallocate(memory); }
void operator delete(void *memory, const std::nothrow_t &) noexcept { deallocate(memory); }
void operator delete[](void *memory, const std::nothrow_t &) noexcept { deallocate(memory); }

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

namespace
{

//! What one frame's render phase cost, after a change on one of \p count
//! children of a container on a canvas.
struct RenderCost
{
    size_t allocations=0;
    size_t visited=0;
    size_t childrenScanned=0;
    size_t expectedScanned=0;   //!< the canvas's children and the container's
    bool rendered=false;        //!< the changed child was rendered
};

RenderCost renderOneChangedChild(size_t count)
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);
    canvas->setSize(400, 300);

    Probe *container=canvas->newChild<Probe>();
    container->setBounds(0, 0, 200, 200);
    std::vector<Probe *> children;
    for(size_t i=0; i<count; ++i)
    {
        Probe *child=container->newChild<Probe>();
        child->setBounds(0, int(i%20), 10, 10);
        children.push_back(child);
    }

    for(int i=0; i<10; ++i)
    {
        controls::resetChangeStats();
        canvas->updateCanvas();
        canvas->renderCanvas();
        if(controls::getChangeStats().controls == 0)
            break;
    }

    // logs the changed child only, into vectors that have room already
    WalkLog log;
    log.entered.reserve(1);
    log.rendered.reserve(1);
    Probe *changed=children[count/2];
    changed->log=&log;
    changed->addStateChange(controls::StateChange_Background);

    controls::resetChangeStats();
    canvas->updateCanvas();

    g_allocations=0;
    g_countAllocations=true;
    canvas->renderCanvas();
    g_countAllocations=false;

    RenderCost cost;
    cost.allocations=g_allocations;
    cost.visited=controls::getChangeStats().visited;
    cost.childrenScanned=controls::getChangeStats().childrenScanned;
    cost.expectedScanned=canvas->getChildren().size()+count;
    cost.rendered=log.wasRendered(changed);

    delete canvas;
    return cost;
}

}//namespace

TEST_CASE("the render walk allocates nothing per sibling of a changed control")
{
    const RenderCost small=renderOneChangedChild(10);
    const RenderCost large=renderOneChangedChild(1000);

    CHECK(small.rendered);
    CHECK(large.rendered);
    // the canvas, the container and the changed child
    CHECK(small.visited == 3);
    CHECK(large.visited == 3);

    // the loop still passes over every child of each control it enters
    CHECK(small.childrenScanned == small.expectedScanned);
    CHECK(large.childrenScanned == large.expectedScanned);

    // but allocates nothing for them
    INFO("render-phase allocations: ", small.allocations, " with 10 children, ",
        large.allocations, " with 1000");
    CHECK(large.allocations == small.allocations);
}

namespace
{

//! A Probe of its own type, to tell the controls apart in a report.
class Mover: public Probe
{
public:
    GWENI_CONTROL_INLINE(Mover, Probe)
    {}
};

class Builder: public Probe
{
public:
    GWENI_CONTROL_INLINE(Builder, Probe)
    {}
};

//! Captures std::cerr while it lives.
struct CerrCapture
{
    std::ostringstream text;
    std::streambuf *saved;

    CerrCapture(): saved(std::cerr.rdbuf(text.rdbuf())) {}
    ~CerrCapture() { std::cerr.rdbuf(saved); }

    size_t count(const std::string &needle) const
    {
        const std::string s=text.str();
        size_t found=0;
        for(size_t at=s.find(needle); at != std::string::npos; at=s.find(needle, at+1))
            ++found;
        return found;
    }
};

std::string idOf(controls::Base *control)
{
    return std::to_string(control->getControlId());
}

}//namespace

TEST_CASE("a render() that calls sendToBack() is reported once per operation and type")
{
    controls::resetRenderWalkMutations();
    Scene scene;

    // A Mover at the front of its branch, moved there between frames, which
    // is the update phase's to do and records nothing. Its sendToBack() from
    // render() then leaves the list as it is (it is at the front already), so
    // the walk stays well defined while the call is still reported.
    Probe *branch=scene.branches[1];
    Mover *mover=branch->newChild<Mover>();
    mover->setBounds(0, 0, 10, 10);
    mover->sendToBack();
    REQUIRE(branch->getChildren().front() == mover);
    scene.settle();
    CHECK(controls::renderWalkMutations().empty());

    mover->onRender=[&]() { mover->sendToBack(); };

    CerrCapture cerr;
    mover->addStateChange(controls::StateChange_Background);
    scene.frame();

    const std::vector<std::string> expected={"Mover sendToBack (rendering Mover)"};
    CHECK(controls::renderWalkMutations() == expected);
    const std::string line="gweni: Mover#"+idOf(mover)+" sendToBack during the render walk"
        " (rendering Mover#"+idOf(mover)+")\n";
    CHECK(cerr.count(line) == 1);
    CHECK(cerr.count("during the render walk") == 1);

    // the same pair again is neither printed nor recorded twice
    mover->addStateChange(controls::StateChange_Background);
    scene.frame();
    CHECK(controls::renderWalkMutations() == expected);
    CHECK(cerr.count("during the render walk") == 1);

    // a reset forgets it: the next one is reported again
    controls::resetRenderWalkMutations();
    CHECK(controls::renderWalkMutations().empty());
    mover->addStateChange(controls::StateChange_Background);
    scene.frame();
    CHECK(controls::renderWalkMutations() == expected);
    CHECK(cerr.count("during the render walk") == 2);

    mover->onRender=nullptr;
    controls::resetRenderWalkMutations();
}

TEST_CASE("a render() that adds a child to another control is reported")
{
    controls::resetRenderWalkMutations();
    Scene scene;

    Probe *branch=scene.branches[0];
    Builder *builder=branch->newChild<Builder>();
    builder->setBounds(0, 0, 10, 10);
    scene.settle();

    // a leaf of another branch, which the walk has not reached yet; appending
    // to a std::list leaves the walk's iterators valid
    Probe *target=scene.leaves.back();
    bool once=true;
    builder->onRender=[&]()
        {
            if(!once)
                return;
            once=false;
            target->newChild<Probe>();
        };

    CerrCapture cerr;
    builder->addStateChange(controls::StateChange_Background);
    scene.frame();

    const std::vector<std::string> expected={"Probe addChild (rendering Builder)"};
    CHECK(controls::renderWalkMutations() == expected);
    const std::string line="gweni: Probe#"+idOf(target)+" addChild during the render walk"
        " (rendering Builder#"+idOf(builder)+")\n";
    CHECK(cerr.count(line) == 1);

    builder->onRender=nullptr;
    scene.settle();
    controls::resetRenderWalkMutations();
}

TEST_CASE("tree changes in the update phase and in renderPhaseLayout() are not reported")
{
    controls::resetRenderWalkMutations();
    CerrCapture cerr;
    Scene scene;
    scene.settle();

    // between frames: every operation the guard watches
    Probe *leaf=scene.leaves[3];
    leaf->setParent(scene.children[7]);
    leaf->bringToFront();
    leaf->sendToBack();
    leaf->bringNextToControl(scene.children[7]->getChildren().back(), true);
    scene.frame();
    delete scene.leaves[8];
    scene.frame();

    // a delayed delete, carried out inside updateCanvas()
    scene.leaves[11]->delayedDelete();
    scene.frame();

    // a wrapped Text made after the update phase: renderCanvas()'s
    // renderPhaseLayout() lays it out and builds its line children, before
    // the walk
    controls::resetChangeStats();
    scene.canvas->updateCanvas();
    controls::Base *box=scene.canvas->newChild<controls::Base>();
    box->setBounds(0, 0, 40, 200);
    auto *wrapped=box->newChild<controls::internal::Text>();
    wrapped->setWrap(true);
    wrapped->setText("aa bbbbbbbbbb ccc dddd eeeee ff");
    scene.canvas->renderCanvas();
    CHECK(scene.canvas->getLayoutStats().renderLayout);
    CHECK(wrapped->numLines() > 1);

    scene.settle();
    CHECK(controls::renderWalkMutations().empty());
    CHECK(cerr.count("during the render walk") == 0);
}
