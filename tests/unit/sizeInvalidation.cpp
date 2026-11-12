/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  What a new size invalidates (#578), on a real Canvas with the StubSkin. A
 *  control given a new size invalidates the children it holds (its inner
 *  panel included), not its subtree: below them, each control whose own
 *  bounds the arrange changes invalidates its children in turn. A canvas
 *  resized so invalidates only its own children. In a layout pass neither
 *  that nor the resized control walks up to the canvas, and placement code
 *  invalidating a control (a ScrollControl's bars given their new ranges)
 *  walks up only to the first ancestor already dirty. The trees nest plain
 *  panels and vertical ScrollControls docked Center, a Label docked Top in
 *  each, the innermost holding Labels docked Top and a TreeControl of open
 *  TreeNodes, so ScrollControl and TreeNode inner panels are resized too.
 *
 *  - A MenuStrip docked in a short panel is as high as its items whatever
 *    the canvas's height: it does not take the clamp to the canvas's height a
 *    Menu opened on the canvas has (Menu::calculateSize), which would read a
 *    size from beyond its parent that a canvas resize no longer invalidates
 *    it for. Shrunk below the strip and grown again, the canvas lays it out
 *    with no layout-verify difference.
 *  - Nested 8 deep and resized in both axes (smaller, larger, smaller), the
 *    tree is laid out in one pass, the next frame runs none, no shown control
 *    is left dirty and every control's bounds are those of a canvas built at
 *    that size; back at the first size they are the first listing, with no
 *    layout-verify difference.
 *  - A resize there and back costs at most once per axis and resize per
 *    control in invalidate() calls, and each 4 levels more (two panels and
 *    two ScrollControls) cost exactly the same at depths 4, 8, 12 and 16: no
 *    control pays for its depth. Main's walks made one call per resized
 *    ancestor and two ways up to the canvas per resized control.
 *  - Outside a pass (an input-phase setSize on a ScrollControl in a plain
 *    panel) the way up is kept: the request, the new size and
 *    onBoundsChanged each walk up to the canvas, the new size invalidates
 *    the control's children, every ancestor needs layout and is dirty, as on
 *    main, and the next frames settle.
 *  - A page drawn shown, hidden as a TabControl leaves the pages switched
 *    away from, and resized with its TabControl: the resize marks the page
 *    (and, as the page is given the new size, its children) and nothing
 *    deeper, in fewer invalidate() calls than the page has controls; shown,
 *    the page and every shown control are where a canvas built at that size
 *    with the page shown puts them, with no layout-verify difference. Its
 *    pixels are checked in tests/hiddenPageResize.cpp.
 */
#include "headlessFixture.h"

#include <gweni/controls/label.h>
#include <gweni/controls/menuStrip.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/tabButton.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/treeNode.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;

    explicit Scene(const Point &size)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(size.x, size.y);
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

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int frame=0; frame<3; ++frame)
            this->frame();
    }
};

//! Layout verification on for the scope, and the failures it found in it.
struct Verify
{
    Verify(): m_outer(controls::layoutVerifyEnabled()), m_failures(controls::layoutVerifyFailures())
    {
        controls::setLayoutVerify(true);
    }
    ~Verify() { controls::setLayoutVerify(m_outer); }

    size_t failures() const { return controls::layoutVerifyFailures()-m_failures; }

private:
    bool m_outer;
    size_t m_failures;
};

//! A chain of \a depth containers, each docked Center in the one above (the
//! first in \a parent): even levels plain panels, odd ones ScrollControls
//! scrolling vertically only (their inner panel the viewport's width, so its
//! contents are resized with it). Each holds a Label docked Top before the
//! next. The innermost holds \a leaves Labels docked Top and a TreeControl
//! docked Bottom whose open TreeNodes nest 4 deep, a leaf at each level, so
//! TreeNode inner panels are resized too. \return the containers, outermost
//! first.
std::vector<controls::Base *> buildNest(controls::Base *parent, int depth, int leaves)
{
    std::vector<controls::Base *> levels;

    for(int level=0; level<depth; ++level)
    {
        controls::Base *container;

        if(level%2 == 1)
        {
            controls::ScrollControl *scroll=parent->newChild<controls::ScrollControl>();
            scroll->setScroll(false, true);
            container=scroll;
        }
        else
            container=parent->newChild<controls::Base>();

        container->setDock(DockPosition::Center);
        container->setPadding(Padding(2, 2, 2, 2));

        controls::Label *label=container->newChild<controls::Label>();
        label->setDock(DockPosition::Top);
        label->setText("level "+std::to_string(level));

        levels.push_back(container);
        parent=container;
    }

    for(int i=0; i<leaves; ++i)
    {
        controls::Label *label=parent->newChild<controls::Label>();
        label->setDock(DockPosition::Top);
        label->setText("leaf "+std::to_string(i));
    }

    controls::TreeControl *tree=parent->newChild<controls::TreeControl>();
    tree->setDock(DockPosition::Bottom);

    controls::TreeNode *node=tree->addNode("root");
    for(int level=0; level<4; ++level)
    {
        node->addNode("leaf");
        controls::TreeNode *next=node->addNode("level");
        node->open();
        node=next;
    }
    node->addNode("last");
    node->open();
    return levels;
}

