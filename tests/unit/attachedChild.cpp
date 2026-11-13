/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A child attached to a corner of its anchor (Base::attachTo, what a Badge
 *  is built on), on a real Canvas with the StubSkin, the anchors plain
 *  controls with no code of their own:
 *  - it is centred on its corner, moved by its offset, at its own size, at
 *    each of the four corners; after the anchor is resized it follows the
 *    corner, and after a pure move it keeps its place in the anchor, the move
 *    entering neither it nor the anchor's other children; an anchor of 20
 *    children (the walk of the changed children) places it the same, and so
 *    does one whose own measure and arrange skip its children;
 *  - neither it nor a new size of it counts in the anchor's preferred size,
 *    and its new size enters the way down to it alone;
 *  - it renders in the region its anchor renders in, not in the anchor's
 *    bounds (which clip the anchor's other children), so its overhang shows;
 *  - its z: the anchor's parent reserves one overlay z per attached child of
 *    its children, after its subtree, handed out in child order, so it is in
 *    front of the anchor's later sibling and that sibling's child, and behind
 *    the parent's later sibling and the canvas's later children; an anchor on
 *    the canvas itself reserves its own, after its subtree;
 *  - hit-testing passes it over: a point on its overhang finds the sibling
 *    under it, one on its part inside the anchor finds the anchor;
 *  - it hides and shows with its anchor, is deleted with it (every id back),
 *    and detached by setParent() it gives its Placement and the parent's
 *    reserved z back.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>

#include <doctest/doctest.h>

#include <map>
#include <string>

using namespace gweni;

namespace
{

//! A plain control that counts the layout entries it gets, records the clip
//! region its render() is called with, and says when it is deleted.
class Probe: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Probe, controls::Base)
    {}

    ~Probe() override
    {
        if(deleted)
            *deleted=true;
    }

    int entries=0;
    bool *deleted=nullptr;
    Rect seenClip;
    bool rendered=false;

protected:
    void calculateSize(skin::Base *skin, Dim dim) override
    {
        ++entries;
        ParentClass::calculateSize(skin, dim);
    }

    void arrange(skin::Base *skin, Dim dim) override
    {
        ++entries;
        ParentClass::arrange(skin, dim);
    }

    void render(skin::Base *skin) override
    {
        if(getStateChange() != controls::StateChange_Nothing)
        {
            seenClip=skin->getRenderer()->clipRegion();
            rendered=true;
        }
        ParentClass::render(skin);
    }
};

//! An anchor whose own measure and arrange enter none of its children (as
//! a control laying its children out by a LayoutItem, or drawing them
//! itself, does): Base measures and arranges its attached child anyway.
class Opaque: public Probe
{
public:
    GWENI_CONTROL_INLINE(Opaque, Probe)
    {}

protected:
    void calculateSize(skin::Base * /*skin*/, Dim dim) override
    {
        if(dim == Dim::X)
            m_preferredSize.width=40;
        else
            m_preferredSize.height=20;
    }

    void arrange(skin::Base * /*skin*/, Dim /*dim*/) override {}
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

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        delete canvas;
    }

    //! A Probe in \p parent at \p bounds, its size fixed.
    Probe *add(controls::Base *parent, const Rect &bounds)
    {
        Probe *probe=parent->newChild<Probe>();

        probe->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        probe->setBounds(bounds);
        return probe;
    }

    //! A w x h Probe attached to \p anchor's \p corner.
    Probe *attach(controls::Base *anchor, Corner corner, int w, int h, Point offset=Point(0, 0))
    {
        Probe *probe=anchor->newChild<Probe>();

        probe->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        probe->setSize(w, h);
        probe->attachTo(anchor, corner, offset);
        return probe;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int i=0; i < 3; ++i)
            frame();
    }
};

//! "x,y,w,h", so a failed check shows both rects
std::string str(const Rect &rect)
{
    return std::to_string(rect.x)+","+std::to_string(rect.y)+","
        +std::to_string(rect.w)+","+std::to_string(rect.h);
}

}//namespace

