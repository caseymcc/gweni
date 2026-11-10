/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A child placed after its own arrange, in its parent's postLayout() (the
 *  end of the parent's Y arrange), on a real Canvas with the StubSkin (#522).
 *  The way down to it is still dirty when the pass ends, so the canvas runs
 *  a second pass in the same frame, which LayoutStats counts and names the
 *  placer for (redirtiedType), and once layoutCanvas returns uncapped no
 *  shown control is layoutDirty(). Main left that way dirty with the canvas
 *  clean, so the next pass something else caused, in any later frame,
 *  entered it and counted it as that frame's work: here an unrelated
 *  invalidation enters only its own way down (4 entries per control), the
 *  same as the next one. A placer in a setExcludedFromStats() subtree makes
 *  that second pass the excluded subtree's alone, which counts nothing
 *  (Canvas::passOnlyExcluded), in a frame the app laid something out in too.
 *
 *  A TreeNode, which centres its toggle on its title (the StubSkin's title
 *  is not the toggle's height, so the toggle moves on the first layout),
 *  places it in arrange(): its tree's first frame takes one pass and leaves
 *  nothing dirty, and an unrelated invalidation then enters only its own way.
 *  layout::Tile, which places its children in cells, and layout::Center, which
 *  aligns them, did so in postLayout() too; they now place in arrange(), and
 *  lay out in one pass.
 */
#include "headlessFixture.h"

#include <gweni/controls/layout/position.h>
#include <gweni/controls/layout/tile.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/treeNode.h>

#include <doctest/doctest.h>

#include <cstring>
#include <functional>
#include <string>

using namespace gweni;

namespace
{

//! A control that counts its layout() runs.
class Counted: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Counted, controls::Base)
    {}

    int layouts=0;

protected:
    void layout(skin::Base *skin) override
    {
        ++layouts;
        ParentClass::layout(skin);
    }
};

//! Owns one 20x20 child, sized in arrange() and placed `offset` down in
//! postLayout(): the pattern TreeNode used for its toggle.
class PostPlacer: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(PostPlacer, controls::Base)
    {
        child=newChild<Counted>("placed");
        child->setPlacement(controls::Placement::Owner);
    }

    Counted *child;
    int offset=10;

protected:
    void arrange(skin::Base *skin, Dim dim) override
    {
        placeChildSize(child, 20, 20);
        ParentClass::arrange(skin, dim);
    }

    void postLayout(skin::Base * /*skin*/) override
    {
        placeChildPos(child, 0, offset);
    }
};

void forEachActual(controls::Base *control, const std::function<void(controls::Base *)> &func)
{
    func(control);
    for(controls::Base *child : control->getActualChildren())
        forEachActual(child, func);
}

//! The shown controls still layoutDirty(), as Type#id, or "" for none.
std::string dirtyShown(controls::Base *root)
{
    std::string dirty;
    forEachActual(root, [&](controls::Base *control)
        {
            if(control->visible() && control->layoutDirty())
                dirty+=std::string(control->getTypeName())+"#"+std::to_string(control->getControlId())+" ";
        });
    return dirty;
}

//! A control's bounds as "x,y,w,h".
std::string boundsOf(const controls::Base *control)
{
    const Rect &b=control->getBounds();
    return std::to_string(b.x)+","+std::to_string(b.y)+","+std::to_string(b.w)+","+std::to_string(b.h);
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    //! An unrelated control and its child, for an invalidation that has
    //! nothing to do with what was placed: its way down is the canvas, other
    //! and leaf.
    controls::Base *other;
    controls::Base *leaf;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        other=canvas->newChild<controls::Base>("other");
        other->setBounds(300, 0, 100, 100);
        leaf=other->newChild<controls::Base>("leaf");
        leaf->setBounds(0, 0, 10, 10);
    }

    ~Scene()
    {
        delete canvas;
    }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i<10 && frame().passes != 0; ++i)
            ;
    }

    //! The unrelated invalidation, laid out alone in its frame.
    const controls::LayoutStats &touchLeaf()
    {
        leaf->invalidate();
        return frame();
    }
};

//! What the canvas, other and leaf take: 4 entries each.
const size_t LeafWay=3*4;

//! A PostPlacer in a plain panel on the canvas.
PostPlacer *addPlacer(Scene &scene, controls::Base **holderOut=nullptr)
{
    controls::Base *holder=scene.canvas->newChild<controls::Base>("holder");
    holder->setBounds(0, 0, 200, 200);
    PostPlacer *placer=holder->newChild<PostPlacer>("placer");
    placer->setBounds(0, 0, 100, 100);
    if(holderOut)
        *holderOut=holder;
    return placer;
}

void checkNamesPlacer(const controls::LayoutStats &stats, PostPlacer *placer)
{
    CHECK(stats.passes == 2);
    CHECK_FALSE(stats.capped);
    CHECK_FALSE(stats.renderLayout);
    REQUIRE(stats.redirtiedType != nullptr);
    CHECK(std::strcmp(stats.redirtiedType, "PostPlacer") == 0);
    CHECK(stats.redirtiedId == placer->getControlId());
}

}//namespace

