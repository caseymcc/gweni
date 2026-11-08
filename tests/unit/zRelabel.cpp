/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas::renumberZOrder() hands out z with gaps and relabels only what an
 *  order change put out of order (Base::renumberZRecursive): a control keeps
 *  its z while that is still past the ranges before it, a moved subtree
 *  (bringToFront, sendToBack, bringNextToControl, a reparent) takes new ones,
 *  and the first numbering leaves Canvas::ZGap free after every range, so a
 *  change costs the controls it moved or added plus the few after them up to
 *  a free gap, whatever the size of the tree. Past the last numbered control
 *  a new one leaves the gap too, and a moved one Canvas::MovedZGap.
 *
 *  In a settled tree of 260 controls: k controls added in the middle change
 *  at most k+8 z indices, and only those carry StateChange_Geometry; a removal
 *  changes none; a move of an m-control subtree changes at most m+8. A seeded
 *  sequence of 400 random inserts, deletes, moves, reparents and
 *  depthLayers()/overlayLayers() changes keeps every control's ranges in tree
 *  order (test::zOrderViolation) after each frame. When the labels run past
 *  the renderer's maxZIndex() one full pass spreads the tree over the range
 *  again, with a smaller gap, packed when it barely fits, and with ties, every
 *  z still within it, when even packed it does not.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <vector>

using namespace gweni;

namespace
{

//! Reserves StubSkin::depthLayers z values (2 unless a case changes it).
class Layered: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Layered, controls::Base)
    {}
};

//! Reserves StubSkin::overlayLayers z values after its subtree (1 unless a
//! case changes it).
class Overlaid: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Overlaid, controls::Base)
    {}
};

struct Scene
{
    static constexpr int Branches=20;
    static constexpr int Children=12;

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    std::vector<controls::Base *> branches;

    Scene()
    {
        skin.depthLayers[Layered::getTypeNameStatic()]=2;
        skin.overlayLayers[Overlaid::getTypeNameStatic()]=1;
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        delete canvas;
    }

    //! 20 branches of 12 children, every third child Layered and every fifth
    //! Overlaid: 261 controls with the canvas.
    void populate()
    {
        for(int b=0; b<Branches; ++b)
        {
            controls::Base *branch=add(canvas, (b%4) == 3?2:0);
            branches.push_back(branch);

            for(int c=0; c<Children; ++c)
                add(branch, (c%3) == 2?1:((c%5) == 4?2:0));
        }
        settle();
    }

    //! A new child of \p parent: 0 a plain control, 1 Layered, 2 Overlaid.
    static controls::Base *add(controls::Base *parent, int kind)
    {
        controls::Base *control;

        if(kind == 1)
            control=parent->newChild<Layered>();
        else if(kind == 2)
            control=parent->newChild<Overlaid>();
        else
            control=parent->newChild<controls::Base>();
        control->setBounds(0, 0, 10, 10);
        return control;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int i=0; i<5; ++i)
            frame();
        REQUIRE_FALSE(canvas->zOrderDirty());
    }

    std::string violation() const
    {
        return test::zOrderViolation(canvas, renderer.maxZ);
    }

    //! every control on the canvas, depth-first
    std::vector<controls::Base *> all() const
    {
        std::vector<controls::Base *> result;
        test::forEachControl(canvas, [&](controls::Base *control) { result.push_back(control); });
        return result;
    }

    struct Labels
    {
        int z;
        int overlayZ;
    };

    std::map<controls::Base *, Labels> labels() const
    {
        std::map<controls::Base *, Labels> result;
        for(controls::Base *control : all())
            result[control]={control->getZIndex(), control->getOverlayZIndex()};
        return result;
    }

    //! Runs the update phase alone and returns the controls it marked
    //! StateChange_Geometry, checking that each is new (not in \p before) or
    //! had its z or overlay z changed, and that zChanged counts exactly them.
    std::set<controls::Base *> update(const std::map<controls::Base *, Labels> &before)
    {
        canvas->updateCanvas();
        std::set<controls::Base *> marked;

        for(controls::Base *control : all())
        {
            const auto old=before.find(control);
            const bool fresh=old == before.end();
            const bool moved=fresh || (old->second.z != control->getZIndex())
                || (old->second.overlayZ != control->getOverlayZIndex());
            const bool geometry=(control->getStateChange()&controls::StateChange_Geometry) != 0;

            CAPTURE(control->getControlId());
            if(!fresh)
                CHECK(geometry == moved);
            if(moved)
                marked.insert(control);
        }
        CHECK(canvas->getUpdateStats().zChanged == marked.size());
        canvas->renderCanvas();
        return marked;
    }
};

