/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The render phase renders the controls in the pending-render queue and no
 *  others: addStateChange() queues a control once, and the canvas's drain
 *  (Base::drainRenderQueue) renders the queued controls on it in z order,
 *  which is the tree's preorder, and clears them. ChangeStats::visited counts
 *  the controls rendered, so a settled frame is 0, and renderCarried the
 *  entries left queued for a later frame.
 *
 *  The tree is 51 logging controls on a real Canvas. The drain renders all of
 *  them on the first frame, none on a settled one, only a changed leaf (none
 *  of its ancestors), a whole addStateChangeRecursive subtree, a newly added
 *  or re-attached control, a change raised from a render() on another control
 *  (that frame, after the sorted ones), and on a Canvas::setScale every
 *  control, laid out in one pass and rendered once, then settled. Whatever it
 *  renders, it renders in preorder, after bringToFront(), sendToBack() and
 *  moves between branches too.
 *
 *  It rebuilds each control's render offset and clip region from the root
 *  down: render() and renderUnder() at the parent's, renderOver() and
 *  renderFocus() with the control's own offset added in the parent's region,
 *  a non-clipping control leaving its children its parent's region and a
 *  getChildClipRect() narrowing one child's, as the recursive walk it
 *  replaced gave them (a reference walk here computes those), from a root
 *  offset and region read from the renderer: a canvas that sets (-x, -y) and
 *  its render bounds, as WindowCanvas does, moves every control by (-x, -y).
 *  The renderer is left at that root offset and region, as the walk left it.
 *
 *  A control deleted while queued is never touched (its slot is nulled), a
 *  queued control off the canvas or on a second canvas stays queued until it
 *  is on a canvas that renders, and the queue gives every entry back: the
 *  queue is the process's, so the cases compare queuedRenderCount() with a
 *  baseline.
 *
 *  One changed child of a container of 1 000 costs the render phase as many
 *  heap allocations and renders as one of a container of 10: the drain
 *  scans no siblings. The allocations are counted by this file's replacement
 *  of the global operator new, which is the whole gweni_unit_tests process's:
 *  malloc and free underneath, counted only while a case sets the flag. The
 *  aligned forms are left to the runtime (they pair among themselves), so an
 *  over-aligned allocation is not counted; gweni makes none.
 *
 *  The render phase only draws, and a tree change made from it is reported: a
 *  render() that calls sendToBack() on itself, adds a child to another
 *  control, or deletes or detaches a queued one prints one stderr line naming
 *  both controls and is recorded in renderWalkMutations() once per
 *  (operation, type) pair, until the record is reset, and the drain survives
 *  it (no deleted control rendered; the ASAN tree checks the memory). A
 *  render() that queues another control, whose render() queues it back, ends
 *  the frame anyway: the drain renders what is queued during it up to a cap.
 *  The same changes made between frames, a delayed delete processed in the
 *  update phase, and the line children a wrapped Text builds in
 *  renderPhaseLayout() (layout, before the drain) record nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/properties.h>
#include <gweni/controls/property/text.h>
#include <gweni/controls/text.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
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

//! The render offset and clip region a hook ran at.
struct RenderState
{
    Point offset;
    Rect clip;

    bool operator==(const RenderState &other) const
    {
        return offset.x == other.offset.x && offset.y == other.offset.y && clip == other.clip;
    }
};

std::ostream &operator<<(std::ostream &out, const RenderState &state)
{
    return out<<"offset ("<<state.offset.x<<", "<<state.offset.y<<") clip ("<<state.clip.x<<", "
        <<state.clip.y<<", "<<state.clip.w<<", "<<state.clip.h<<")";
}

//! What the render phase did in one frame: the controls whose hooks it ran
//! (renderUnder() runs once per control rendered), in order, and the ones
//! render() found a state change on, with the state each hook ran at.
struct WalkLog
{
    std::vector<controls::Base *> entered;
    std::vector<controls::Base *> rendered;
    std::map<controls::Base *, RenderState> under, render, over, focus;