TEST_CASE("postLayout placement: the way down to the placed child is laid out by a second pass in the same frame")
{
    Scene scene;
    PostPlacer *placer=addPlacer(scene);

    // the first layout moves the child from y 0 to its offset
    const controls::LayoutStats &first=scene.frame();
    checkNamesPlacer(first, placer);
    CHECK(dirtyShown(scene.canvas) == "");
    CHECK(placer->child->getBounds().y == 10);
    // its own layout() ran at its new bounds, in the frame that placed it
    CHECK_FALSE(placer->child->needsLayout());

    CHECK(scene.frame().passes == 0);

    // a new offset, from the placer's own invalidation
    const int layouts=placer->child->layouts;
    placer->offset=30;
    placer->invalidate();
    const controls::LayoutStats &moved=scene.frame();
    checkNamesPlacer(moved, placer);
    CHECK(dirtyShown(scene.canvas) == "");
    CHECK(placer->child->getBounds().y == 30);
    CHECK(placer->child->layouts > layouts);
    CHECK_FALSE(placer->child->needsLayout());

    CHECK(scene.frame().passes == 0);
}

TEST_CASE("postLayout placement: a later unrelated invalidation enters only its own way down")
{
    Scene scene;
    addPlacer(scene);
    scene.settle();
    CHECK(dirtyShown(scene.canvas) == "");

    const controls::LayoutStats &touched=scene.touchLeaf();
    CHECK(touched.passes == 1);
    CHECK(touched.redirtiedType == nullptr);
    CHECK(touched.visited == LeafWay);
    // the same as the next one, which nothing is left over for
    CHECK(scene.touchLeaf().visited == LeafWay);
}

TEST_CASE("postLayout placement: a placer in an excluded subtree adds no counted pass to the app's")
{
    Scene scene;
    controls::Base *holder=nullptr;
    PostPlacer *placer=addPlacer(scene, &holder);
    holder->setExcludedFromStats(true);
    scene.settle();
    CHECK(dirtyShown(scene.canvas) == "");

    // the excluded placer alone: no pass is the app's
    placer->offset=30;
    placer->invalidate();
    const controls::LayoutStats &alone=scene.frame();
    CHECK(alone.passes == 0);
    CHECK(alone.visited == 0);
    CHECK(dirtyShown(scene.canvas) == "");
    CHECK(placer->child->getBounds().y == 30);

    // with the app's invalidation in the same frame: the second pass is the
    // excluded subtree's alone, and the canvas's entries in it are not counted
    placer->offset=40;
    placer->invalidate();
    const controls::LayoutStats &both=scene.touchLeaf();
    CHECK(both.passes == 1);
    CHECK(both.visited == LeafWay);
    CHECK(dirtyShown(scene.canvas) == "");
    CHECK(placer->child->getBounds().y == 40);
}

TEST_CASE("postLayout placement: a TreeNode's toggle is placed in its first pass and leaves nothing dirty")
{
    Scene scene;
    auto *tree=scene.canvas->newChild<controls::TreeControl>("tree");
    tree->setBounds(0, 0, 200, 200);
    controls::TreeNode *open=tree->addNode("Open");
    open->addNode("Leaf");
    controls::TreeNode *closed=tree->addNode("Closed");
    closed->addNode("Leaf")->addNode("Deeper");
    open->open();

    const controls::LayoutStats &first=scene.frame();
    CHECK(first.passes == 1);
    CHECK(first.redirtiedType == nullptr);
    CHECK(dirtyShown(scene.canvas) == "");
    CHECK(scene.frame().passes == 0);

    CHECK(scene.touchLeaf().visited == LeafWay);
}

TEST_CASE("postLayout placement: layout::Tile and layout::Center place their children in one pass")
{
    Scene scene;
    auto *tile=scene.canvas->newChild<controls::layout::Tile>("tile");
    tile->setDock(DockPosition::None);
    tile->setBounds(0, 0, 50, 100);
    controls::Base *tiles[3];
    for(controls::Base *&child : tiles)
    {
        child=tile->newChild<controls::Base>();
        child->setSize(10, 10);
    }

    auto *center=scene.canvas->newChild<controls::layout::Center>("center");
    center->setBounds(100, 0, 100, 100);
    controls::Base *centred=center->newChild<controls::Base>();
    centred->setSize(10, 10);

    const controls::LayoutStats &first=scene.frame();
    CHECK(first.passes == 1);
    CHECK(first.redirtiedType == nullptr);
    CHECK(first.requests == 0);
    CHECK(dirtyShown(scene.canvas) == "");

    // 22-unit cells, two to a 50-unit row, each child centred in its cell
    CHECK(boundsOf(tiles[0]) == "6,6,10,10");
    CHECK(boundsOf(tiles[1]) == "28,6,10,10");
    CHECK(boundsOf(tiles[2]) == "6,28,10,10");
    CHECK(boundsOf(centred) == "45,45,10,10");

    CHECK(scene.frame().passes == 0);
    CHECK(scene.touchLeaf().visited == LeafWay);

    // a child taken out of the Tile is its new parent's to place again
    tiles[2]->setParent(scene.canvas);
    CHECK(tiles[2]->getPlacement() == controls::Placement::Parent);
}