//! Controls in \p control's subtree, itself included.
size_t subtreeSize(controls::Base *control)
{
    size_t count=0;
    test::forEachControl(control, [&](controls::Base *) { ++count; });
    return count;
}

}//namespace

TEST_CASE("Z relabel: controls added in the middle change only their own z and a few after them")
{
    for(const size_t k : {size_t(1), size_t(5), size_t(20)})
    {
        CAPTURE(k);
        Scene scene;
        scene.populate();
        REQUIRE(scene.all().size() == 261);
        CHECK(scene.violation() == "");

        const auto before=scene.labels();
        // a child of the middle branch: 130 or so controls come after it
        controls::Base *parent=*std::next(scene.branches[Scene::Branches/2]->getChildren().begin(), 5);
        std::vector<controls::Base *> added;
        for(size_t i=0; i<k; ++i)
            added.push_back(Scene::add(parent, (int)(i%3)));

        const std::set<controls::Base *> marked=scene.update(before);

        CHECK(scene.violation() == "");
        // the canvas, the branch and the parent, then the added controls and
        // the ones after them that they pushed on (every one relabelled)
        CHECK(scene.canvas->getUpdateStats().zVisited == 3+marked.size());
        CHECK(marked.size() <= k+8);
        for(controls::Base *control : added)
            CHECK(marked.count(control) == 1);

        // and the next frame has nothing to number
        scene.frame();
        CHECK(scene.canvas->getUpdateStats().zVisited == 0);
    }
}

TEST_CASE("Z relabel: controls added after the last one are spread out, at no cost to the rest")
{
    Scene scene;
    scene.populate();

    for(int round=0; round<3; ++round)
    {
        CAPTURE(round);
        const auto before=scene.labels();
        std::vector<controls::Base *> added;
        for(int i=0; i<10; ++i)
            added.push_back(Scene::add(scene.canvas, 0));

        const std::set<controls::Base *> marked=scene.update(before);
        CHECK(scene.violation() == "");
        CHECK(marked.size() == added.size());

        // past every kept label, each new one leaves the gap after it
        for(size_t i=1; i<added.size(); ++i)
            CHECK(added[i]->getZIndex() == added[i-1]->getZIndex()+1+controls::Canvas::ZGap);
    }
}

TEST_CASE("Z relabel: a subtree moved after the last one leaves the smaller Canvas::MovedZGap")
{
    Scene scene;
    scene.populate();
    controls::Base *moved=scene.branches.front();
    const int high=scene.branches.back()->getChildren().back()->getZIndex();

    moved->bringToFront();
    scene.frame();
    CHECK(scene.violation() == "");

    // every range of it is above the old top, one gap apart; it is the last
    // subtree, so the first plain child follows the branch's own range
    CHECK(moved->getZIndex() > high);
    int expected=moved->getZIndex()+1+controls::Canvas::MovedZGap;
    for(controls::Base *child : moved->getChildren())
    {
        CAPTURE(child->getControlId());
        CHECK(child->getZIndex() == expected);
        const skin::Control *skinControl=child->getSkinControl();
        expected=child->getZIndex()
            +(int)const_cast<skin::Control *>(skinControl)->depthLayers()+controls::Canvas::MovedZGap;
        if(child->getOverlayZIndex())
            expected=child->getOverlayZIndex()+1+controls::Canvas::MovedZGap;
    }
}