TEST_CASE("Attached child: centred on each corner, moved by its offset")
{
    Scene scene;
    controls::Base *panel=scene.add(scene.canvas, Rect(0, 0, 300, 200));
    Probe *anchor=scene.add(panel, Rect(50, 40, 80, 30));
    Probe *topLeft=scene.attach(anchor, Corner::TopLeft, 20, 12);
    Probe *topRight=scene.attach(anchor, Corner::TopRight, 20, 12);
    Probe *bottomLeft=scene.attach(anchor, Corner::BottomLeft, 21, 13);
    Probe *bottomRight=scene.attach(anchor, Corner::BottomRight, 20, 12, Point(-3, 4));

    scene.settle();

    CHECK(topLeft->isAttached());
    CHECK(anchor->hasAttachedChildren());
    CHECK(topLeft->getPlacement() == controls::Placement::Owner);
    CHECK((bottomRight->getAttachedCorner() == Corner::BottomRight));
    CHECK(bottomRight->getAttachedOffset().x == -3);
    CHECK(str(topLeft->getBounds()) == str(Rect(-10, -6, 20, 12)));
    CHECK(str(topRight->getBounds()) == str(Rect(70, -6, 20, 12)));
    // an odd size has its extra unit past the corner
    CHECK(str(bottomLeft->getBounds()) == str(Rect(-10, 24, 21, 13)));
    CHECK(str(bottomRight->getBounds()) == str(Rect(67, 28, 20, 12)));

    SUBCASE("a new size of the anchor: they follow its corners")
    {
        anchor->setSize(100, 40);
        scene.settle();

        CHECK(str(topLeft->getBounds()) == str(Rect(-10, -6, 20, 12)));
        CHECK(str(topRight->getBounds()) == str(Rect(90, -6, 20, 12)));
        CHECK(str(bottomLeft->getBounds()) == str(Rect(-10, 34, 21, 13)));
        CHECK(str(bottomRight->getBounds()) == str(Rect(87, 38, 20, 12)));
    }

    SUBCASE("a pure move of the anchor enters neither them nor its other children")
    {
        Probe *inside=scene.add(anchor, Rect(0, 0, 10, 10));

        scene.settle();
        inside->entries=0;
        topRight->entries=0;
        bottomRight->entries=0;
        anchor->setPos(60, 45);
        scene.frame();

        CHECK(anchor->getX() == 60);
        CHECK(inside->entries == 0);
        CHECK(topRight->entries == 0);
        CHECK(bottomRight->entries == 0);
        CHECK(str(topRight->getBounds()) == str(Rect(70, -6, 20, 12)));
    }

    SUBCASE("a new corner and offset place it again")
    {
        topLeft->attachTo(anchor, Corner::BottomRight, Point(1, 1));
        scene.settle();

        CHECK(str(topLeft->getBounds()) == str(Rect(71, 25, 20, 12)));
    }
}

TEST_CASE("Attached child: an anchor of 20 children places it as one of 2 does")
{
    Scene scene;
    controls::Base *panel=scene.add(scene.canvas, Rect(0, 0, 300, 200));
    Probe *anchor=scene.add(panel, Rect(50, 40, 200, 100));

    for(int i=0; i < 20; ++i)
        scene.add(anchor, Rect(i*5, 0, 4, 4));

    Probe *attached=scene.attach(anchor, Corner::BottomRight, 20, 12);

    scene.settle();
    CHECK(str(attached->getBounds()) == str(Rect(190, 94, 20, 12)));

    // the changed children alone are walked
    anchor->setHeight(120);
    scene.settle();
    CHECK(str(attached->getBounds()) == str(Rect(190, 114, 20, 12)));

    attached->setSize(30, 12);
    scene.settle();
    CHECK(str(attached->getBounds()) == str(Rect(185, 114, 30, 12)));
}

TEST_CASE("Attached child: measured and arranged by Base when its anchor's own layout skips it")
{
    Scene scene;
    controls::Base *panel=scene.add(scene.canvas, Rect(0, 0, 300, 200));
    Opaque *anchor=panel->newChild<Opaque>();

    anchor->setPos(10, 10);

    Probe *attached=scene.attach(anchor, Corner::BottomRight, 20, 12);

    scene.settle();
    CHECK(anchor->getWidth() == 40);
    CHECK(str(attached->getBounds()) == str(Rect(30, 14, 20, 12)));
    CHECK(attached->entries == 4);

    attached->entries=0;
    attached->setSize(30, 16);
    scene.frame();
    CHECK(str(attached->getBounds()) == str(Rect(25, 12, 30, 16)));
    CHECK(attached->entries == 4);
    CHECK(scene.canvas->getLayoutStats().passes == 1);
}