    void clear()
    {
        entered.clear();
        rendered.clear();
        under.clear();
        render.clear();
        over.clear();
        focus.clear();
    }

    bool wasRendered(controls::Base *control) const
    {
        return std::find(rendered.begin(), rendered.end(), control) != rendered.end();
    }
};

RenderState stateOf(skin::Base *skin)
{
    renderer::Base *renderer=skin->getRenderer();
    return {renderer->getRenderOffset(), renderer->clipRegion()};
}

//! A plain control that logs the render phase, and can run a hook from its
//! render(). It may be told not to clip, or to clip its first child to a rect
//! of its own (as a ScrollControl clips its inner panel to the viewport).
class Probe: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Probe, controls::Base)
    {}

    WalkLog *log=nullptr;
    std::function<void()> onRender;
    bool clips=true;
    bool clipsFirstChild=false;
    Rect firstChildClip;

    bool shouldClip() override { return clips; }

    const Rect *getChildClipRect(const Base *child) const override
    {
        if(clipsFirstChild && !m_children.empty() && (child == m_children.front()))
            return &firstChildClip;
        return nullptr;
    }

    void renderUnder(skin::Base *skin) override
    {
        if(log)
        {
            log->entered.push_back(this);
            log->under[this]=stateOf(skin);
        }
        ParentClass::renderUnder(skin);
    }

    void render(skin::Base *skin) override
    {
        if(log && getStateChange() != controls::StateChange_Nothing)
        {
            log->rendered.push_back(this);
            log->render[this]=stateOf(skin);
        }
        if(onRender)
            onRender();
        ParentClass::render(skin);
    }

    void renderOver(skin::Base *skin) override
    {
        if(log)
            log->over[this]=stateOf(skin);
        ParentClass::renderOver(skin);
    }

    void renderFocus(skin::Base *skin) override
    {
        if(log)
            log->focus[this]=stateOf(skin);
        ParentClass::renderFocus(skin);
    }
};

//! A canvas that renders from its own position, as WindowCanvas does: the
//! render offset (-x, -y) and the clip region its render bounds.
class OffsetCanvas: public controls::Canvas
{
public:
    OffsetCanvas(skin::Base *skin): Canvas(skin) {}