TEST_CASE("Z relabel: a removal changes no z")
{
    Scene scene;
    scene.populate();

    SUBCASE("a subtree taken off and deleted")
    {
        controls::Base *removed=scene.branches[7];
        const auto before=scene.labels();
        removed->setParent(nullptr);
        CHECK(scene.update(before).empty());
        delete removed;
        CHECK(scene.violation() == "");
    }

    SUBCASE("a control deleted in place")
    {
        const auto before=scene.labels();
        delete scene.branches[3]->getChildren().front();
        CHECK(scene.update(before).empty());
        CHECK(scene.violation() == "");
    }

    SUBCASE("a delayed delete")
    {
        const auto before=scene.labels();
        scene.branches[12]->delayedDelete();
        scene.canvas->updateCanvas();
        CHECK(scene.canvas->getUpdateStats().zChanged == 0);
        scene.canvas->renderCanvas();
        CHECK(scene.violation() == "");
    }
}

TEST_CASE("Z relabel: a moved subtree takes new z values, the rest of the tree keeps nearly all of its own")
{
    Scene scene;
    scene.populate();
    const auto before=scene.labels();
    controls::Base *moved=nullptr;

    SUBCASE("sendToBack of the last branch")
    {
        moved=scene.branches.back();
        moved->sendToBack();
    }

    SUBCASE("bringToFront of the first branch")
    {
        moved=scene.branches.front();
        moved->bringToFront();
    }

    SUBCASE("bringToFront of a middle branch")
    {
        moved=scene.branches[9];
        moved->bringToFront();
    }

    SUBCASE("bringNextToControl in front of")
    {
        moved=scene.branches[15];
        moved->bringNextToControl(scene.branches[4], false);
    }

    SUBCASE("bringNextToControl behind")
    {
        moved=scene.branches[2];
        moved->bringNextToControl(scene.branches[17], true);
    }

    SUBCASE("a child reparented to an earlier branch")
    {
        moved=scene.branches[18]->getChildren().front();
        Scene::add(moved, 1);
        Scene::add(moved, 0);
        scene.settle();
        const auto settled=scene.labels();
        moved->setParent(scene.branches[1]);

        const std::set<controls::Base *> marked=scene.update(settled);
        CHECK(scene.violation() == "");
        CHECK(marked.size() <= subtreeSize(moved)+8);
        return;
    }

    REQUIRE(moved);
    const size_t m=subtreeSize(moved);
    CAPTURE(m);
    const std::set<controls::Base *> marked=scene.update(before);

    CHECK(scene.violation() == "");
    CHECK(marked.size() <= m+8);
    // the rest of the tree is not relabelled
    size_t outside=0;
    for(controls::Base *control : marked)
    {
        bool inMoved=false;
        test::forEachControl(moved, [&](controls::Base *c) { inMoved|=(c == control); });
        outside+=inMoved?0:1;
    }
    CHECK(outside <= 8);
}