void forEachActual(controls::Base *control, const std::function<void(controls::Base *)> &func)
{
    func(control);
    for(controls::Base *child : control->getActualChildren())
        forEachActual(child, func);
}

size_t controlCount(controls::Base *root)
{
    size_t count=0;
    forEachActual(root, [&](controls::Base *) { ++count; });
    return count;
}

//! Every control under \a root (itself included) in pre-order over
//! getActualChildren(), one "<Type> x,y wxh [hidden]" line each: what two
//! trees built alike are laid out to, whatever their ids.
std::string listing(controls::Base *root)
{
    std::string out;
    forEachActual(root, [&](controls::Base *control)
        {
            const Rect &b=control->getBounds();

            out+=std::string(control->getTypeName())+" "+std::to_string(b.x)+","+std::to_string(b.y)+" "+
                std::to_string(b.w)+"x"+std::to_string(b.h)+(control->hidden()?" hidden":"")+"\n";
        });
    return out;
}

//! listing(), but of the shown controls only: a hidden subtree keeps the
//! bounds of the last pass that entered it.
std::string shownListing(controls::Base *control)
{
    if(control->hidden())
        return std::string();

    const Rect &b=control->getBounds();
    std::string out=std::string(control->getTypeName())+" "+std::to_string(b.x)+","+std::to_string(b.y)+" "+
        std::to_string(b.w)+"x"+std::to_string(b.h)+"\n";

    for(controls::Base *child : control->getActualChildren())
        out+=shownListing(child);
    return out;
}

}//namespace

TEST_CASE("Size invalidation: a MenuStrip in a short panel is as high as its items at any canvas height")
{
    Verify verify;
    Scene scene(Point(400, 300));

    controls::Base *panel=scene.canvas->newChild<controls::Base>();
    panel->setDock(DockPosition::Top);
    panel->setHeight(60);

    controls::MenuStrip *strip=panel->newChild<controls::MenuStrip>();
    strip->addItem("File");
    strip->addItem("Edit");
    scene.settle();

    const int height=strip->getHeight();

    REQUIRE(height > 12);
    CHECK(strip->getPreferredSize().height == height);

    // a canvas shorter than the strip, then as tall as before
    scene.canvas->setSize(400, height/2);
    scene.settle();
    CHECK(strip->getHeight() == height);
    CHECK(strip->getPreferredSize().height == height);

    scene.canvas->setSize(400, 300);
    scene.settle();
    CHECK(strip->getHeight() == height);
    CHECK(verify.failures() == 0);
}

TEST_CASE("Size invalidation: nested containers resized in both axes are laid out in one pass as a fresh canvas is")
{
    Verify verify;
    const Point large(800, 600), small(600, 450);
    Scene scene(large);
    const std::vector<controls::Base *> levels=buildNest(scene.canvas, 8, 12);

    scene.settle();

    const std::string atLarge=listing(scene.canvas);
    const int innerWidth=levels.back()->getWidth();

    for(const Point &size : {small, large, small})
    {
        CAPTURE(size.x);
        scene.canvas->setSize(size.x, size.y);
        scene.frame();
        // one pass lays the new size out, and the next frame has nothing to do
        CHECK(scene.canvas->getLayoutStats().passes == 1);
        scene.frame();
        CHECK(scene.canvas->getLayoutStats().passes == 0);

        // every control where a canvas built at that size puts it
        Scene fresh(size);
        buildNest(fresh.canvas, 8, 12);
        fresh.settle();
        CHECK(listing(scene.canvas) == listing(fresh.canvas));
        // nothing is left dirty
        forEachActual(scene.canvas, [&](controls::Base *control)
            {
                if(control->visible())
                    CHECK_MESSAGE(!control->layoutDirty(), control->getTypeName());
            });
    }
    CHECK(levels.back()->getWidth() != innerWidth);

    scene.canvas->setSize(large.x, large.y);
    scene.settle();
    CHECK(listing(scene.canvas) == atLarge);
    CHECK(verify.failures() == 0);
}