TEST_CASE("Attached child: not in its anchor's preferred size, and its new size enters only its way down")
{
    Scene scene;
    controls::Base *panel=scene.add(scene.canvas, Rect(0, 0, 300, 200));
    Probe *anchor=panel->newChild<Probe>();
    Probe *sibling=scene.add(panel, Rect(150, 0, 40, 20));

    anchor->setPos(10, 10);
    scene.add(anchor, Rect(0, 0, 30, 20));
    scene.settle();

    const Size before=anchor->getPreferredSize();

    CHECK(before.width == 30);
    CHECK(before.height == 20);

    Probe *attached=scene.attach(anchor, Corner::BottomRight, 50, 50);

    scene.settle();
    CHECK(anchor->getPreferredSize().width == before.width);
    CHECK(anchor->getPreferredSize().height == before.height);
    CHECK(anchor->getWidth() == 30);

    anchor->entries=0;
    sibling->entries=0;
    attached->entries=0;
    attached->setSize(60, 50);
    scene.frame();

    CHECK(anchor->getPreferredSize().width == before.width);
    CHECK(str(attached->getBounds()) == str(Rect(0, -5, 60, 50)));
    // the anchor and the child it holds, X and Y, measured and arranged
    CHECK(anchor->entries == 4);
    CHECK(attached->entries == 4);
    CHECK(sibling->entries == 0);
    CHECK(scene.canvas->getLayoutStats().passes == 1);
}

TEST_CASE("Attached child: renders in its anchor's region, not in its anchor's bounds")
{
    Scene scene;
    Probe *panel=scene.add(scene.canvas, Rect(10, 10, 200, 100));
    Probe *anchor=scene.add(panel, Rect(20, 20, 50, 20));
    Probe *inside=scene.add(anchor, Rect(0, 0, 10, 10));
    Probe *attached=scene.attach(anchor, Corner::TopRight, 20, 12);

    scene.frame();

    REQUIRE(anchor->rendered);
    REQUIRE(inside->rendered);
    REQUIRE(attached->rendered);
    // the panel's children render in the panel (on the canvas at 10, 10)
    CHECK(str(anchor->seenClip) == str(Rect(10, 10, 200, 100)));
    // a plain child of the anchor in the anchor's bounds
    CHECK(str(inside->seenClip) == str(Rect(30, 30, 50, 20)));
    // the attached one where the anchor renders
    CHECK(str(attached->seenClip) == str(Rect(10, 10, 200, 100)));

    SUBCASE("through the queue's ancestor chains too")
    {
        // a few controls queued among many: no walk of the tree
        for(int i=0; i < 40; ++i)
            scene.add(panel, Rect(100+i, 60, 1, 1));
        scene.settle();

        attached->rendered=false;
        inside->rendered=false;
        attached->redraw();
        inside->redraw();
        scene.frame();

        REQUIRE(attached->rendered);
        REQUIRE(inside->rendered);
        CHECK(str(attached->seenClip) == str(Rect(10, 10, 200, 100)));
        CHECK(str(inside->seenClip) == str(Rect(30, 30, 50, 20)));
    }

    SUBCASE("cut where the anchor's parent cuts the anchor")
    {
        anchor->setPos(-5, -5);
        attached->rendered=false;
        scene.frame();

        REQUIRE(attached->rendered);
        CHECK(str(attached->seenClip) == str(Rect(10, 10, 200, 100)));
        CHECK(attached->getBounds().y == -6);
    }
}

