/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The z renumber enters only the part of the tree an order change can
 *  relabel (Base::renumberZRecursive with ZNumbering::skipClean): a control
 *  whose child list or layer counts changed marks itself and its ancestors
 *  m_zDirty, and the walk passes a clean subtree whose first label is still
 *  at or past the cursor in O(1), jumping to its cached end (m_zEnd). A
 *  removal walks nothing at all.
 *
 *  It gives exactly the labels of the walk that enters every control: two
 *  identical trees take the same seeded 300 random inserts, deletes, delayed
 *  deletes, moves, reparents and replaced skin Controls, one of them with
 *  Canvas::invalidateZOrder() (which enters every control, #327's walk)
 *  before each renumber, and after each frame every control's z and overlay z
 *  are the same in both, with the renderer's maxZIndex() roomy, small enough
 *  to compact a few times, and too small for the tree (ties). The skipping
 *  walk never enters more controls than the full one.
 *
 *  What it enters: an insert enters the path to it, itself and the controls
 *  its labels cascade into, no more than depth + 1 + zChanged, and the same
 *  count in a tree of 61 controls as in one of 2401; a removal or a delayed
 *  delete enters none; a moved subtree is entered whole, and a clean sibling
 *  of it not; N Labels with the oldest deleted and a new one added per frame
 *  (gweni_bench's add-remove) enter the same few controls at N=100 and 2000.
 */
#include "headlessFixture.h"

#include <gweni/controls/label.h>

#include <doctest/doctest.h>

#include <cstdint>
#include <random>
#include <vector>

using namespace gweni;

namespace
{

//! Reserves StubSkin::depthLayers z values (2).
class Layered: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Layered, controls::Base)
    {}
};

//! Reserves StubSkin::overlayLayers z values after its subtree (1).
class Overlaid: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Overlaid, controls::Base)
    {}
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    //! A second skin whose Controls reserve other counts, for a replaced skin
    //! Control (which Base reports for that control alone).
    test::StubSkin other{&renderer};
    controls::Canvas *canvas;
    std::vector<controls::Base *> branches;
    //! Call Canvas::invalidateZOrder() before each renumber: the walk that
    //! enters every control.
    bool walkAll=false;

    Scene()
    {
        skin.depthLayers[Layered::getTypeNameStatic()]=2;
        skin.overlayLayers[Overlaid::getTypeNameStatic()]=1;
        other.depthLayers[controls::Base::getTypeNameStatic()]=3;
        other.depthLayers[Layered::getTypeNameStatic()]=1;
        other.overlayLayers[Layered::getTypeNameStatic()]=2;
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        delete canvas;
    }

    //! \p branches branches of \p children children, each of \p leaves
    //! leaves; every third child Layered and every fifth Overlaid.
    void populate(int branchCount, int children, int leaves)
    {
        for(int b=0; b<branchCount; ++b)
        {
            controls::Base *branch=add(canvas, (b%4) == 3?2:0);
            branches.push_back(branch);

            for(int c=0; c<children; ++c)
            {
                controls::Base *child=add(branch, (c%3) == 2?1:((c%5) == 4?2:0));

                for(int l=0; l<leaves; ++l)
                    add(child, 0);
            }
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

    void renumberHook()
    {
        if(walkAll && canvas->zOrderDirty())
            canvas->invalidateZOrder();
    }

    //! One frame; the update phase's UpdateStats.
    controls::UpdateStats frame()
    {
        renumberHook();
        canvas->updateCanvas();
        const controls::UpdateStats stats=canvas->getUpdateStats();
        // the render phase's renumber has nothing to do: no layout adds here
        renumberHook();
        canvas->renderCanvas();
        return stats;
    }

    void settle()
    {
        for(int i=0; i<5; ++i)
            frame();
        REQUIRE_FALSE(canvas->zOrderDirty());
    }

    //! every control on the canvas, depth-first
    std::vector<controls::Base *> all() const
    {
        std::vector<controls::Base *> result;
        test::forEachControl(canvas, [&](controls::Base *control) { result.push_back(control); });
        return result;
    }

    //! z and overlay z of every control, depth-first
    std::vector<std::pair<int, int>> labels() const
    {
        std::vector<std::pair<int, int>> result;
        for(controls::Base *control : all())
            result.emplace_back(control->getZIndex(), control->getOverlayZIndex());
        return result;
    }
};

//! The depth of \p control below the canvas (a canvas child is 1).
size_t depthOf(controls::Base *control)
{
    size_t depth=0;
    for(controls::Base *parent=control->getParent(); parent; parent=parent->getParent())
        ++depth;
    return depth;
}

//! Applies the same random step to \p a and \p b, two identical trees.
void randomStep(Scene &a, Scene &b, std::mt19937 &rng)
{
    auto pick=[&](size_t n) { return (size_t)(rng()%(uint32_t)n); };
    const std::vector<controls::Base *> listA=a.all();
    const std::vector<controls::Base *> listB=b.all();
    REQUIRE(listA.size() == listB.size());
    // the canvas itself never moves
    const size_t target=1+pick(listA.size()-1);
    const size_t op=pick(9);
    CAPTURE(op);

    auto both=[&](auto &&action) {
        action(a, listA);
        action(b, listB);
    };

    switch(op)
    {
    case 0:
    case 1:
    {
        const size_t n=1+pick(4);
        std::vector<int> kinds;
        for(size_t i=0; i<n; ++i)
            kinds.push_back((int)pick(3));
        both([&](Scene &, const std::vector<controls::Base *> &list) {
            for(int kind : kinds)
                Scene::add(list[target], kind);
        });
        break;
    }
    case 2:
        both([&](Scene &, const std::vector<controls::Base *> &list) {
            if(list.size() > 150)
                delete list[target];
            else
                Scene::add(list[target], 0);
        });
        break;
    case 3:
        both([&](Scene &, const std::vector<controls::Base *> &list) {
            if(list.size() > 150)
                list[target]->delayedDelete();
            else
                list[target]->bringToFront();
        });
        break;
    case 4:
        both([&](Scene &, const std::vector<controls::Base *> &list) { list[target]->sendToBack(); });
        break;
    case 5:
    {
        const size_t siblings=listA[target]->getParent()->getChildren().size();
        const size_t other=pick(siblings);
        const bool behind=pick(2) == 1;
        both([&](Scene &, const std::vector<controls::Base *> &list) {
            controls::Base *control=list[target];
            controls::Base *next=*std::next(control->getParent()->getChildren().begin(), (long)other);
            if(next != control)
                control->bringNextToControl(next, behind);
        });
        break;
    }
    case 6:
    {
        // to a control outside its own subtree
        const size_t parent=pick(listA.size());
        both([&](Scene &, const std::vector<controls::Base *> &list) {
            bool inside=false;
            test::forEachControl(list[target], [&](controls::Base *c) { inside|=(c == list[parent]); });
            if(!inside && (list[parent] != list[target]->getParent()))
                list[target]->setParent(list[parent]);
        });
        break;
    }
    case 7:
    {
        const bool toOther=pick(2) == 1;
        const bool recursive=pick(2) == 1;
        both([&](Scene &scene, const std::vector<controls::Base *> &list) {
            list[target]->setSkin(toOther?&scene.other:&scene.skin, recursive);
        });
        break;
    }
    default:
        // several changes in one frame
        both([&](Scene &, const std::vector<controls::Base *> &list) {
            Scene::add(list[target], 1);
            list[target]->bringToFront();
        });
        break;
    }
}

}//namespace

