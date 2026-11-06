/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The clip region the render walk hands each control: a control's render()
 *  sees its parent's region, its children the parent's region narrowed to the
 *  control (unless it does not clip), and a region with no overlap left is an
 *  empty rect, never a negative one, that the walk still passes to dirty
 *  children.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

#include <map>
#include <string>

using namespace gweni;

namespace
{

//! A plain control that records the clip region its render() was called with.
class ClipProbe: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(ClipProbe, controls::Base)
    {}

    bool clips=true;
    std::map<ClipProbe *, Rect> *seen=nullptr;

    bool shouldClip() override { return clips; }

    void render(skin::Base *skin) override
    {
        if(seen && getStateChange() != controls::StateChange_Nothing)
            (*seen)[this]=skin->getRenderer()->clipRegion();
        ParentClass::render(skin);
    }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    std::map<ClipProbe *, Rect> seen;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        delete canvas;
    }

    ClipProbe *add(controls::Base *parent, const Rect &bounds)
    {
        ClipProbe *probe=parent->newChild<ClipProbe>();
        probe->seen=&seen;
        probe->setAlignment(Alignment::Absolute);
        probe->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        probe->setBounds(bounds);
        return probe;
    }

    void frame()
    {
        seen.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

//! "x,y,w,h", so a failed check shows both rects
std::string str(const Rect &rect)
{
    return std::to_string(rect.x)+","+std::to_string(rect.y)+","
        +std::to_string(rect.w)+","+std::to_string(rect.h);
}

}//namespace

TEST_CASE("addClipRegion intersects the region with the rect at the render offset")
{
    test::StubRenderer renderer;

    renderer.setClipRegion(Rect(10, 20, 100, 50));
    renderer.setRenderOffset(Point(0, 0));
    // only the rect's size counts; its position is the render offset
    renderer.addRenderOffset(Rect(60, 40, 0, 0));
    renderer.addClipRegion(Rect(999, 999, 80, 80));
    CHECK(str(renderer.clipRegion()) == str(Rect(60, 40, 50, 30)));
    CHECK(renderer.clipRegionVisible());

    // past the left and top edges
    renderer.setClipRegion(Rect(10, 20, 100, 50));
    renderer.setRenderOffset(Point(-30, -5));
    renderer.addClipRegion(Rect(0, 0, 60, 40));
    CHECK(str(renderer.clipRegion()) == str(Rect(10, 20, 20, 15)));
}

TEST_CASE("addClipRegion with no overlap gives an empty region, never a negative size")
{
    test::StubRenderer renderer;
    const Rect region(10, 20, 100, 50);

    const Point offsets[]={Point(200, 30), Point(-200, 30), Point(30, 200), Point(30, -200)};
    for(const Point &offset : offsets)
    {
        CAPTURE(offset.x);
        CAPTURE(offset.y);
        renderer.setClipRegion(region);
        renderer.setRenderOffset(offset);
        renderer.addClipRegion(Rect(0, 0, 40, 40));

        const Rect &clip=renderer.clipRegion();
        CHECK(clip.w >= 0);
        CHECK(clip.h >= 0);
        CHECK(clip.w*clip.h == 0);
        CHECK_FALSE(renderer.clipRegionVisible());

        // and narrowing an empty region again keeps it empty
        renderer.addRenderOffset(Rect(-50, -50, 0, 0));
        renderer.addClipRegion(Rect(0, 0, 400, 400));
        CHECK(renderer.clipRegion().w >= 0);
        CHECK(renderer.clipRegion().h >= 0);
        CHECK_FALSE(renderer.clipRegionVisible());
    }
}

TEST_CASE("a control renders in its parent's region and its children in its own")
{
    Scene scene;
    ClipProbe *outer=scene.add(scene.canvas, Rect(50, 40, 200, 100));
    ClipProbe *inner=scene.add(outer, Rect(150, 60, 100, 100));
    ClipProbe *leaf=scene.add(inner, Rect(10, 10, 20, 20));
    scene.frame();

    CHECK(str(scene.seen.at(outer)) == str(Rect(0, 0, 400, 300)));
    CHECK(str(scene.seen.at(inner)) == str(Rect(50, 40, 200, 100)));
    // inner is at (200, 100) on the canvas and runs past outer's right and
    // bottom edges, at (250, 140)
    CHECK(str(scene.seen.at(leaf)) == str(Rect(200, 100, 50, 40)));
}

TEST_CASE("a control that does not clip hands its children its parent's region")
{
    Scene scene;
    ClipProbe *outer=scene.add(scene.canvas, Rect(50, 40, 200, 100));
    ClipProbe *strip=scene.add(outer, Rect(10, 10, 20, 20));
    strip->clips=false;
    ClipProbe *leaf=scene.add(strip, Rect(0, 0, 60, 60));
    scene.frame();

    CHECK(str(scene.seen.at(leaf)) == str(Rect(50, 40, 200, 100)));
}

TEST_CASE("children of a control moved out of its parent are still rendered, with an empty region")
{
    Scene scene;
    ClipProbe *outer=scene.add(scene.canvas, Rect(50, 40, 200, 100));
    ClipProbe *moved=scene.add(outer, Rect(10, 10, 50, 50));
    ClipProbe *leaf=scene.add(moved, Rect(5, 5, 20, 20));
    for(int i=0; i<5; ++i)
        scene.frame();
    CHECK(scene.seen.empty());

    // a bounds change marks the whole subtree, so the leaf's primitives are
    // drawn again with the empty region rather than left with the old one
    moved->setPos(300, 10);
    scene.frame();

    REQUIRE(scene.seen.count(leaf) == 1);
    const Rect &clip=scene.seen.at(leaf);
    CHECK(clip.w >= 0);
    CHECK(clip.h >= 0);
    CHECK(clip.w*clip.h == 0);
    CHECK(leaf->getStateChange() == controls::StateChange_Nothing);
}