TEST_CASE("Size invalidation: a resize invalidates each control a constant number of times whatever its depth")
{
    // the nest at depths 4, 8, 12 and 16: each step adds 4 levels alike (two
    // plain panels and two ScrollControls, each with its Label), the 40
    // leaves and the TreeControl staying at the bottom
    const int depths[]={4, 8, 12, 16};
    size_t calls[4], controls[4];

    for(int i=0; i<4; ++i)
    {
        Scene scene(Point(800, 600));
        buildNest(scene.canvas, depths[i], 40);
        scene.settle();

        // a resize in both axes, then back: every container and every Label
        // under them has a new width, the containers a new height
        const size_t start=controls::invalidateCalls();

        for(const Point &size : {Point(600, 450), Point(800, 600)})
        {
            scene.canvas->setSize(size.x, size.y);
            scene.frame();
        }
        calls[i]=controls::invalidateCalls()-start;
        controls[i]=controlCount(scene.canvas);

        CAPTURE(depths[i]);
        CHECK(scene.canvas->getLayoutStats().passes == 1);
        // at most once per axis and resize, for each control
        CHECK(calls[i] <= 4*controls[i]);
    }

    // each 4 levels more cost the same: the deeper ones as much as the first,
    // the leaves nothing more for being deeper. Main's walks made one call
    // per resized ancestor and two ways up to the canvas per resized control
    // (14 per control at depth 4 and 41 at depth 16 in the nested-resize
    // bench scene).
    for(int i=1; i<3; ++i)
    {
        CAPTURE(depths[i]);
        CHECK(controls[i+1]-controls[i] == controls[i]-controls[i-1]);
        CHECK(calls[i+1]-calls[i] == calls[i]-calls[i-1]);
    }
}

TEST_CASE("Size invalidation: a resize outside a pass still walks up to the canvas")
{
    Scene scene(Point(800, 600));
    const std::vector<controls::Base *> levels=buildNest(scene.canvas, 6, 4);

    scene.settle();

    // a ScrollControl in a plain panel, whose onChildBoundsChanged() does
    // nothing of its own
    controls::Base *resized=levels[5];
    size_t depth=0;

    for(controls::Base *up=resized->getParent(); up; up=up->getParent())
        ++depth;

    forEachActual(scene.canvas, [&](controls::Base *control)
        {
            if(control->visible())
                REQUIRE(!control->layoutDirty());
        });

    // the input phase: the request invalidates it and its way up, its new
    // size its children (main: its subtree) and the way up again, and
    // onBoundsChanged the way up once more, as on main
    const size_t calls=controls::invalidateCalls();

    resized->setSize(resized->getWidth()-10, resized->getHeight()-10);

    const size_t children=resized->getActualChildren().size();

    CHECK(controls::invalidateCalls()-calls == children+3*(depth+1));
    for(controls::Base *up=resized; up; up=up->getParent())
    {
        CAPTURE(up->getTypeName());
        CHECK(up->needsLayout());
        CHECK(up->layoutDirty());
    }
    for(controls::Base *child : resized->getActualChildren())
        CHECK(child->layoutDirty());

    // and the next pass lays it out again where its dock puts it
    scene.settle();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
}

TEST_CASE("Size invalidation: a hidden page resized with its TabControl is laid out when shown as one built shown")
{
    Verify verify;
    const Point large(800, 600), small(600, 450);

    auto build=[](controls::Base *parent) -> controls::TabControl *
    {
        controls::TabControl *tabs=parent->newChild<controls::TabControl>();

        tabs->setDock(DockPosition::Center);
        tabs->addPage("First")->getPage()->newChild<controls::Label>()->setText("first page");
        buildNest(tabs->addPage("Second")->getPage(), 6, 8);
        return tabs;
    };

    Scene scene(large);
    controls::TabControl *tabs=build(scene.canvas);
    controls::Base *page=tabs->getTab(1)->getPage();

    // drawn shown, then hidden as a TabControl leaves a page switched away from
    tabs->getTab(1)->doAction();
    scene.settle();
    tabs->getTab(0)->doAction();
    scene.settle();
    REQUIRE(page->hidden());

    // what is dirty under it already: its hidden parts (scroll bars that do
    // not show, closed or never-entered subtrees), which no pass enters
    auto dirtyUnder=[](controls::Base *root)
    {
        std::vector<controls::Base *> dirty;
        for(controls::Base *child : root->getActualChildren())
            forEachActual(child, [&](controls::Base *control)
                {
                    if(control->layoutDirty())
                        dirty.push_back(control);
                });
        return dirty;
    };
    const std::vector<controls::Base *> before=dirtyUnder(page);
    const size_t calls=controls::invalidateCalls();

    // its TabControl resized: the hidden page is marked (and, as it is given
    // the new size, its children), nothing deeper
    scene.canvas->setSize(small.x, small.y);
    scene.settle();
    CHECK(page->layoutDirty());
    for(controls::Base *control : dirtyUnder(page))
    {
        if(std::find(before.begin(), before.end(), control) == before.end())
            CHECK(control->getActualParent() == page);
    }
    // main walked the page's subtree too, from each resized ancestor
    CHECK(controls::invalidateCalls()-calls < controlCount(page));

    // shown: as a page built shown at that size
    tabs->getTab(1)->doAction();
    scene.settle();

    Scene fresh(small);
    controls::TabControl *freshTabs=build(fresh.canvas);
    freshTabs->getTab(1)->doAction();
    fresh.settle();

    CHECK(listing(page) == listing(freshTabs->getTab(1)->getPage()));
    CHECK(shownListing(scene.canvas) == shownListing(fresh.canvas));
    CHECK(verify.failures() == 0);
}