TEST_CASE("Z walk: skipping clean subtrees gives the labels of the walk that enters every control")
{
    for(const int maxZ : {65535, 1300, 120})
    {
        CAPTURE(maxZ);
        Scene skipping;
        Scene full;
        full.walkAll=true;
        skipping.renderer.maxZ=maxZ;
        full.renderer.maxZ=maxZ;
        skipping.populate(10, 6, 1);
        full.populate(10, 6, 1);
        REQUIRE(skipping.labels() == full.labels());

        std::mt19937 rng(20261108u);
        size_t entered=0;
        size_t enteredFull=0;
        size_t compactions=0;

        for(int step=0; step<300; ++step)
        {
            CAPTURE(step);
            // one seed for both trees' choices: randomStep draws each once
            randomStep(skipping, full, rng);

            const controls::UpdateStats stats=skipping.frame();
            const controls::UpdateStats statsFull=full.frame();
            CHECK(stats.zVisited <= statsFull.zVisited);
            CHECK(stats.zChanged == statsFull.zChanged);
            entered+=stats.zVisited;
            enteredFull+=statsFull.zVisited;
            // a compaction's full pass after the first
            compactions+=(statsFull.zVisited > full.all().size())?1:0;

            const bool same=skipping.labels() == full.labels();
            CHECK(same);
            if(!same)
                break;
        }
        MESSAGE("maxZ ", maxZ, ": the walk entered ", entered, " controls, the full walk ",
            enteredFull, "; ", compactions, " compactions");
        if(maxZ == 1300)
            CHECK(compactions >= 1);

        // given back before `other` goes (Scene's members outlive the canvas
        // anyway; this keeps no control on it)
        for(Scene *scene : {&skipping, &full})
            scene->canvas->setSkin(&scene->skin, true);
    }
}

