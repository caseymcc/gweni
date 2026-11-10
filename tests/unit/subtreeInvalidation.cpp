/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Base::invalidateChildren(true), the walk that invalidates a whole subtree
 *  (a resize, a scale change, a first layout), on a real Canvas with the
 *  StubSkin. The tree nests inner panels 6 deep (ScrollControls and
 *  GroupBoxes in turn, the innermost holding a Label and a Button) beside a
 *  TreeControl whose TreeNodes nest 7 deep (open, a leaf at each level, and a
 *  closed node holding a hidden subtree). Each walk starts from a settled
 *  canvas and is rooted at the canvas, at a deep inner-panel owner, at a deep
 *  TreeNode, and at an inner panel itself (a ScrollControl's and a
 *  TreeNode's), and:
 *
 *  - each control's needsLayout() / layoutDirty() after it is what main
 *    (cb8b545) gives, but for the way down to each TreeNode's toggle, which
 *    main's settled canvas still had layoutDirty() (#522), recorded below as
 *    one character per control of the canvas's tree in pre-order over
 *    getActualChildren(): the walk dirties its subtree and the way up from
 *    the root, and nothing else;
 *  - it enters each control of the subtree exactly once, and its upward
 *    work is done once, for the walk's root, not by every entry:
 *    invalidateCalls() goes up by the subtree's size plus the controls on
 *    the way up from the root's children's logical parent (the root, or for
 *    an inner panel its owner) to the canvas, no more than the root's depth
 *    and one. Main walked an inner panel's subtree twice, once from the
 *    panel and once from its owner, which compounded per level, and every
 *    entry walked up to the canvas (Base::invalidate()'s invalidateParent()):
 *    528 600 calls for the canvas's 304 controls, where it is now 305 (#519).
 *
 *  Canvas::setScale's walk (onScaleChanged over the whole tree) likewise
 *  makes one call per control, the canvas's own included.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/groupBox.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/treeNode.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

const int NestDepth=6;
const int NodeDepth=7;

void forEachActual(controls::Base *control, const std::function<void(controls::Base *)> &func)
{
    func(control);
    for(controls::Base *child : control->getActualChildren())
        forEachActual(child, func);
}

//! One character per control of `root`'s tree, in pre-order: 'B' needs
//! layout and is dirty, 'n' needs layout only, 'd' is dirty only, '.' neither.
std::string layoutState(controls::Base *root)
{
    std::string state;
    forEachActual(root, [&](controls::Base *control)
        {
            const bool needs=control->needsLayout();
            const bool dirty=control->layoutDirty();

            state+=needs?(dirty?'B':'n'):(dirty?'d':'.');
        });
    return state;
}

//! Each control of `root`'s tree whose layoutState() character is not the
//! one in `expected`, as "<index> <Type>: <expected> -> <actual>" lines;
//! empty when all match.
std::string differences(controls::Base *root, const std::string &expected)
{
    std::string out;
    size_t index=0;

    forEachActual(root, [&](controls::Base *control)
        {
            const std::string actual=layoutState(control).substr(0, 1);
            const std::string wanted=(index < expected.size())?expected.substr(index, 1):"?";

            if(actual != wanted)
                out+=std::to_string(index)+" "+control->getTypeName()+": "+wanted+" -> "+actual+"\n";
            ++index;
        });
    if(index != expected.size())
        out+="controls: "+std::to_string(expected.size())+" -> "+std::to_string(index)+"\n";
    return out;
}

size_t subtreeSize(controls::Base *root)
{
    size_t count=0;
    forEachActual(root, [&](controls::Base *) { ++count; });
    return count-1;
}

//! The controls a walk from `root` invalidates above its subtree, once each:
//! the logical parents of its children (the root itself, or an inner panel's
//! owner) and their getParent() chains up to the canvas.
size_t upOnce(controls::Base *root)
{
    std::set<controls::Base *> up;
    for(controls::Base *child : root->getActualChildren())
    {
        for(controls::Base *parent=child->getParent(); parent; parent=parent->getParent())
            up.insert(parent);
    }
    return up.size();
}

//! The actual ancestors of `control`, up to the canvas.
size_t depthOf(controls::Base *control)
{
    size_t depth=0;
    for(const controls::Base *parent=control->getActualParent(); parent; parent=parent->getActualParent())
        ++depth;
    return depth;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    //! The inner-panel owners, outermost first (ScrollControl, GroupBox, ...).
    std::vector<controls::Base *> nest;
    //! The open TreeNodes, outermost first.
    std::vector<controls::TreeNode *> nodes;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);

        controls::Base *parent=canvas;
        for(int level=0; level<NestDepth; ++level)
        {
            controls::Base *owner;

            if(level%2 == 0)
                owner=parent->newChild<controls::ScrollControl>();
            else
            {
                controls::GroupBox *group=parent->newChild<controls::GroupBox>();

                group->setText("group");
                owner=group;
            }
            owner->setDock(DockPosition::Top);
            owner->setHeight(400-level*50);
            parent->newChild<controls::Label>()->setText("beside");
            nest.push_back(owner);
            parent=owner;
        }
        parent->newChild<controls::Label>()->setText("innermost");
        parent->newChild<controls::Button>()->setText("button");

        controls::TreeControl *tree=canvas->newChild<controls::TreeControl>();
        tree->setDock(DockPosition::Center);

        controls::TreeNode *node=tree->addNode("root");
        for(int level=0; level<NodeDepth; ++level)
        {
            nodes.push_back(node);
            node->addNode("leaf");
            controls::TreeNode *closed=node->addNode("closed");
            closed->addNode("hidden")->addNode("hidden below");
            node=node->addNode("level");
        }
        node->addNode("last leaf");
        for(controls::TreeNode *open : nodes)
            open->open();

        settle();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void settle()
    {
        for(int frame=0; frame<3; ++frame)
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
    }
};

bool holds(controls::Base *holder, controls::Base *control)
{
    for(controls::Base *child : holder->getActualChildren())
    {
        if(child == control)
            return true;
    }
    return false;
}

controls::Base *innerPanelOf(controls::Base *owner)
{
    return const_cast<controls::Base *>(owner->getInnerPanel());
}

//! Each walk's root, by name.
struct Walk
{
    const char *name;
    std::function<controls::Base *(Scene &)> root;
};

const Walk Walks[]=
{
    {"canvas", [](Scene &s) { return static_cast<controls::Base *>(s.canvas); }},
    {"deep inner-panel owner", [](Scene &s) { return s.nest[NestDepth-2]; }},
    {"deep TreeNode", [](Scene &s) { return static_cast<controls::Base *>(s.nodes[4]); }},
    {"a ScrollControl's inner panel", [](Scene &s) { return innerPanelOf(s.nest[NestDepth-2]); }},
    {"a TreeNode's inner panel", [](Scene &s) { return innerPanelOf(s.nodes[3]); }},
};

//! The settled canvas's state (hidden subtrees, which no pass enters, stay
//! dirty, and needsLayout() stays set on controls whose layout() did not run),
//! and after each walk, in Walks' order: main's (cb8b545), but with nothing
//! on the way down to each TreeNode's toggle layoutDirty(), which main left
//! dirty after the pass that placed it in postLayout() (#522).
const char *const SettledState=
    "nn.n..n.n..n.n..n.n.n.BBBBBBBBBBBBn.n.BBBBBBBBBBBBn.n.BBBBBBBBBB"
    "BBn.nn.nn.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n..nBBn.Bnn.n.BBBBBBBBBB"
    "BBBnn.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n..nBBn.Bnn.n.BBBBBBBBBBBBBn"
    "n.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n."
    ".nBBn.Bnn.n.BBBBBBBBBBBBBnn.n.BBBBBBBnn.n..BBBBBB";
const char *const WalkState[]=
{
    // the canvas
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
    // the deep inner-panel owner
    "BBdB.dBdB.dBBBBBBBBBBBBBBBBBBBBBBBn.n.BBBBBBBBBBBBn.n.BBBBBBBBBB"
    "BBn.nn.nn.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n..nBBn.Bnn.n.BBBBBBBBBB"
    "BBBnn.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n..nBBn.Bnn.n.BBBBBBBBBBBBBn"
    "n.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n."
    ".nBBn.Bnn.n.BBBBBBBBBBBBBnn.n.BBBBBBBnn.n..BBBBBB",
    // the deep TreeNode
    "Bn.n..n.n..n.n..n.n.n.BBBBBBBBBBBBn.n.BBBBBBBBBBBBn.n.BBBBBBBBBB"
    "BBn.BBdBn.n.dnBBn.Bnn.n.BBBBBBBBBBBBBBn.n.dnBBn.Bnn.n.BBBBBBBBBB"
    "BBBBn.n.dnBBn.Bnn.n.BBBBBBBBBBBBBBn.n.dnBBn.Bnn.n.BBBBBBBBBBBBBB"
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBnn.n..BBBBBB",
    // the ScrollControl's inner panel
    "BBdB.dBdB.dBdBBBBBBBBBBBBBBBBBBBBBn.n.BBBBBBBBBBBBn.n.BBBBBBBBBB"
    "BBn.nn.nn.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n..nBBn.Bnn.n.BBBBBBBBBB"
    "BBBnn.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n..nBBn.Bnn.n.BBBBBBBBBBBBBn"
    "n.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n..nBBn.Bnn.n.BBBBBBBBBBBBBnn.n."
    ".nBBn.Bnn.n.BBBBBBBBBBBBBnn.n.BBBBBBBnn.n..BBBBBB",
    // the TreeNode's inner panel
    "Bn.n..n.n..n.n..n.n.n.BBBBBBBBBBBBn.n.BBBBBBBBBBBBn.n.BBBBBBBBBB"
    "BBn.BBdBn.n.dnBBn.Bnn.n.BBBBBBBBBBBBBBn.n.dnBBn.Bnn.n.BBBBBBBBBB"
    "BBBBn.n.dnBBn.Bnn.n.BBBBBBBBBBBBBBn.n.dBBBBBBBBBBBBBBBBBBBBBBBBB"
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBnn.n..BBBBBB",
};

}//namespace