    void renderCanvas() override
    {
        renderer::Base *render=m_skin->getRenderer();
        render->begin();
        renderPhaseLayout();
        renumberZOrder();
        render->setClipRegion(getRenderBounds());
        render->setRenderOffset(Point(-getX(), -getY()));
        render->setScale(scale());
        {
            controls::RenderWalkScope walk;
            renderPendingControls();
        }
        render->end();
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

    explicit Scene(bool offsetCanvas=false)
    {
        canvas=offsetCanvas?new OffsetCanvas(&skin):new controls::Canvas(&skin);
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

    std::set<controls::Base *> entered() const
    {
        return std::set<controls::Base *>(log.entered.begin(), log.entered.end());
    }

    std::set<controls::Base *> rendered() const
    {
        return std::set<controls::Base *>(log.rendered.begin(), log.rendered.end());
    }

    //! The controls rendered this frame, in the tree's preorder.
    std::vector<controls::Base *> renderedInPreorder() const
    {
        std::vector<controls::Base *> order;
        const std::set<controls::Base *> done=entered();
        const auto walk=[&](const auto &self, controls::Base *control) -> void
            {
                if(done.count(control))
                    order.push_back(control);
                for(controls::Base *child : control->getActualChildren())
                    self(self, child);
            };
        walk(walk, canvas);
        return order;
    }
};

void checkSettled(const controls::ChangeStats &stats)
{
    CHECK(stats.visited == 0);
    CHECK(stats.controls == 0);
    CHECK(stats.created == 0);
    CHECK(stats.visibility == 0);
    CHECK(stats.geometry == 0);
    CHECK(stats.background == 0);
    CHECK(stats.text == 0);
}

//! The states the recursive render walk gave every control below \p control,
//! rendered at \p state, computed independently of the drain: render (and
//! renderUnder) at the state its parent handed it,
//! renderOver and renderFocus at its own offset in that region.
struct Reference
{
    std::map<controls::Base *, RenderState> render, over;
};

Rect intersect(const Rect &clip, const Point &origin, const Rect &bounds)
{
    const int x0=std::max(origin.x, clip.x);
    const int y0=std::max(origin.y, clip.y);
    const int x1=std::min(origin.x+bounds.w, clip.x+clip.w);
    const int y1=std::min(origin.y+bounds.h, clip.y+clip.h);
    return Rect(x0, y0, std::max(0, x1-x0), std::max(0, y1-y0));
}

void referenceWalk(controls::Base *control, RenderState state, Reference &out)
{
    out.render[control]=state;

    const Rect &bounds=control->getBounds();
    RenderState inner{Point(state.offset.x+bounds.x, state.offset.y+bounds.y), state.clip};

    out.over[control]={inner.offset, state.clip};

    if(control->shouldClip())
        inner.clip=intersect(state.clip, inner.offset, bounds);

    for(controls::Base *child : control->getActualChildren())
    {
        RenderState childState=inner;

        if(const Rect *childClip=control->getChildClipRect(child))
            childState.clip=intersect(inner.clip,
                Point(inner.offset.x+childClip->x, inner.offset.y+childClip->y), *childClip);
        referenceWalk(child, childState, out);
    }
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

TEST_CASE("the first frame renders every control, a settled one none")
{
    const size_t queued=controls::queuedRenderCount();
    Scene scene;

    const controls::ChangeStats &first=scene.frame();
    // the canvas too, which has a state change of its own
    CHECK(first.visited == Scene::Controls+1);
    CHECK(scene.log.entered.size() == Scene::Controls);
    CHECK(scene.renderedInPreorder() == scene.log.entered);
    // the canvas's own render() does not count itself
    CHECK(first.created == Scene::Controls);

    scene.settle();
    checkSettled(scene.frame());
    CHECK(scene.log.entered.empty());
    CHECK(controls::queuedRenderCount() == queued);
}

TEST_CASE("a change on a deep leaf renders the leaf alone")
{
    Scene scene;
    scene.settle();

    Probe *leaf=scene.leaves[17];
    leaf->addStateChange(controls::StateChange_Background);
    CHECK(leaf->isQueuedForRender());

    const controls::ChangeStats &stats=scene.frame();
    CHECK(stats.visited == 1);
    CHECK(stats.controls == 1);
    CHECK(stats.background == 1);
    CHECK(scene.entered() == std::set<controls::Base *>{leaf});
    CHECK(scene.log.wasRendered(leaf));
    CHECK_FALSE(leaf->isQueuedForRender());

    checkSettled(scene.frame());
}

TEST_CASE("addStateChangeRecursive renders the whole subtree, in preorder")
{
    Scene scene;
    scene.settle();

    Probe *child=scene.children[5];
    child->addStateChangeRecursive(controls::StateChange_Geometry);

    const controls::ChangeStats &stats=scene.frame();
    std::set<controls::Base *> expected{child};
    for(controls::Base *leaf : child->getChildren())
        expected.insert(leaf);

    CHECK(scene.entered() == expected);
    // queued children first, then the child; rendered parent first
    CHECK(scene.log.entered.front() == child);
    CHECK(scene.renderedInPreorder() == scene.log.entered);
    CHECK(stats.visited == expected.size());
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
    CHECK(scene.entered() == scene.rendered());
    CHECK(scene.renderedInPreorder() == scene.log.entered);

    scene.settle();
    checkSettled(scene.frame());
}

TEST_CASE("a change made while a subtree is detached is rendered once it is back")
{
    Scene scene;
    scene.settle();

    // the last child of the last branch, re-attached to the end of the same
    // branch: only its queue entry brings the drain to its leaf
    Probe *branch=scene.branches.back();
    Probe *child=scene.children.back();
    Probe *leaf=static_cast<Probe *>(child->getChildren().front());

    child->setParent(nullptr);
    scene.frame();
    leaf->addStateChange(controls::StateChange_Background);
    const controls::ChangeStats &detached=scene.frame();
    CHECK_FALSE(scene.log.wasRendered(leaf));
    // the leaf waits in the queue, the frame carries it
    CHECK(leaf->isQueuedForRender());
    CHECK(detached.renderCarried >= 1);

    // re-attaching lays the branch out again, which changes more than the
    // leaf; the drain still renders only what changed
    child->setParent(branch);
    scene.frame();
    CHECK(scene.log.wasRendered(leaf));
    CHECK_FALSE(leaf->isQueuedForRender());
    CHECK(scene.entered() == scene.rendered());

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
    CHECK(scene.renderedInPreorder() == scene.log.entered);
    // at its new place: its parent's origin
    const Point origin=target->localPosToCanvas(Point(0, 0));
    CHECK(scene.log.render[leaf].offset.x == origin.x);
    CHECK(scene.log.render[leaf].offset.y == origin.y);

    // and one with a pending change, moved before the frame that renders it
    Probe *other=scene.leaves[4];
    other->addStateChange(controls::StateChange_Background);
    other->setParent(scene.children[0]);
    scene.frame();
    CHECK(scene.log.wasRendered(other));

    scene.settle();
    checkSettled(scene.frame());
}

TEST_CASE("controls reordered between frames are rendered in their new preorder")
{
    Scene scene;
    scene.settle();

    scene.branches[2]->sendToBack();
    scene.children[1]->bringToFront();
    scene.leaves[30]->setParent(scene.children[2]);
    scene.leaves[8]->bringNextToControl(scene.leaves[6], false);
    for(Probe *probe : scene.leaves)
        probe->addStateChange(controls::StateChange_Background);
    for(Probe *probe : scene.branches)
        probe->addStateChange(controls::StateChange_Background);

    scene.frame();
    CHECK(scene.log.entered.size() >= scene.leaves.size()+scene.branches.size());
    CHECK(scene.renderedInPreorder() == scene.log.entered);

    scene.settle();
    checkSettled(scene.frame());
}

TEST_CASE("a change raised from a render() on another control is rendered that frame")
{
    Scene scene;
    scene.settle();

    // raised from the middle branch's render() on a leaf the drain has
    // already passed, and on one it has not reached yet
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

    const controls::ChangeStats &stats=scene.frame();
    CHECK(scene.log.wasRendered(passed));
    CHECK(scene.log.wasRendered(ahead));
    CHECK(stats.visited == 3);
    CHECK(stats.renderCarried == controls::queuedRenderCount());

    checkSettled(scene.frame());
}

TEST_CASE("a change a render() raises on its own control is cleared with it")
{
    Scene scene;
    scene.settle();

    Probe *leaf=scene.leaves[9];
    leaf->onRender=[&]() { leaf->addStateChange(controls::StateChange_Text); };
    leaf->addStateChange(controls::StateChange_Background);
    scene.frame();
    CHECK(scene.log.wasRendered(leaf));
    CHECK_FALSE(leaf->isQueuedForRender());
    CHECK(leaf->getStateChange() == controls::StateChange_Nothing);

    leaf->onRender=nullptr;
    checkSettled(scene.frame());
}

TEST_CASE("renders that keep queueing each other end the frame")
{
    Scene scene;
    scene.settle();

    // each render() queues the other again: the drain renders what is queued
    // during it up to a cap, and leaves the rest for the next frame
    Probe *ping=scene.leaves[3];
    Probe *pong=scene.leaves[34];
    ping->onRender=[&]() { pong->addStateChange(controls::StateChange_Background); };
    pong->onRender=[&]() { ping->addStateChange(controls::StateChange_Background); };
    ping->addStateChange(controls::StateChange_Background);

    const controls::ChangeStats &stats=scene.frame();
    CHECK(stats.visited > 2);
    CHECK(stats.visited < 100000);
    CHECK(stats.renderCarried >= 1);
    CHECK((ping->isQueuedForRender() || pong->isQueuedForRender()));

    ping->onRender=nullptr;
    pong->onRender=nullptr;
    scene.settle();
    checkSettled(scene.frame());
    CHECK_FALSE(ping->isQueuedForRender());
    CHECK_FALSE(pong->isQueuedForRender());
}

TEST_CASE("a control deleted while queued is never rendered")
{
    const size_t queued=controls::queuedRenderCount();
    Scene scene;
    scene.settle();

    Probe *leaf=scene.leaves[20];
    Probe *child=scene.children[3];
    leaf->addStateChange(controls::StateChange_Background);
    child->addStateChangeRecursive(controls::StateChange_Background);
    std::set<controls::Base *> gone{leaf, child};
    for(controls::Base *control : child->getChildren())
        gone.insert(control);

    scene.leaves.erase(std::remove_if(scene.leaves.begin(), scene.leaves.end(),
        [&](Probe *probe) { return gone.count(probe) != 0; }), scene.leaves.end());
    delete leaf;
    delete child;

    // compared as values only: what the log holds was never dereferenced
    scene.frame();
    for(controls::Base *control : scene.log.entered)
        CHECK(gone.count(control) == 0);

    scene.settle();
    checkSettled(scene.frame());
    delete scene.canvas;
    scene.canvas=nullptr;
    CHECK(controls::queuedRenderCount() == queued);
}

TEST_CASE("a control queued on a second canvas is left to that canvas")
{
    Scene first;
    Scene second;
    first.settle();
    second.settle();

    Probe *theirs=second.leaves[11];
    Probe *ours=first.leaves[11];
    theirs->addStateChange(controls::StateChange_Background);
    ours->addStateChange(controls::StateChange_Background);

    const controls::ChangeStats &stats=first.frame();
    CHECK(first.log.wasRendered(ours));
    CHECK_FALSE(second.log.wasRendered(theirs));
    CHECK(theirs->isQueuedForRender());
    CHECK(stats.visited == 1);
    CHECK(stats.renderCarried >= 1);

    second.frame();
    CHECK(second.log.wasRendered(theirs));
    CHECK_FALSE(theirs->isQueuedForRender());

    checkSettled(first.frame());
    checkSettled(second.frame());
}

namespace
{

//! A scene with a non-clipping branch, a branch clipping its first child to
//! a rect of its own, and bounds that cut the children off, so the offsets
//! and regions differ from control to control.
void shapeForClipping(Scene &scene)
{
    scene.branches[0]->clips=false;
    scene.branches[1]->clipsFirstChild=true;
    scene.branches[1]->firstChildClip=Rect(3, 4, 12, 9);
    scene.children[2]->clips=false;
    scene.children[5]->clipsFirstChild=true;
    scene.children[5]->firstChildClip=Rect(-2, 1, 30, 6);
    scene.branches[2]->setBounds(380, 290, 50, 20);
    scene.children[7]->setBounds(-10, 15, 40, 8);
}

void checkAgainstReference(Scene &scene, const RenderState &root)
{
    Reference reference;
    referenceWalk(scene.canvas, root, reference);

    for(Probe *probe : scene.leaves)
        probe->addStateChange(controls::StateChange_Background);
    for(Probe *probe : scene.children)
        probe->addStateChange(controls::StateChange_Background);
    for(Probe *probe : scene.branches)
        probe->addStateChange(controls::StateChange_Background);
    scene.frame();
    // and the renderer is left as the canvas set it, for what it draws after
    CHECK(stateOf(&scene.skin) == root);

    const size_t probes=scene.leaves.size()+scene.children.size()+scene.branches.size();
    REQUIRE(scene.log.entered.size() == probes);

    for(controls::Base *control : scene.log.entered)
    {
        CAPTURE(control->getControlId());
        CHECK(scene.log.under[control] == reference.render[control]);
        CHECK(scene.log.render[control] == reference.render[control]);
        CHECK(scene.log.over[control] == reference.over[control]);
        CHECK(scene.log.focus[control] == reference.over[control]);
    }
}

}//namespace

TEST_CASE("every hook runs at the offset and clip region the recursive walk gave it")
{
    Scene scene;
    shapeForClipping(scene);
    scene.settle();

    // the same frame, whole and then a few scattered controls, each rebuilt
    // from the root or from the stack
    checkAgainstReference(scene, {Point(0, 0), scene.canvas->getBounds()});

    Reference reference;
    referenceWalk(scene.canvas, {Point(0, 0), scene.canvas->getBounds()}, reference);
    const std::vector<Probe *> few={scene.leaves[35], scene.leaves[2], scene.children[5],
        scene.leaves[16], scene.branches[2]};
    for(Probe *probe : few)
        probe->addStateChange(controls::StateChange_Background);
    scene.frame();
    REQUIRE(scene.log.entered.size() == few.size());
    for(Probe *probe : few)
    {
        CAPTURE(probe->getControlId());
        CHECK(scene.log.render[probe] == reference.render[probe]);
        CHECK(scene.log.over[probe] == reference.over[probe]);
        CHECK(scene.log.focus[probe] == reference.over[probe]);
    }
}

TEST_CASE("a canvas rendering from (-x, -y) and its render bounds moves every control by it")
{
    Scene scene(true);
    scene.canvas->setBounds(30, 20, 400, 300);
    shapeForClipping(scene);
    scene.settle();

    checkAgainstReference(scene, {Point(-30, -20), scene.canvas->getRenderBounds()});
}

TEST_CASE("a canvas scale change lays out and renders every control once, then settles")
{
    Scene scene;
    scene.settle();

    scene.canvas->setScale(2.0f);
    CHECK(scene.renderer.scale() == 2.0f);

    const controls::ChangeStats &stats=scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    CHECK(stats.visited >= Scene::Controls);
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

    // logs the changed child only, into a log that has room already
    WalkLog log;
    log.entered.reserve(1);
    log.rendered.reserve(1);
    Probe *changed=children[count/2];
    changed->log=&log;
    changed->addStateChange(controls::StateChange_Background);

    controls::resetChangeStats();
    canvas->updateCanvas();
    // the log's maps get their node before the counted frame
    log.under[changed];
    log.render[changed];
    log.over[changed];
    log.focus[changed];

    g_allocations=0;
    g_countAllocations=true;
    canvas->renderCanvas();
    g_countAllocations=false;

    RenderCost cost;
    cost.allocations=g_allocations;
    cost.visited=controls::getChangeStats().visited;
    cost.rendered=log.wasRendered(changed);

    delete canvas;
    return cost;
}

}//namespace

TEST_CASE("the render phase scans no siblings of a changed control and allocates nothing for them")
{
    // the drain's vectors, kept from frame to frame, grow once
    renderOneChangedChild(10);

    const RenderCost small=renderOneChangedChild(10);
    const RenderCost large=renderOneChangedChild(1000);

    CHECK(small.rendered);
    CHECK(large.rendered);
    // the changed child alone
    CHECK(small.visited == 1);
    CHECK(large.visited == 1);

    INFO("render-phase allocations: ", small.allocations, " with 10 children, ",
        large.allocations, " with 1000");
    CHECK(large.allocations == small.allocations);
}

TEST_CASE("a PropertyRow's label takes the row's look when its property is focused or hovered")
{
    // The row's look follows its property's focus and hover, which change
    // controls below it: the row is told in the update phase
    // (onDescendantInteraction), since the render phase calls no hook of a
    // row that did not change itself.
    Scene scene;
    auto *properties=scene.canvas->newChild<controls::Properties>();
    properties->setBounds(0, 0, 200, 100);
    controls::PropertyRow *row=properties->add("Name", "value");
    scene.settle();

    auto *text=gweni_cast<controls::property::Text>(row->getProperty());
    REQUIRE(text);
    controls::Base *label=row->getLabel();
    REQUIRE_FALSE(label->isQueuedForRender());

    text->m_textBox->focus();
    CHECK(row->isEditing());
    CHECK(label->isQueuedForRender());
    scene.frame();
    CHECK(scene.renderer.generatedBy.count(label) == 1);
    CHECK_FALSE(label->isQueuedForRender());

    text->m_textBox->unfocus();
    CHECK(label->isQueuedForRender());
    scene.settle();

    controls::Base *hoveredBefore=HoveredControl;
    HoveredControl=text->m_textBox;
    text->m_textBox->onMouseEnter();
    CHECK(row->isHovered());
    CHECK(label->isQueuedForRender());
    scene.settle();
    HoveredControl=nullptr;
    text->m_textBox->onMouseLeave();
    CHECK(label->isQueuedForRender());
    HoveredControl=hoveredBefore;
    scene.settle();
    checkSettled(scene.frame());
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

TEST_CASE("a render() that deletes or detaches queued controls is reported and survived")
{
    controls::resetRenderWalkMutations();
    const size_t queued=controls::queuedRenderCount();
    Scene scene;

    Probe *branch=scene.branches[0];
    Builder *builder=branch->newChild<Builder>();
    builder->setBounds(0, 0, 10, 10);
    scene.settle();

    // queued ahead of the builder (a later branch), and its own parent's
    // subtree around it: deleted and detached from inside its render(), while
    // the drain holds their slots and its ancestor stack holds the branch
    Probe *doomed=scene.children[6];
    Probe *detached=scene.children[10];
    Probe *detachedLeaf=static_cast<Probe *>(detached->getChildren().back());
    std::set<controls::Base *> gone{doomed};
    for(controls::Base *control : doomed->getChildren())
        gone.insert(control);
    doomed->addStateChangeRecursive(controls::StateChange_Background);
    detachedLeaf->addStateChange(controls::StateChange_Background);
    scene.leaves.back()->addStateChange(controls::StateChange_Background);

    bool once=true;
    builder->onRender=[&]()
        {
            if(!once)
                return;
            once=false;
            delete doomed;
            detached->setParent(nullptr);
        };

    CerrCapture cerr;
    builder->addStateChange(controls::StateChange_Background);
    scene.frame();

    // doomed's children leave it once it is a Base, being deleted
    const std::vector<std::string> expected={"Base removeChild (rendering Builder)",
        "Base ~Base (rendering Builder)", "Probe removeChild (rendering Builder)"};
    CHECK(controls::renderWalkMutations() == expected);
    for(controls::Base *control : scene.log.entered)
        CHECK(gone.count(control) == 0);
    // off the canvas now: it waits
    CHECK_FALSE(scene.log.wasRendered(detachedLeaf));
    CHECK(detachedLeaf->isQueuedForRender());
    // what comes after them is still rendered, at its place
    CHECK(scene.log.wasRendered(scene.leaves.back()));
    Reference reference;
    referenceWalk(scene.canvas, {Point(0, 0), scene.canvas->getBounds()}, reference);
    CHECK(scene.log.render[scene.leaves.back()] == reference.render[scene.leaves.back()]);

    builder->onRender=nullptr;
    detached->setParent(scene.branches[2]);
    scene.frame();
    CHECK(scene.log.wasRendered(detachedLeaf));
    scene.settle();
    checkSettled(scene.frame());

    // the deleted controls are not in the scene's lists any more
    delete scene.canvas;
    scene.canvas=nullptr;
    CHECK(controls::queuedRenderCount() == queued);
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