TEST_CASE("Z walk: an insert enters the path to it and its cascade, whatever the size of the tree")
{
    size_t visited[2]={};
    int run=0;

    for(const int branches : {4, 160})
    {
        CAPTURE(branches);
        Scene scene;
        // 4 branches of 7 children of 1 leaf: 61 controls with the canvas, or
        // 2401 at 160 branches
        scene.populate(branches, 7, 1);
        CHECK(scene.all().size() == (size_t)(1+branches*(1+7*2)));

        // in the middle of the second branch's children, under one of them
        controls::Base *parent=*std::next(scene.branches[1]->getChildren().begin(), 3);
        controls::Base *added=Scene::add(parent, 1);
        const controls::UpdateStats stats=scene.frame();

        CAPTURE(stats.zVisited);
        CAPTURE(stats.zChanged);
        CHECK(stats.zChanged >= 1);
        CHECK(stats.zVisited <= depthOf(added)+1+stats.zChanged);
        visited[run++]=stats.zVisited;

        // and the next frame has nothing to number
        CHECK(scene.frame().zVisited == 0);
    }
    CHECK(visited[0] == visited[1]);
}

TEST_CASE("Z walk: a removal enters nothing, and a moved subtree is entered whole")
{
    Scene scene;
    scene.populate(20, 6, 1);
    const size_t size=scene.all().size();

    SUBCASE("a control deleted in place")
    {
        delete scene.branches[3]->getChildren().back();
        CHECK(scene.canvas->zOrderDirty());
        CHECK(scene.frame().zVisited == 0);
    }

    SUBCASE("the last branch taken off, then a control added after the rest")
    {
        const int high=scene.branches[18]->getChildren().back()->getChildren().back()->getZIndex();
        controls::Base *removed=scene.branches.back();
        removed->setParent(nullptr);
        CHECK(scene.frame().zVisited == 0);
        delete removed;

        // past every kept label, so the gap after the last one is free again:
        // the new control is spread out from there, as after a full walk
        controls::Base *added=Scene::add(scene.canvas, 0);
        const controls::UpdateStats stats=scene.frame();
        CHECK(stats.zVisited == 2);
        CHECK(added->getZIndex() == high+1);
    }

    SUBCASE("a delayed delete")
    {
        scene.branches[12]->delayedDelete();
        CHECK(scene.frame().zVisited == 0);
    }

    SUBCASE("a branch brought to the front")
    {
        controls::Base *moved=scene.branches[5];
        size_t m=0;
        test::forEachControl(moved, [&](controls::Base *) { ++m; });
        moved->bringToFront();
        const controls::UpdateStats stats=scene.frame();
        // the canvas and the moved branch; the branches it passed keep theirs
        CHECK(stats.zVisited == 1+m);
        CHECK(stats.zVisited < size);
    }
}

TEST_CASE("Z walk: deleting the oldest of N Labels and adding one enters the same few controls at any N")
{
    size_t visited[2]={};
    int run=0;

    for(const int n : {100, 2000})
    {
        CAPTURE(n);
        Scene scene;
        std::vector<controls::Label *> labels;
        for(int i=0; i<n; ++i)
            labels.push_back(scene.canvas->newChild<controls::Label>());
        scene.settle();

        size_t most=0;
        for(int i=0; i<6; ++i)
        {
            if(i%2 == 0)
                delete labels.front();
            else
                labels.front()->delayedDelete();
            labels.erase(labels.begin());
            labels.push_back(scene.canvas->newChild<controls::Label>());
            most=std::max(most, scene.frame().zVisited);
        }
        CHECK(most > 0);
        CHECK(most <= 4);
        visited[run++]=most;
    }
    CHECK(visited[0] == visited[1]);
}