TEST_CASE("Subtree invalidation: the scene nests inner panels and TreeNodes 6 levels and more")
{
    Scene scene;

    // each level is held by the one above's inner panel
    for(size_t level=1; level<scene.nest.size(); ++level)
        CHECK(holds(innerPanelOf(scene.nest[level-1]), scene.nest[level]));
    for(size_t level=1; level<scene.nodes.size(); ++level)
        CHECK(holds(innerPanelOf(scene.nodes[level-1]), scene.nodes[level]));
    CHECK(scene.nest.size() >= 6);
    CHECK(scene.nodes.size() >= 6);
    CHECK(differences(scene.canvas, SettledState) == "");
}

TEST_CASE("Subtree invalidation: the walk leaves each control's layout state as main does")
{
    for(size_t w=0; w<sizeof(Walks)/sizeof(Walks[0]); ++w)
    {
        Scene scene;
        controls::Base *root=Walks[w].root(scene);

        CAPTURE(std::string(Walks[w].name));
        REQUIRE(differences(scene.canvas, SettledState) == "");
        root->invalidateChildren(true);
        CHECK(differences(scene.canvas, WalkState[w]) == "");
        // and, as a rule: everything under the root needs layout and is dirty
        for(controls::Base *child : root->getActualChildren())
            CHECK(layoutState(child) == std::string(layoutState(child).size(), 'B'));
    }
}

TEST_CASE("Subtree invalidation: one walk enters each control of the subtree once and walks up once")
{
    for(const Walk &walk : Walks)
    {
        Scene scene;
        controls::Base *root=walk.root(scene);
        const size_t calls=controls::invalidateCalls();

        CAPTURE(std::string(walk.name));
        CAPTURE(subtreeSize(root));
        root->invalidateChildren(true);
        CHECK(controls::invalidateCalls()-calls == subtreeSize(root)+upOnce(root));
        CHECK(upOnce(root) <= depthOf(root)+1);
    }
}

TEST_CASE("Subtree invalidation: a new canvas scale invalidates each control once")
{
    Scene scene;
    const size_t calls=controls::invalidateCalls();

    scene.canvas->setScale(2.0f);
    CHECK(controls::invalidateCalls()-calls == subtreeSize(scene.canvas)+1);
    // and the tree is laid out again from it, as from any scale change
    CHECK(differences(scene.canvas, WalkState[0]) == "");
}