TEST_CASE("Attached child: drawn in front of the anchor's later siblings, behind what follows its parent")
{
    Scene scene;
    controls::Base *panel=scene.add(scene.canvas, Rect(0, 0, 300, 200));
    Probe *anchor=scene.add(panel, Rect(10, 10, 50, 20));
    Probe *next=scene.add(panel, Rect(60, 10, 50, 20));
    Probe *nextChild=scene.add(next, Rect(0, 0, 20, 10));
    Probe *second=scene.add(panel, Rect(110, 10, 50, 20));
    controls::Base *after=scene.add(scene.canvas, Rect(0, 210, 300, 50));
    Probe *attached=scene.attach(anchor, Corner::TopRight, 20, 12);
    Probe *attachedSecond=scene.attach(second, Corner::TopLeft, 20, 12);

    scene.settle();

    CHECK(panel->attachedOverlayLayers() == 2);
    CHECK(attached->getAttachedZIndex() == panel->getOverlayZIndex());
    CHECK(attached->getAttachedZIndex() > next->getZIndex());
    CHECK(attached->getAttachedZIndex() > nextChild->getZIndex());
    CHECK(attached->getAttachedZIndex() > second->getZIndex());
    // in child order, one each
    CHECK(attachedSecond->getAttachedZIndex() == attached->getAttachedZIndex()+1);
    CHECK(attachedSecond->getAttachedZIndex() < after->getZIndex());
    // its own preorder z is still the anchor's child's
    CHECK(attached->getZIndex() < next->getZIndex());

    SUBCASE("a control added to the canvas later is in front")
    {
        controls::Base *last=scene.add(scene.canvas, Rect(0, 0, 10, 10));

        scene.settle();
        CHECK(attachedSecond->getAttachedZIndex() < last->getZIndex());
    }

    SUBCASE("detached, it gives its Placement and the reserved z back")
    {
        attachedSecond->setParent(nullptr);
        scene.settle();

        CHECK_FALSE(attachedSecond->isAttached());
        CHECK_FALSE(second->hasAttachedChildren());
        CHECK(attachedSecond->getPlacement() == controls::Placement::Parent);
        CHECK(panel->attachedOverlayLayers() == 1);
        delete attachedSecond;
    }

    SUBCASE("its anchor taken off the parent: the parent's reserve goes")
    {
        second->setParent(scene.canvas);
        scene.settle();

        CHECK(panel->attachedOverlayLayers() == 1);
        // on the canvas, the anchor reserves its own, after its subtree
        CHECK(second->attachedOverlayLayers() == 1);
        CHECK(attachedSecond->getAttachedZIndex() == second->getOverlayZIndex());
        CHECK(attachedSecond->getAttachedZIndex() > attachedSecond->getZIndex());
    }
}

TEST_CASE("Attached child: takes no mouse input")
{
    Scene scene;
    controls::Base *panel=scene.add(scene.canvas, Rect(0, 0, 300, 200));
    Probe *anchor=scene.add(panel, Rect(10, 10, 50, 20));
    Probe *next=scene.add(panel, Rect(60, 10, 50, 20));
    Probe *attached=scene.attach(anchor, Corner::TopRight, 20, 12);

    scene.settle();
    REQUIRE(str(attached->getBounds()) == str(Rect(40, -6, 20, 12)));

    // on its overhang, over the next button: that button
    CHECK(scene.canvas->getControlAt(65, 12) == next);
    // on its overhang over nothing but the panel: the panel
    CHECK(scene.canvas->getControlAt(55, 6) == panel);
    // on its part inside the anchor: the anchor
    CHECK(scene.canvas->getControlAt(55, 12) == anchor);
}

TEST_CASE("Attached child: hidden and shown with its anchor, deleted with it")
{
    Scene scene;
    controls::Base *panel=scene.add(scene.canvas, Rect(0, 0, 300, 200));
    Probe *anchor=scene.add(panel, Rect(10, 10, 50, 20));
    bool deleted=false;

    scene.settle();

    // the canvas, the panel and the anchor hold theirs
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    Probe *attached=scene.attach(anchor, Corner::TopRight, 20, 12);

    attached->deleted=&deleted;
    scene.settle();
    CHECK(attached->visible());

    anchor->hide();
    scene.frame();
    CHECK_FALSE(attached->visible());
    CHECK_FALSE(attached->hidden());

    anchor->show();
    scene.frame();
    CHECK(attached->visible());

    delete anchor;
    scene.settle();
    CHECK(deleted);
    CHECK(panel->attachedOverlayLayers() == 0);
    // the anchor's and the attached child's back
    CHECK((long)test::IdBankCounts::now().livePrimitives()-(long)baseline.livePrimitives()
        == -(long)test::StubControl::primitivesPerControl);
}