TEST_CASE("Z relabel: random inserts, deletes, moves and layer changes keep every range in tree order")
{
    Scene scene;
    scene.populate();

    // fixed seed, and no distribution (their output differs between standard
    // libraries): the same sequence everywhere
    std::mt19937 rng(20261108u);
    auto pick=[&](size_t n) { return (size_t)(rng()%(uint32_t)n); };
    size_t maxChanged=0;

    for(int step=0; step<400; ++step)
    {
        CAPTURE(step);
        std::vector<controls::Base *> controls=scene.all();
        // the canvas itself never moves
        controls::Base *target=controls[1+pick(controls.size()-1)];
        const size_t op=pick(9);
        CAPTURE(op);

        switch(op)
        {
        case 0:
        case 1:
            for(size_t i=0, n=1+pick(4); i<n; ++i)
                Scene::add(target, (int)pick(3));
            break;
        case 2:
            if(controls.size() > 150)
                delete target;
            else
                Scene::add(target, 0);
            break;
        case 3:
            target->bringToFront();
            break;
        case 4:
            target->sendToBack();
            break;
        case 5:
        {
            const controls::Base::List &siblings=target->getParent()->getChildren();
            controls::Base *other=*std::next(siblings.begin(), (long)pick(siblings.size()));
            if(other != target)
                target->bringNextToControl(other, pick(2) == 1);
            break;
        }
        case 6:
        {
            // to a control outside its own subtree
            controls::Base *parent=controls[pick(controls.size())];
            bool inside=false;
            test::forEachControl(target, [&](controls::Base *c) { inside|=(c == parent); });
            if(!inside && (parent != target->getParent()))
                target->setParent(parent);
            break;
        }
        case 7:
            scene.skin.depthLayers[Layered::getTypeNameStatic()]=1+pick(12);
            scene.canvas->invalidateZOrder();
            break;
        default:
            scene.skin.overlayLayers[Overlaid::getTypeNameStatic()]=pick(4);
            scene.canvas->invalidateZOrder();
            break;
        }

        scene.frame();
        const std::string violation=scene.violation();
        CHECK(violation == "");
        if(!violation.empty())
            break;
        if(op < 7)
            maxChanged=std::max(maxChanged, scene.canvas->getUpdateStats().zChanged);
    }
    MESSAGE("most z indices one structural step changed: ", maxChanged, " of ", scene.all().size());
}

TEST_CASE("Z relabel: labels past maxZIndex() are compacted in one full pass")
{
    Scene scene;
    // the first numbering of 261 controls and 289 ranges, spread out, and a
    // little room after it
    scene.renderer.maxZ=2600;
    scene.populate();
    CHECK(scene.violation() == "");

    // controls added at the end take the gap each until the range runs out
    bool compacted=false;
    for(int round=0; (round < 40) && !compacted; ++round)
    {
        CAPTURE(round);
        const size_t size=scene.all().size();
        for(int i=0; i<5; ++i)
            Scene::add(scene.canvas, 0);
        scene.frame();

        CHECK(scene.violation() == "");
        if(scene.canvas->getUpdateStats().zChanged > 5)
        {
            compacted=true;
            // the whole tree, once
            CHECK(scene.canvas->getUpdateStats().zChanged >= size/2);
            // the canvas and the 5 new ones, then the whole tree once
            CHECK(scene.canvas->getUpdateStats().zVisited == (1+5)+(size+5));
        }
    }
    CHECK(compacted);

    SUBCASE("packed, when the tree barely fits")
    {
        size_t layers=0;
        test::forEachControl(scene.canvas, [&](controls::Base *control) {
            skin::Control *skinControl=const_cast<skin::Control *>(control->getSkinControl());
            layers+=skinControl?skinControl->depthLayers()+skinControl->overlayLayers():1;
        });
        scene.renderer.maxZ=(int)layers+3;
        Scene::add(scene.branches[3], 0);
        scene.frame();
        CHECK(scene.violation() == "");

        // so the next insert in the middle relabels everything after it
        scene.renderer.maxZ+=1;
        Scene::add(scene.branches[3], 1);
        scene.frame();
        CHECK(scene.violation() == "");
    }

    SUBCASE("ties, every z within the range, when even packed it does not fit")
    {
        scene.renderer.maxZ=100;
        Scene::add(scene.branches[3], 0);
        scene.frame();

        test::forEachControl(scene.canvas, [&](controls::Base *control) {
            CHECK(control->getZIndex() >= 1);
            CHECK(control->getZIndex() <= 100);
            CHECK(control->getOverlayZIndex() <= 100);
        });
    }
}
