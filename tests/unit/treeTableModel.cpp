/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  TreeTable's live model edits: compare every projection with an independent
 *  preorder rebuilt from the app's hierarchy, including edits under hidden
 *  and expanded parents. Selection follows its node through edits and moves;
 *  removing it or an ancestor chooses the removed subtree's next sibling,
 *  previous sibling, then parent, notifying once. The first shown node and
 *  its partial-row offset anchor edits above the view. Only changed or newly
 *  shown nodes are bound, with the same pool, controls and live ids at 1 000
 *  and 100 000 nodes; no context or resources are needed. Lazy children: a
 *  node's first expand asks once (until resetChildren), shows one "Loading…"
 *  placeholder row in the placeholder colour that the app's binder never
 *  sees and a press does not select, and an answer in the request or later
 *  takes its place in one splice on the same pooled rows; an empty answer
 *  takes the expander away; keys pass over a placeholder; a loading node
 *  under a collapsed ancestor, expandAll and expandSubtree ask once each.
 *  Sorting: a header click with a column comparator sorts every shown
 *  sibling group (ascending, then descending; no sort column gives the app's
 *  order back), stably, folders first if asked, equal to an independent
 *  sorted rebuild, the app's `children` order untouched; expansion, the
 *  selection and the first shown node are kept, a collapsed group is not
 *  compared until it is expanded; inserts, batches, moves, renames
 *  (nodeChanged), a leaf becoming a folder, removals and a lazy answer go to
 *  their sorted place; sorted edits bind the same rows with the same pool,
 *  controls and ids at 1 000 and 100 000 nodes. Review cases: a hidden
 *  selection (moved under a collapsed node) takes no Left/Right step and a
 *  collapseAll fires no event for it; a move under a loading node is its
 *  answer; siblings renamed before their nodeChanged still end sorted; a lazy
 *  answer whose placeholder is the top row shows its first child there;
 *  expandSubtree keeps the first shown node; a sort and then an insert above
 *  the viewport keep the stripes on screen; an edit leaves a pending
 *  scrollToItem alone; a parent whose last child goes is a collapsed leaf;
 *  a sorted move that changes its old and new parents' keys (folders first,
 *  a child-count key) leaves every group sorted, and seeded random edits
 *  while sorted match a sorted rebuild after each one, ties in the app's
 *  order. Combined seeded edits also cover lazy answers, batched renames
 *  into ties, every pooled row's depth/text/selection/stripes, and the anchor;
 *  lazy answers at the list end keep the view, placeholders at a new depth
 *  rebind, and placeholder cell contents hide and restore their own flags.
 *  A sorted batch and an immediate lazy answer that re-sorts its folder
 *  restore the anchor after the entire edit, despite intermediate clamps.
 *  GWENI_TREETABLE_SEED_FIRST/COUNT extend the combined model's seed range
 *  for manual stress runs; the suite defaults to seeds 1..1000.
 */
#include "headlessFixture.h"

#include <gweni/controls/treeTable.h>
#include <gweni/controls/checkBox.h>
#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <random>
#include <set>
#include <tuple>

using namespace gweni;

namespace
{

using Tree=controls::TreeTable;
using Id=Tree::NodeId;
constexpr Id Root=Tree::Root;

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas canvas{&skin};
    Tree *tree=canvas.newChild<Tree>();
    std::vector<Id> binds;
    std::map<Id, String> names;

    Scene()
    {
        canvas.setSize(600, 400);
        tree->setBounds(10, 10, 400, 200);
        tree->setColumnCount(2);
        tree->setColumnCaption(0, "Name");
        tree->setColumnCaption(1, "Children");
        tree->setRowHeight(20);
        tree->setNodeBinder([this](controls::DataTableRow *row, Id id, size_t)
            {
                binds.push_back(id);
                row->setCellText(0, names.count(id)?names.at(id):"node "+std::to_string(id));
                row->setCellText(1, std::to_string(tree->getNode(id)->children.size()));
            });
    }

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
    }

    void frame()
    {
        controls::resetChangeStats();
        canvas.updateCanvas();
        canvas.renderCanvas();
        CHECK(canvas.getLayoutStats().passes <= 1);
        CHECK(canvas.getLayoutStats().requests == 0);
        CHECK_FALSE(canvas.getLayoutStats().renderLayout);
    }

    void settle()
    {
        frame();
        frame();
        binds.clear();
        renderer.events.clear();
    }

    void roots(size_t count)
    {
        for(size_t i=0; i < count; ++i)
            REQUIRE(tree->addNode(Root, i) == i);
    }

    Id top() const { return tree->nodeForItem(tree->getVirtualBody()->firstVisibleItem()); }
};

// App-side hierarchy, deliberately independent of the table's model/store.
// Recompute depths and the visible preorder from roots after every edit.
struct Model
{
    struct Node
    {
        Id parent;
        std::vector<Id> children;
        bool expanded=false;
    };
    std::map<Id, Node> nodes;
    std::vector<Id> roots;

    std::vector<Id> &children(Id parent)
    {
        return parent == Root?roots:nodes.at(parent).children;
    }

    void insert(Id parent, size_t at, const std::vector<Id> &ids)
    {
        auto &siblings=children(parent);
        at=std::min(at, siblings.size());
        siblings.insert(siblings.begin()+at, ids.begin(), ids.end());
        for(Id id : ids)
            nodes.emplace(id, Node{parent, {}, false});
    }

    //! A parent whose last child went is a collapsed leaf.
    void emptied(Id parent)
    {
        if(parent != Root && nodes.at(parent).children.empty())
            nodes.at(parent).expanded=false;
    }

    void remove(Id id)
    {
        const Id parent=nodes.at(id).parent;
        auto &siblings=children(parent);
        siblings.erase(std::find(siblings.begin(), siblings.end(), id));
        emptied(parent);
        std::vector<Id> pending{id};
        while(!pending.empty())
        {
            const Id next=pending.back();
            pending.pop_back();
            const auto child=nodes.at(next).children;
            pending.insert(pending.end(), child.begin(), child.end());
            nodes.erase(next);
        }
    }

    void move(Id id, Id parent, size_t at)
    {
        const Id from=nodes.at(id).parent;
        auto &old=children(from);
        old.erase(std::find(old.begin(), old.end(), id));
        emptied(from);
        auto &next=children(parent);
        next.insert(next.begin()+std::min(at, next.size()), id);
        nodes.at(id).parent=parent;
    }

    void check(const Tree *tree) const
    {
        std::vector<Id> visible;
        std::vector<std::tuple<Id, size_t, bool>> pending;
        for(auto root=roots.rbegin(); root != roots.rend(); ++root)
            pending.emplace_back(*root, 0, true);
        while(!pending.empty())
        {
            const auto entry=pending.back();
            pending.pop_back();
            const Id id=std::get<0>(entry);
            const size_t depth=std::get<1>(entry);
            const bool shown=std::get<2>(entry);
            const auto &node=nodes.at(id);
            const auto *actual=tree->getNode(id);
            REQUIRE(actual != nullptr);
            CHECK(actual->parent == node.parent);
            CHECK(actual->depth == depth);
            CHECK(actual->children == node.children);
            CHECK(actual->expanded == node.expanded);
            if(shown)
                visible.push_back(id);
            for(auto child=node.children.rbegin(); child != node.children.rend(); ++child)
                pending.emplace_back(*child, depth+1, shown && node.expanded);
        }
        CHECK(tree->getNodeCount() == nodes.size());
        REQUIRE(tree->getItemCount() == visible.size());
        REQUIRE(tree->getVisibleNodes().size() == visible.size());
        for(size_t i=0; i < visible.size(); ++i)
        {
            CHECK(tree->nodeForItem(i) == visible[i]);
            CHECK(tree->itemForNode(visible[i]) == i);
            CHECK(tree->getVisibleNodes()[i].depth == tree->getNode(visible[i])->depth);
        }
        for(const auto &node : nodes)
        {
            if(std::find(visible.begin(), visible.end(), node.first) == visible.end())
                CHECK(tree->itemForNode(node.first) == Tree::NoItem);
        }
    }
};

struct Selections: event::Handler
{
    std::vector<Id> ids;
    void changed(event::Info info) { ids.push_back(info.integer); }
};

std::vector<controls::Base *> controlsIn(controls::Base *root)
{
    std::vector<controls::Base *> result{root};
    for(size_t i=0; i < result.size(); ++i)
    {
        const auto &children=result[i]->getActualChildren();
        result.insert(result.end(), children.begin(), children.end());
    }
    return result;
}

}//namespace

TEST_CASE("TreeTable model: append above the viewport keeps the first shown node")
{
    Scene scene;
    scene.roots(100);
    scene.tree->addNode(0, 1000);
    scene.tree->setExpanded(0, true);
    scene.settle();
    controls::VirtualList *body=scene.tree->getVirtualBody();
    body->setVerticalScrollOffset(50*20+7);
    scene.settle();
    const Id anchor=scene.top();
    std::map<controls::DataTableRow *, bool> stripes;
    for(controls::Base *control : body->getRows())
    {
        auto *row=gweni_cast<controls::DataTableRow>(control);
        if(!row->hidden())
            stripes[row]=row->isStriped();
    }

    REQUIRE(scene.tree->addNode(0, 1001) == 1001);
    scene.frame();
    CHECK(scene.top() == anchor);
    CHECK(body->getVerticalScrollOffset() == 51*20+7);
    CHECK(scene.binds.empty());
    for(const auto &stripe : stripes)
        CHECK(stripe.first->isStriped() == stripe.second);

    // An edit takes a thumb moved since layout at the old content size.
    controls::VerticalScrollBar *bar=nullptr;
    for(controls::Base *control : body->getActualChildren())
    {
        if(auto *vertical=gweni_cast<controls::VerticalScrollBar>(control))
            bar=vertical;
    }
    REQUIRE(bar != nullptr);
    bar->setScrolledAmount(0.63f, false);
    const int pending=static_cast<int>(std::lround(bar->getScrolledAmount()*body->getMaxVerticalScrollOffset()));
    const Id pendingAnchor=scene.tree->nodeForItem(static_cast<size_t>(pending/20));
    REQUIRE(scene.tree->addNode(0, 1002) == 1002);
    scene.frame();
    CHECK(scene.top() == pendingAnchor);
    CHECK(body->getVerticalScrollOffset() == pending+20);
}

TEST_CASE("TreeTable model: live edits match an independent rebuild at every position")
{
    Scene scene;
    Tree *tree=scene.tree;
    Model model;
    auto insert=[&](Id parent, size_t at, const std::vector<Id> &ids)
        {
            REQUIRE(tree->insertNodes(parent, at, ids));
            model.insert(parent, at, ids);
            model.check(tree);
        };
    auto expand=[&](Id id)
        {
            tree->setExpanded(id, true);
            model.nodes.at(id).expanded=true;
            model.check(tree);
        };
    auto move=[&](Id id, Id parent, size_t at)
        {
            REQUIRE(tree->moveNode(id, parent, at));
            model.move(id, parent, at);
            model.check(tree);
        };
    auto remove=[&](Id id)
        {
            REQUIRE(tree->removeNode(id));
            model.remove(id);
            model.check(tree);
            CHECK(tree->getNode(id) == nullptr);
            CHECK(tree->itemForNode(id) == Tree::NoItem);
        };
    std::vector<Id> roots;
    for(Id i=0; i < 120; ++i)
        roots.push_back(i);
    insert(Root, 0, roots);
    // Above, inside and below the viewport, with expanded and collapsed
    // subtrees. Expand descendants before their hidden ancestors as well.
    for(Id parent : {0, 50, 110})
    {
        insert(parent, 0, {1000+parent, 2000+parent, 3000+parent});
        insert(1000+parent, 0, {4000+parent, 5000+parent});
        expand(1000+parent);
    }
    expand(0);
    expand(50);
    scene.settle();
    tree->getVirtualBody()->setVerticalScrollOffset(50*20+7);
    scene.settle();

    Id fresh=10000;
    for(Id parent : {Root, Id(0), Id(50), Id(110), Id(1110)})
    {
        for(size_t at : {size_t(0), size_t(1), size_t(99999)})
        {
            const Id id=fresh++;
            REQUIRE(tree->insertNode(parent, at, id) == id);
            model.insert(parent, at, {id});
            model.check(tree);
            scene.frame();
            remove(id);
            scene.frame();
        }
    }
    move(1000, 50, 1);      // expanded -> expanded, including its subtree
    move(1050, 110, 0);     // expanded -> collapsed
    move(1110, 50, 99999);  // collapsed -> expanded, preserved child expansion
    move(2110, Root, 0);    // collapsed -> root
    move(3050, 1050, 0);    // visible -> hidden at a greater depth
    move(1050, 50, 0);      // show it again at its original depth
    move(50, Root, 0);      // same siblings, earlier and later
    move(50, Root, 99999);
    expand(110);
    remove(1000);          // expanded subtree
    remove(110);           // root and its descendants
    remove(0);
    scene.frame();

    // Invalid edits are atomic, including a bad id late in a batch.
    CHECK_FALSE(tree->insertNodes(987654, 0, {fresh}));
    CHECK_FALSE(tree->insertNodes(Root, 0, {fresh, Root}));
    CHECK_FALSE(tree->insertNodes(Root, 0, {fresh, fresh}));
    CHECK_FALSE(tree->insertNodes(Root, 0, {fresh, 1}));
    CHECK(tree->insertNodes(Root, 0, {}));
    CHECK_FALSE(tree->moveNode(50, 1050, 0)); // cycle
    CHECK_FALSE(tree->moveNode(50, 50, 0));
    CHECK_FALSE(tree->moveNode(50, 987654, 0));
    CHECK_FALSE(tree->moveNode(987654, Root, 0));
    CHECK_FALSE(tree->removeNode(Root));
    CHECK_FALSE(tree->removeNode(987654));
    scene.settle();
    CHECK(tree->moveNode(50, Root, tree->getItemCount()));
    model.check(tree);
    scene.frame();
    CHECK(scene.canvas.getLayoutStats().passes == 0);

    // Interleave the edits rather than always returning to the same fixture.
    // Fixed seed makes a failing projection reproducible.
    std::mt19937 random(638);
    for(size_t step=0; step < 250; ++step)
    {
        CAPTURE(step);
        if(model.nodes.empty())
            insert(Root, 0, {fresh++});
        std::vector<Id> candidates{Root};
        for(const auto &node : model.nodes)
            candidates.push_back(node.first);
        const Id parent=candidates[random()%candidates.size()];
        const Id id=candidates[1+random()%(candidates.size()-1)];
        const size_t at=random()%150;
        switch(random()%4)
        {
        case 0:
            insert(parent, at, {fresh, fresh+1});
            fresh+=2;
            break;
        case 1:
            remove(id);
            break;
        case 2:
        {
            bool cycle=false;
            for(Id ancestor=parent; ancestor != Root; ancestor=model.nodes.at(ancestor).parent)
                cycle=cycle || ancestor == id;
            if(!cycle)
                move(id, parent, at);
            break;
        }
        case 3:
        {
            auto &node=model.nodes.at(id);
            const bool expanded=!node.expanded;
            if(expanded && node.children.empty())
                break;
            tree->setExpanded(id, expanded);
            node.expanded=expanded;
            model.check(tree);
            break;
        }
        }
        scene.frame();
    }
}

TEST_CASE("TreeTable model: removing selection or its ancestor chooses the sibling then parent once")
{
    Scene scene;
    Tree *tree=scene.tree;
    Selections events;
    tree->onSelectionChange.add(&events, &Selections::changed);
    tree->addNode(Root, 1);
    tree->addNode(Root, 2);
    tree->addNode(Root, 3);
    tree->addNode(2, 20);
    tree->addNode(2, 21);
    tree->addNode(2, 22);
    tree->addNode(21, 210);
    tree->addNode(210, 2100);
    tree->expandAll();
    scene.settle();
    REQUIRE(tree->setSelectedNode(2));
    tree->setExpanded(2, false);
    CHECK(tree->getSelectedNode() == 2);
    CHECK(events.ids.empty()); // collapsing the selected parent keeps it
    tree->setExpanded(2, true);
    REQUIRE(tree->setSelectedNode(20));
    REQUIRE(tree->removeNode(20));
    CHECK(tree->getSelectedNode() == 21); // next sibling
    REQUIRE(tree->setSelectedNode(22));
    REQUIRE(tree->removeNode(22));
    CHECK(tree->getSelectedNode() == 21); // previous sibling
    REQUIRE(tree->setSelectedNode(2100));
    REQUIRE(tree->removeNode(21));
    CHECK(tree->getSelectedNode() == 2);  // the subtree's parent
    CHECK(tree->getNode(2100) == nullptr);
    CHECK(tree->getNode(210) == nullptr);
    REQUIRE(tree->removeNode(2));
    CHECK(tree->getSelectedNode() == 3);  // next root
    REQUIRE(tree->removeNode(3));
    CHECK(tree->getSelectedNode() == 1);  // previous root
    REQUIRE(tree->removeNode(1));
    CHECK(tree->getSelectedNode() == Tree::NoNode);
    CHECK(tree->getSelectedItem() == Tree::NoItem);
    CHECK(events.ids == std::vector<Id>{21, 21, 2, 3, 1, Tree::NoNode});
    scene.frame();

    // A removed ancestor chooses its own surviving sibling, not one of the
    // selected node's siblings that also went with the subtree.
    tree->addNode(Root, 10);
    tree->addNode(10, 11);
    tree->addNode(10, 12);
    tree->addNode(Root, 13);
    tree->addNode(Root, 14);
    tree->setExpanded(10, true);
    REQUIRE(tree->setSelectedNode(11));
    REQUIRE(tree->removeNode(10));
    CHECK(tree->getSelectedNode() == 13);
    CHECK(events.ids.back() == 13);
    const size_t notifications=events.ids.size();
    REQUIRE(tree->insertNode(Root, 0, 15) == 15);
    REQUIRE(tree->moveNode(13, Root, 0));
    REQUIRE(tree->removeNode(14)); // a different node
    CHECK(tree->getSelectedNode() == 13);
    CHECK(events.ids.size() == notifications);
    CHECK(tree->getSelectedItem() == tree->itemForNode(13));
    scene.frame();
    REQUIRE(tree->moveNode(13, 15, 0));
    CHECK(tree->getSelectedNode() == 13);
    CHECK(tree->getSelectedItem() == Tree::NoItem);
    REQUIRE(tree->removeNode(15)); // remove the hidden selection's ancestor
    CHECK(tree->getSelectedNode() == Tree::NoNode);
    CHECK(events.ids.size() == notifications+1);
    CHECK(events.ids.back() == Tree::NoNode);
    scene.frame();
}

TEST_CASE("TreeTable model: moves keep selection and the scroll anchor including a moved anchor")
{
    Scene scene;
    scene.roots(120);
    Tree *tree=scene.tree;
    controls::VirtualList *body=tree->getVirtualBody();
    Selections events;
    tree->onSelectionChange.add(&events, &Selections::changed);
    tree->addNode(0, 1000);
    tree->addNode(110, 1110);
    tree->setExpanded(0, true);
    scene.settle();
    body->setVerticalScrollOffset(50*20+7);
    scene.settle();
    const Id anchor=scene.top();
    const int y=tree->rowForItem(tree->itemForNode(anchor))->getY();
    REQUIRE(tree->setSelectedNode(anchor+1));
    auto checkAnchor=[&]()
        {
            scene.frame();
            CHECK(scene.top() == anchor);
            CHECK(body->getVerticalScrollOffset()%20 == 7);
            REQUIRE(tree->rowForItem(tree->itemForNode(anchor)) != nullptr);
            CHECK(tree->rowForItem(tree->itemForNode(anchor))->getY() == y);
            CHECK(tree->getSelectedNode() == anchor+1);
            CHECK(events.ids.empty());
        };
    REQUIRE(tree->insertNodes(Root, 0, {2000, 2001, 2002}));
    checkAnchor();
    REQUIRE(tree->removeNode(2001));
    checkAnchor();
    REQUIRE(tree->moveNode(5, Root, 90)); // above -> below
    checkAnchor();
    REQUIRE(tree->moveNode(100, Root, 0)); // below -> above
    checkAnchor();
    REQUIRE(tree->moveNode(anchor, Root, 80)); // anchor itself
    checkAnchor();
    REQUIRE(tree->moveNode(anchor+1, 110, 0)); // selection hidden
    checkAnchor();
    CHECK(tree->getSelectedItem() == Tree::NoItem);
    REQUIRE(tree->moveNode(110, Root, 0)); // its ancestor moves, still hidden
    checkAnchor();
    tree->setExpanded(110, true);
    checkAnchor();
    CHECK(tree->getSelectedItem() == tree->itemForNode(anchor+1));

    // Removing the anchor uses the next surviving row, losing the partial
    // offset as VirtualList does when its anchored item is removed.
    const Id next=tree->nodeForItem(body->firstVisibleItem()+1);
    REQUIRE(tree->removeNode(anchor));
    scene.frame();
    CHECK(scene.top() == next);
    CHECK(body->getVerticalScrollOffset()%20 == 0);
    REQUIRE(tree->moveNode(anchor+1, 110, 0)); // valid no-op
    tree->setExpanded(110, false);
    REQUIRE(tree->setSelectedNode(110));
    REQUIRE(tree->moveNode(110, 109, 0)); // a selected subtree becomes hidden
    CHECK(tree->getSelectedNode() == 110);
    CHECK(tree->setSelectedNode(Tree::NoNode));
    CHECK(tree->getSelectedNode() == Tree::NoNode);
}

TEST_CASE("TreeTable model: nodeChanged rebinds only its shown row and hidden edits do no work")
{
    Scene scene;
    scene.roots(120);
    Tree *tree=scene.tree;
    tree->addNode(2, 1000);
    scene.settle();
    const auto pool=tree->getVirtualBody()->getRows();
    const auto controls=controlsIn(tree);
    const auto serial=scene.canvas.treeSerial();

    scene.names[3]="renamed";
    tree->nodeChanged(3);
    scene.frame();
    CHECK(scene.binds == std::vector<Id>{3});
    CHECK(tree->rowForItem(3)->getCellText(0) == "renamed");
    scene.settle();

    tree->nodeChanged(1000); // hidden
    tree->nodeChanged(110);  // offscreen
    tree->nodeChanged(Root);
    tree->nodeChanged(999999);
    REQUIRE(tree->insertNodes(2, 0, {1001, 1002}));
    scene.frame();
    CHECK(scene.binds == std::vector<Id>{2}); // changed child count
    scene.settle();

    REQUIRE(tree->insertNode(110, 0, 2000) == 2000);
    REQUIRE(tree->moveNode(2000, 111, 0));
    REQUIRE(tree->removeNode(2000));
    scene.frame();
    CHECK(scene.binds.empty());
    CHECK(scene.canvas.getLayoutStats().passes == 0);
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
    scene.settle();

    // A shown leaf gains its first child: its row alone gets an expander.
    auto *row=gweni_cast<controls::TreeTableRow>(tree->rowForItem(4));
    REQUIRE(row != nullptr);
    REQUIRE(row->getExpander()->hidden());
    const Point at=row->getExpander()->localPosToCanvas();
    scene.canvas.inputMouseMoved(at.x+7, at.y+7, 0, 0);
    scene.frame();
    REQUIRE(HoveredControl != row->getExpander());
    REQUIRE(tree->insertNode(4, 0, 3000) == 3000);
    scene.frame();
    CHECK_FALSE(row->getExpander()->hidden());
    CHECK(HoveredControl == row->getExpander());
    CHECK(scene.binds == std::vector<Id>{4});
    scene.settle();
    REQUIRE(tree->removeNode(3000));
    scene.frame();
    CHECK(row->getExpander()->hidden());
    CHECK(scene.binds == std::vector<Id>{4});
    CHECK(tree->getVirtualBody()->getRows() == pool);
    CHECK(controlsIn(tree) == controls);
    CHECK(scene.canvas.treeSerial() == serial);
    CHECK(scene.renderer.events.empty());
}

TEST_CASE("TreeTable model: live edits bind locally with the same pool controls and ids at 1000 and 100000 nodes")
{
    std::vector<std::vector<size_t>> costs;
    for(size_t count : {1000, 100000})
    {
        CAPTURE(count);
        const auto baseline=test::IdBankCounts::now();
        {
            Scene scene;
            scene.roots(count-1);
            Tree *tree=scene.tree;
            tree->addNode(0, 1000000);
            tree->setExpanded(0, true);
            scene.settle();
            controls::VirtualList *body=tree->getVirtualBody();
            body->setVerticalScrollOffset(50*20+7);
            scene.settle();
            const auto pool=body->getRows();
            const auto controls=controlsIn(tree);
            const auto serial=scene.canvas.treeSerial();
            const auto prims=scene.renderer.livePrimitives;
            const auto texts=scene.renderer.liveTexts;
            const Id anchor=scene.top();
            const auto bank=test::IdBankCounts::now();
            std::vector<size_t> cost{pool.size(), controls.size(),
                bank.livePrimitives()-baseline.livePrimitives(),
                bank.liveTexts()-baseline.liveTexts()};
            auto checkFrame=[&](bool noBinds)
                {
                    scene.frame();
                    cost.push_back(scene.binds.size());
                    cost.push_back(controls::getChangeStats().controls);
                    cost.push_back(scene.canvas.getLayoutStats().visited);
                    cost.push_back(scene.renderer.frameStats().primitiveSlotsWritten);
                    CHECK(scene.binds.size() <= pool.size());
                    CHECK(body->getRows() == pool);
                    CHECK(controlsIn(tree) == controls);
                    CHECK(scene.canvas.treeSerial() == serial);
                    CHECK(scene.renderer.livePrimitives == prims);
                    CHECK(scene.renderer.liveTexts == texts);
                    CHECK(scene.renderer.events.empty());
                    CHECK(scene.top() == anchor);
                    if(noBinds)
                        CHECK(scene.binds.empty());
                    scene.binds.clear();
                };
            // A batch above, a single row within, and one below the viewport.
            // Each is removed as well, with no full projection rebuild.
            for(size_t at : {size_t(0), size_t(55), size_t(500)})
            {
                REQUIRE(tree->insertNodes(Root, at, {2000000, 2000001}));
                checkFrame(at != 55);
                REQUIRE(tree->removeNode(2000000));
                checkFrame(at != 55);
                REQUIRE(tree->removeNode(2000001));
                checkFrame(at != 55);
            }
            REQUIRE(tree->insertNode(Root, 55, 2000002) == 2000002);
            checkFrame(false);
            REQUIRE(tree->moveNode(2000002, 0, 0));
            checkFrame(false);
            REQUIRE(tree->removeNode(2000002));
            checkFrame(true);
            tree->nodeChanged(anchor);
            scene.frame();
            CHECK(scene.binds == std::vector<Id>{anchor});
            scene.binds.clear();
            tree->nodeChanged(count-2);
            checkFrame(true);
            scene.frame();
            CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
            CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
            costs.push_back(cost);
        }
        const auto after=test::IdBankCounts::now();
        CHECK(after.livePrimitives() == baseline.livePrimitives());
        CHECK(after.liveTexts() == baseline.liveTexts());
    }
    REQUIRE(costs.size() == 2);
    CHECK(costs[0] == costs[1]);
}

namespace
{

struct Requests: event::Handler
{
    Tree *tree=nullptr;
    std::vector<Id> ids;
    std::vector<Id> expanded;
    std::vector<Id> collapsed;
    //! What the request handler answers with, if anything.
    std::function<void(Id)> answer;
    void requested(event::Info info)
    {
        ids.push_back(info.integer);
        if(answer)
            answer(info.integer);
    }
    void onExpanded(event::Info info) { expanded.push_back(info.integer); }
    void onCollapsed(event::Info info) { collapsed.push_back(info.integer); }

    void listen(Tree *table)
    {
        tree=table;
        tree->onChildrenRequested.add(this, &Requests::requested);
        tree->onExpanded.add(this, &Requests::onExpanded);
        tree->onCollapsed.add(this, &Requests::onCollapsed);
    }
};

std::vector<Id> visibleIds(const Tree *tree)
{
    std::vector<Id> ids;
    for(const auto &node : tree->getVisibleNodes())
        ids.push_back(node.id);
    return ids;
}

controls::TreeTableRow *treeRow(Tree *tree, size_t item)
{
    return gweni_cast<controls::TreeTableRow>(tree->rowForItem(item));
}

constexpr Id P=Tree::NoNode;

}//namespace

TEST_CASE("TreeTable model: a lazy node asks once and shows its placeholder until a later answer replaces it")
{
    Scene scene;
    Requests requests;
    Selections selections;
    requests.listen(scene.tree);
    scene.tree->onSelectionChange.add(&selections, &Selections::changed);
    scene.roots(5);
    scene.settle();

    // advertised children give a leaf on screen its expander
    scene.tree->setHasChildren(2, true);
    scene.frame();
    REQUIRE(treeRow(scene.tree, 2) != nullptr);
    CHECK_FALSE(treeRow(scene.tree, 2)->getExpander()->hidden());
    CHECK(scene.tree->getNode(2)->hasChildren);
    scene.settle();
    const auto pool=scene.tree->getVirtualBody()->getRows();
    const auto controls=controlsIn(scene.tree);

    scene.tree->setExpanded(2, true);
    CHECK(requests.ids == std::vector<Id>{2});
    CHECK(requests.expanded == std::vector<Id>{2});
    CHECK(scene.tree->isExpanded(2));
    CHECK(scene.tree->getNode(2)->loading);
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 2, P, 3, 4});
    CHECK(scene.tree->getVisibleNodes()[3].depth == 1);
    scene.frame();
    controls::TreeTableRow *placeholder=treeRow(scene.tree, 3);
    REQUIRE(placeholder != nullptr);
    CHECK(placeholder->isPlaceholder());
    CHECK(placeholder->getCellText(0) == Tree::PlaceholderText);
    CHECK(placeholder->getCellText(1) == "");
    CHECK(placeholder->getExpander()->hidden());
    CHECK((placeholder->getCell(0)->getTextColor() == scene.skin.Colors.TreeTable.Placeholder));
    CHECK_FALSE((placeholder->getCell(1)->getTextColor() == scene.skin.Colors.TreeTable.Placeholder));
    // the app's binder never sees a placeholder
    CHECK(std::find(scene.binds.begin(), scene.binds.end(), P) == scene.binds.end());
    scene.settle();

    // a press on it selects nothing
    const Point at=placeholder->getCell(1)->localPosToCanvas();
    scene.canvas.inputMouseMoved(at.x+2, at.y+2, 0, 0);
    input::onMouseClicked(&scene.canvas, 0, true);
    input::onMouseClicked(&scene.canvas, 0, false);
    scene.frame();
    CHECK(scene.tree->getSelectedNode() == Tree::NoNode);
    CHECK(selections.ids.empty());
    scene.canvas.inputMouseMoved(590, 390, 0, 0);
    scene.settle();

    // collapsed and expanded again while loading: no second request
    scene.tree->setExpanded(2, false);
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 2, 3, 4});
    scene.tree->setExpanded(2, true);
    CHECK(requests.ids == std::vector<Id>{2});
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 2, P, 3, 4});
    scene.settle();

    // the later answer takes the placeholder's place in one splice
    REQUIRE(scene.tree->insertNodes(2, 0, {20, 21}));
    CHECK_FALSE(scene.tree->getNode(2)->loading);
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 2, 20, 21, 3, 4});
    scene.frame();
    CHECK(treeRow(scene.tree, 3) == placeholder);
    CHECK_FALSE(placeholder->isPlaceholder());
    CHECK(placeholder->getCellText(0) == "node 20");
    CHECK_FALSE((placeholder->getCell(0)->getTextColor() == scene.skin.Colors.TreeTable.Placeholder));
    // the answered node's row and its two children; 3 and 4 keep their rows
    CHECK(std::set<Id>(scene.binds.begin(), scene.binds.end()) == std::set<Id>{2, 20, 21});
    CHECK(scene.tree->getVirtualBody()->getRows() == pool);
    CHECK(controlsIn(scene.tree) == controls);
    CHECK(requests.ids == std::vector<Id>{2});
    CHECK(requests.expanded == std::vector<Id>{2, 2});
    CHECK(requests.collapsed == std::vector<Id>{2});
    scene.settle();
    scene.frame();
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
}

TEST_CASE("TreeTable model: lazy children answered in the request, or with none now or later")
{
    Scene scene;
    Requests requests;
    requests.listen(scene.tree);
    scene.roots(4);
    for(Id id=0; id < 4; ++id)
        scene.tree->setHasChildren(id, true);
    scene.settle();

    // answered in the call: the node expands straight onto its children
    requests.answer=[&](Id id)
        {
            if(id == 1)
                REQUIRE(scene.tree->insertNodes(1, 0, {10, 11}));
            if(id == 2)
                scene.tree->setHasChildren(2, false);
            if(id == 3)
                REQUIRE(scene.tree->insertNodes(3, 0, {}));
        };
    scene.tree->setExpanded(1, true);
    CHECK(requests.ids == std::vector<Id>{1});
    CHECK(requests.expanded == std::vector<Id>{1});
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 10, 11, 2, 3});
    scene.frame();
    for(controls::Base *control : scene.tree->getVirtualBody()->getRows())
    {
        if(!control->hidden())
            CHECK_FALSE(gweni_cast<controls::TreeTableRow>(control)->isPlaceholder());
    }

    // answered empty in the call, two ways: a leaf, never expanded
    for(Id id : {Id(2), Id(3)})
    {
        CAPTURE(id);
        scene.settle();
        scene.tree->setExpanded(id, true);
        CHECK_FALSE(scene.tree->isExpanded(id));
        CHECK_FALSE(scene.tree->getNode(id)->hasChildren);
        CHECK_FALSE(scene.tree->getNode(id)->loading);
        scene.frame();
        CHECK(treeRow(scene.tree, scene.tree->itemForNode(id))->getExpander()->hidden());
    }
    CHECK(requests.ids == std::vector<Id>{1, 2, 3});
    CHECK(requests.expanded == std::vector<Id>{1});
    CHECK(requests.collapsed.empty());
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 10, 11, 2, 3});
    // a leaf now: no expand and no new request
    scene.tree->setExpanded(2, true);
    CHECK(requests.ids == std::vector<Id>{1, 2, 3});

    // answered empty later: the placeholder and the expander go, it collapses
    requests.answer=nullptr;
    scene.tree->setExpanded(0, true);
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, P, 1, 10, 11, 2, 3});
    scene.settle();
    REQUIRE(scene.tree->insertNodes(0, 0, {}));
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 10, 11, 2, 3});
    CHECK_FALSE(scene.tree->isExpanded(0));
    CHECK_FALSE(scene.tree->getNode(0)->hasChildren);
    CHECK(requests.collapsed == std::vector<Id>{0});
    scene.frame();
    CHECK(treeRow(scene.tree, 0)->getExpander()->hidden());
    for(controls::Base *control : scene.tree->getVirtualBody()->getRows())
    {
        if(!control->hidden())
            CHECK_FALSE(gweni_cast<controls::TreeTableRow>(control)->isPlaceholder());
    }
}

TEST_CASE("TreeTable model: resetChildren drops the children and the next expand asks again")
{
    Scene scene;
    Requests requests;
    Selections selections;
    requests.listen(scene.tree);
    scene.tree->onSelectionChange.add(&selections, &Selections::changed);
    scene.roots(3);
    scene.tree->setHasChildren(1, true);
    requests.answer=[&](Id id)
        {
            REQUIRE(scene.tree->insertNodes(id, 0, {100+requests.ids.size(), 200+requests.ids.size()}));
        };
    scene.tree->setExpanded(1, true);
    REQUIRE(scene.tree->addNode(101, 1010) == 1010);
    scene.tree->setExpanded(101, true);
    REQUIRE(scene.tree->setSelectedNode(1010));
    scene.settle();
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 101, 1010, 201, 2});
    const size_t nodes=scene.tree->getNodeCount();

    scene.tree->resetChildren(1);
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 2});
    CHECK(scene.tree->getNodeCount() == nodes-3);
    CHECK(scene.tree->getNode(1010) == nullptr);
    CHECK(scene.tree->getNode(201) == nullptr);
    CHECK_FALSE(scene.tree->isExpanded(1));
    CHECK(scene.tree->getNode(1)->hasChildren);
    CHECK_FALSE(scene.tree->getNode(1)->childrenRequested);
    CHECK(scene.tree->getSelectedNode() == 1);
    CHECK(selections.ids == std::vector<Id>{1});
    CHECK(requests.collapsed == std::vector<Id>{1});
    scene.frame();
    CHECK_FALSE(treeRow(scene.tree, 1)->getExpander()->hidden());
    CHECK(treeRow(scene.tree, 1)->isSelected());

    scene.tree->setExpanded(1, true);
    CHECK(requests.ids == std::vector<Id>{1, 1});
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 102, 202, 2});
    // reset while it waits for an answer: the placeholder goes too
    requests.answer=nullptr;
    scene.tree->resetChildren(1);
    scene.tree->setExpanded(1, true);
    CHECK(requests.ids == std::vector<Id>{1, 1, 1});
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, P, 2});
    scene.tree->resetChildren(1);
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 2});
    CHECK(selections.ids == std::vector<Id>{1});
}

TEST_CASE("TreeTable model: keys pass over a placeholder, and hidden or bulk lazy expands ask once")
{
    Scene scene;
    Requests requests;
    Selections selections;
    requests.listen(scene.tree);
    scene.tree->onSelectionChange.add(&selections, &Selections::changed);
    scene.roots(3);
    scene.tree->setHasChildren(1, true);
    scene.tree->setExpanded(1, true);
    scene.settle();
    REQUIRE(visibleIds(scene.tree) == std::vector<Id>{0, 1, P, 2});

    KeyboardFocus=scene.tree;
    REQUIRE(scene.tree->setSelectedNode(1));
    scene.tree->onKeyDown(true);
    CHECK(scene.tree->getSelectedNode() == 2);
    CHECK(scene.tree->getSelectedItem() == 3);
    scene.tree->onKeyUp(true);
    CHECK(scene.tree->getSelectedNode() == 1);
    CHECK(scene.tree->getSelectedItem() == 1);
    CHECK(selections.ids == std::vector<Id>{2, 1});
    // a placeholder last: End lands on its loading node
    REQUIRE(scene.tree->addNode(Root, 3, true) == 3);
    scene.tree->setExpanded(3, true);
    REQUIRE(visibleIds(scene.tree) == std::vector<Id>{0, 1, P, 2, 3, P});
    scene.tree->onKeyend(true);
    CHECK(scene.tree->getSelectedNode() == 3);
    CHECK(scene.tree->getSelectedItem() == 4);
    CHECK(selections.ids == std::vector<Id>{2, 1, 3});
    KeyboardFocus=nullptr;

    // a loading node under a collapsed ancestor takes its answer in the model
    REQUIRE(scene.tree->addNode(0, 30, true) == 30);
    scene.tree->setExpanded(0, true);
    scene.tree->setExpanded(30, true);
    scene.tree->setExpanded(0, false);
    CHECK(requests.ids == std::vector<Id>{1, 3, 30});
    REQUIRE(scene.tree->insertNodes(30, 0, {300}));
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, P, 2, 3, P});
    scene.tree->setExpanded(0, true);
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 30, 300, 1, P, 2, 3, P});

    // expandAll and expandSubtree show the placeholders, then ask once each
    scene.tree->collapseAll();
    REQUIRE(scene.tree->addNode(2, 40, true) == 40);
    REQUIRE(scene.tree->addNode(2, 41, true) == 41);
    scene.tree->expandSubtree(2);
    CHECK(requests.ids == std::vector<Id>{1, 3, 30, 40, 41});
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 1, 2, 40, P, 41, P, 3});
    REQUIRE(scene.tree->addNode(Root, 50, true) == 50);
    scene.tree->collapseAll();
    scene.tree->expandAll();
    CHECK(requests.ids == std::vector<Id>{1, 3, 30, 40, 41, 50});
    CHECK(visibleIds(scene.tree) == std::vector<Id>{0, 30, 300, 1, P, 2, 40, P, 41, P, 3, P, 50, P});
    scene.frame();
    for(size_t item=0; item < scene.tree->getItemCount(); ++item)
    {
        if(controls::TreeTableRow *row=treeRow(scene.tree, item))
            CHECK(row->isPlaceholder() == (scene.tree->nodeForItem(item) == P));
    }
}

namespace
{

//! The app's names and the comparisons the table asks for.
struct Names
{
    std::map<Id, String> names;
    size_t compares=0;
    std::set<Id> compared;

    Tree::Comparator less()
    {
        return [this](Id a, Id b)
            {
                ++compares;
                compared.insert(a);
                compared.insert(b);
                return names.at(a) < names.at(b);
            };
    }
};

//! An independent sorted rebuild: every expanded group of the app's order
//! stable-sorted by name, folders first if asked, reversed when descending.
std::vector<Id> sortedPreorder(const Model &model, const std::map<Id, String> &names,
    bool descending, bool foldersFirst)
{
    const auto shownLess=[&](Id a, Id b)
        {
            if(foldersFirst)
            {
                const bool folderA=!model.nodes.at(a).children.empty();
                const bool folderB=!model.nodes.at(b).children.empty();
                if(folderA != folderB)
                    return folderA;
            }
            return descending?names.at(b) < names.at(a):names.at(a) < names.at(b);
        };
    const auto sorted=[&](std::vector<Id> group)
        {
            std::stable_sort(group.begin(), group.end(), shownLess);
            return group;
        };
    std::vector<Id> visible;
    std::vector<Id> pending;
    const auto roots=sorted(model.roots);
    pending.assign(roots.rbegin(), roots.rend());
    while(!pending.empty())
    {
        const Id id=pending.back();
        pending.pop_back();
        visible.push_back(id);
        const auto &node=model.nodes.at(id);
        if(node.expanded)
        {
            const auto children=sorted(node.children);
            pending.insert(pending.end(), children.rbegin(), children.rend());
        }
    }
    return visible;
}

//! The table against the rebuild: the shown order, every depth, and the
//! app's order (`children`) left as the app gave it.
void checkSorted(Tree *tree, const Model &model, const std::map<Id, String> &names,
    bool descending, bool foldersFirst)
{
    const std::vector<Id> expected=sortedPreorder(model, names, descending, foldersFirst);
    CHECK(visibleIds(tree) == expected);
    REQUIRE(tree->getItemCount() == expected.size());
    for(size_t item=0; item < expected.size(); ++item)
    {
        const auto *node=tree->getNode(expected[item]);
        REQUIRE(node != nullptr);
        CHECK(tree->getVisibleNodes()[item].depth == node->depth);
        CHECK(node->children == model.nodes.at(expected[item]).children);
    }
    CHECK(tree->getNodeCount() == model.nodes.size());
}

struct SortEvents: event::Handler
{
    Tree *tree=nullptr;
    std::vector<Point> points;
    //! The shown order as the handler sees it.
    std::vector<std::vector<Id>> orders;
    void changed(event::Info info)
    {
        points.push_back(info.point);
        orders.push_back(visibleIds(tree));
    }
};

//! A whole left click on the header cell's centre, moved a unit right per
//! click so two clicks are no double-click.
void clickHeader(Scene &scene, int column, int nth)
{
    controls::Base *cell=scene.tree->getHeader()->getCells().at(column);
    const Point at=cell->localPosToCanvas();
    scene.canvas.inputMouseMoved(at.x+cell->getWidth()/2+nth, at.y+cell->getHeight()/2, 0, 0);
    scene.canvas.inputMouseButton(0, true);
    scene.canvas.inputMouseButton(0, false);
    scene.frame();
}

//! A small tree with equal names (stability), folders and leaves mixed,
//! one folder expanded and one collapsed.
void buildNamedTree(Scene &scene, Model &model, Names &names)
{
    const std::vector<std::pair<Id, String>> roots{
        {1, "delta"}, {2, "alpha"}, {3, "charlie"}, {4, "alpha"}, {5, "bravo"}, {6, "echo"}};
    for(const auto &root : roots)
    {
        REQUIRE(scene.tree->addNode(Root, root.first) == root.first);
        model.insert(Root, model.roots.size(), {root.first});
        names.names[root.first]=root.second;
        scene.names[root.first]=root.second;
    }
    const std::vector<std::tuple<Id, Id, String>> children{
        {3, 30, "zulu"}, {3, 31, "kilo"}, {3, 32, "kilo"}, {3, 33, "able"},
        {5, 50, "yankee"}, {5, 51, "baker"}, {30, 300, "two"}, {30, 301, "one"}};
    for(const auto &child : children)
    {
        const Id parent=std::get<0>(child), id=std::get<1>(child);
        REQUIRE(scene.tree->addNode(parent, id) == id);
        model.insert(parent, model.children(parent).size(), {id});
        names.names[id]=std::get<2>(child);
        scene.names[id]=std::get<2>(child);
    }
    scene.tree->setExpanded(3, true);
    model.nodes.at(3).expanded=true;
    scene.tree->setExpanded(30, true);
    model.nodes.at(30).expanded=true;
}

}//namespace

TEST_CASE("TreeTable model: a header click sorts each sibling group ascending then descending, stably, folders first")
{
    Scene scene;
    Model model;
    Names names;
    SortEvents events;
    events.tree=scene.tree;
    scene.tree->onSortChanged.add(&events, &SortEvents::changed);
    buildNamedTree(scene, model, names);
    scene.settle();
    const std::vector<Id> appOrder=visibleIds(scene.tree);

    // no comparator: the click only tells the app, the order is the app's
    clickHeader(scene, 1, 0);
    CHECK(events.points.size() == 1);
    CHECK_FALSE(scene.tree->isSorted());
    CHECK(visibleIds(scene.tree) == appOrder);

    scene.tree->setSortComparator(0, names.less());
    clickHeader(scene, 0, 1);
    CHECK(scene.tree->isSorted());
    CHECK(scene.tree->getSortDirection() == controls::SortDirection::Ascending);
    checkSorted(scene.tree, model, names.names, false, false);
    // the app's handler sees the order already sorted
    REQUIRE(events.orders.size() == 2);
    CHECK(events.orders.back() == visibleIds(scene.tree));
    // stable: the two "alpha" roots and the two "kilo" leaves in app order
    CHECK(visibleIds(scene.tree) == std::vector<Id>{2, 4, 5, 3, 33, 31, 32, 30, 301, 300, 1, 6});

    clickHeader(scene, 0, 2);
    CHECK(scene.tree->getSortDirection() == controls::SortDirection::Descending);
    checkSorted(scene.tree, model, names.names, true, false);
    CHECK(visibleIds(scene.tree) == std::vector<Id>{6, 1, 3, 30, 300, 301, 31, 32, 33, 5, 2, 4});

    // folders first, in both directions
    scene.tree->setFoldersFirst(true);
    checkSorted(scene.tree, model, names.names, true, true);
    CHECK(visibleIds(scene.tree) == std::vector<Id>{3, 30, 300, 301, 31, 32, 33, 5, 6, 1, 2, 4});
    clickHeader(scene, 0, 3);
    checkSorted(scene.tree, model, names.names, false, true);

    // no sort column: the app's order back
    scene.tree->setSortColumn(-1, controls::SortDirection::None);
    CHECK(visibleIds(scene.tree) == appOrder);
    scene.frame();
    scene.frame();
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
}

TEST_CASE("TreeTable model: a sort keeps expansion, selection and the first shown node and sorts collapsed groups on expand")
{
    Scene scene;
    Names names;
    Selections selections;
    scene.tree->onSelectionChange.add(&selections, &Selections::changed);
    // 300 roots whose names reverse their order, root 0 a collapsed folder
    // of 40 and root 1 an expanded one of 3
    for(Id id=0; id < 300; ++id)
    {
        REQUIRE(scene.tree->addNode(Root, id) == id);
        char name[16];
        std::snprintf(name, sizeof(name), "r%03u", unsigned(299-id));
        names.names[id]=name;
    }
    for(Id id=1000; id < 1040; ++id)
    {
        REQUIRE(scene.tree->addNode(0, id) == id);
        char name[16];
        std::snprintf(name, sizeof(name), "c%04u", unsigned(2000-id));
        names.names[id]=name;
    }
    for(Id id=1100; id < 1103; ++id)
    {
        REQUIRE(scene.tree->addNode(1, id) == id);
        names.names[id]="e"+std::to_string(2000-id);
    }
    scene.tree->setExpanded(1, true);
    controls::VirtualList *body=scene.tree->getVirtualBody();
    body->setVerticalScrollOffset(100*20+7);
    scene.settle();
    const Id anchor=scene.top();
    REQUIRE(scene.tree->setSelectedNode(anchor+2));
    scene.settle();
    const auto pool=body->getRows();
    const auto controls=controlsIn(scene.tree);
    const auto serial=scene.canvas.treeSerial();

    scene.tree->setSortComparator(0, names.less());
    scene.tree->setSortColumn(0, controls::SortDirection::Ascending);
    scene.frame();
    // the anchored node is still the first shown, at the same offset into it
    CHECK(scene.top() == anchor);
    CHECK(body->getVerticalScrollOffset()%20 == 7);
    CHECK(scene.tree->getSelectedNode() == anchor+2);
    CHECK(selections.ids.empty());
    CHECK(scene.tree->isExpanded(1));
    CHECK_FALSE(scene.tree->isExpanded(0));
    // the collapsed folder's children were never compared
    for(Id id=1000; id < 1040; ++id)
        CHECK(names.compared.count(id) == 0);
    // the shown order: the reversed roots, folder 1's children sorted
    const auto &visible=scene.tree->getVisibleNodes();
    REQUIRE(visible.size() == 303);
    CHECK(visible.front().id == 299);
    CHECK(scene.tree->nodeForItem(scene.tree->itemForNode(1)+1) == 1102);
    CHECK(scene.binds.size() <= pool.size());
    CHECK(body->getRows() == pool);
    CHECK(controlsIn(scene.tree) == controls);
    CHECK(scene.canvas.treeSerial() == serial);
    CHECK(scene.canvas.getLayoutStats().passes <= 1);
    scene.settle();

    // expanding the collapsed folder sorts its group then
    names.compared.clear();
    scene.tree->setExpanded(0, true);
    CHECK(names.compared.count(1000) == 1);
    const size_t folder=scene.tree->itemForNode(0);
    for(size_t i=0; i < 40; ++i)
        CHECK(scene.tree->nodeForItem(folder+1+i) == Id(1039-i));
    CHECK(scene.tree->getNode(0)->children.front() == 1000);
    CHECK(controlsIn(scene.tree) == controls);
}

TEST_CASE("TreeTable model: inserts, moves, renames and answers while sorted go to their sorted place")
{
    Scene scene;
    Model model;
    Names names;
    Selections selections;
    Requests requests;
    requests.listen(scene.tree);
    scene.tree->onSelectionChange.add(&selections, &Selections::changed);
    buildNamedTree(scene, model, names);
    scene.tree->setSortComparator(0, names.less());
    scene.tree->setFoldersFirst(true);
    scene.tree->setSortColumn(0, controls::SortDirection::Ascending);
    scene.settle();
    checkSorted(scene.tree, model, names.names, false, true);
    const auto controls=controlsIn(scene.tree);
    auto name=[&](Id id, const String &text) { names.names[id]=text; scene.names[id]=text; };

    // a single insert at the app's front, a batch spread over the group
    name(34, "mike");
    REQUIRE(scene.tree->insertNode(3, 0, 34) == 34);
    model.insert(3, 0, {34});
    checkSorted(scene.tree, model, names.names, false, true);
    name(35, "aaa"); name(36, "kilo"); name(37, "zzz");
    REQUIRE(scene.tree->insertNodes(3, 2, {35, 36, 37}));
    model.insert(3, 2, {35, 36, 37});
    checkSorted(scene.tree, model, names.names, false, true);
    // equal names keep the app's order: 36 is before 31 and 32 in it
    const auto shown=scene.tree->getShownChildren(3);
    const auto at=[&](Id id) { return std::find(shown.begin(), shown.end(), id)-shown.begin(); };
    CHECK(at(36) < at(31));
    CHECK(at(31) < at(32));
    // and one added after them in the app's order is shown after them
    name(38, "kilo");
    REQUIRE(scene.tree->insertNode(3, 99, 38) == 38);
    model.insert(3, 99, {38});
    checkSorted(scene.tree, model, names.names, false, true);

    // a root insert, and a leaf that becomes a folder moves before the leaves
    name(7, "foxtrot");
    REQUIRE(scene.tree->insertNode(Root, 1, 7) == 7);
    model.insert(Root, 1, {7});
    checkSorted(scene.tree, model, names.names, false, true);
    name(60, "x");
    REQUIRE(scene.tree->insertNode(6, 0, 60) == 60);
    model.insert(6, 0, {60});
    checkSorted(scene.tree, model, names.names, false, true);
    // "echo" is a folder now, before the leaf "delta"
    const auto roots=scene.tree->getShownChildren(Root);
    CHECK(std::find(roots.begin(), roots.end(), 6) < std::find(roots.begin(), roots.end(), 1));

    // a rename re-sorts its node with its visible subtree
    REQUIRE(scene.tree->setSelectedNode(30));
    name(3, "zzz");
    scene.tree->nodeChanged(3);
    checkSorted(scene.tree, model, names.names, false, true);
    CHECK(scene.tree->getSelectedNode() == 30);
    CHECK(scene.tree->isExpanded(30));

    // a move into another group goes to its sorted place there
    REQUIRE(scene.tree->moveNode(31, 5, 0));
    model.move(31, 5, 0);
    checkSorted(scene.tree, model, names.names, false, true);
    REQUIRE(scene.tree->moveNode(51, Root, 99));
    model.move(51, Root, 99);
    checkSorted(scene.tree, model, names.names, false, true);

    // a removed selection takes its next shown sibling
    selections.ids.clear();
    const auto group=scene.tree->getShownChildren(30);
    REQUIRE(group.size() == 2);
    REQUIRE(scene.tree->setSelectedNode(group.front()));
    REQUIRE(scene.tree->removeNode(group.front()));
    model.remove(group.front());
    CHECK(scene.tree->getSelectedNode() == group.back());
    CHECK(selections.ids == std::vector<Id>{group.back()});
    checkSorted(scene.tree, model, names.names, false, true);

    // a lazy answer under a sort replaces the placeholder sorted
    name(8, "golf");
    REQUIRE(scene.tree->addNode(Root, 8, true) == 8);
    scene.tree->setExpanded(8, true);
    CHECK(requests.ids == std::vector<Id>{8});
    name(80, "c"); name(81, "a"); name(82, "b");
    REQUIRE(scene.tree->insertNodes(8, 0, {80, 81, 82}));
    model.insert(Root, model.roots.size(), {8});
    model.insert(8, 0, {80, 81, 82});
    model.nodes.at(8).expanded=true;
    checkSorted(scene.tree, model, names.names, false, true);
    for(controls::Base *control : scene.tree->getVirtualBody()->getRows())
    {
        if(!control->hidden())
            CHECK_FALSE(gweni_cast<controls::TreeTableRow>(control)->isPlaceholder());
    }

    // descending and back, after all the edits
    scene.tree->setSortColumn(0, controls::SortDirection::Descending);
    checkSorted(scene.tree, model, names.names, true, true);
    scene.tree->setSortColumn(-1, controls::SortDirection::None);
    model.check(scene.tree);
    scene.frame();
    CHECK(controlsIn(scene.tree) == controls);
}

TEST_CASE("TreeTable model: sorted edits bind locally with the same pool controls and ids at 1000 and 100000 nodes")
{
    std::vector<std::vector<size_t>> costs;
    for(size_t count : {1000, 100000})
    {
        CAPTURE(count);
        const auto baseline=test::IdBankCounts::now();
        {
            Scene scene;
            Tree *tree=scene.tree;
            // roots named by their id, so the sort keeps their order and the
            // rows on screen are the same nodes at both counts
            for(Id id=0; id < count; ++id)
                REQUIRE(tree->addNode(Root, id) == id);
            // an added node (id*1000000+1) sorts right after node id
            tree->setSortComparator(0, [](Id a, Id b)
                {
                    const auto key=[](Id x) { return x > 1000000?x/1000000:x; };
                    return key(a) < key(b) || (key(a) == key(b) && a < b);
                });
            tree->setSortColumn(0, controls::SortDirection::Ascending);
            controls::VirtualList *body=tree->getVirtualBody();
            body->setVerticalScrollOffset(50*20+7);
            scene.settle();
            const auto pool=body->getRows();
            const auto controls=controlsIn(tree);
            const Id anchor=scene.top();
            const auto bank=test::IdBankCounts::now();
            std::vector<size_t> cost{pool.size(), controls.size(),
                bank.livePrimitives()-baseline.livePrimitives(),
                bank.liveTexts()-baseline.liveTexts()};
            // a node sorting above, inside and below the viewport, inserted
            // at the app's end and removed
            for(Id id : {Id(10), Id(55), Id(500)})
            {
                const Id added=id*1000000+1;
                REQUIRE(tree->insertNode(Root, count, added) == added);
                scene.frame();
                cost.push_back(scene.binds.size());
                cost.push_back(controls::getChangeStats().controls);
                cost.push_back(scene.renderer.frameStats().primitiveSlotsWritten);
                // off screen it binds nothing
                if(id != 55)
                    CHECK(scene.binds.empty());
                CHECK(scene.top() == anchor);
                CHECK(tree->nodeForItem(tree->itemForNode(id)+1) == added);
                scene.binds.clear();
                REQUIRE(tree->removeNode(added));
                scene.frame();
                cost.push_back(scene.binds.size());
                cost.push_back(controls::getChangeStats().controls);
                CHECK(scene.top() == anchor);
                CHECK(body->getRows() == pool);
                CHECK(controlsIn(tree) == controls);
                scene.binds.clear();
            }
            costs.push_back(cost);
        }
        const auto after=test::IdBankCounts::now();
        CHECK(after.livePrimitives() == baseline.livePrimitives());
        CHECK(after.liveTexts() == baseline.liveTexts());
    }
    REQUIRE(costs.size() == 2);
    CHECK(costs[0] == costs[1]);
}

TEST_CASE("TreeTable model: Left and Right with a selection hidden by a move stay on it")
{
    Scene scene;
    Tree *tree=scene.tree;
    for(Id id : {Id(1), Id(2)})
        REQUIRE(tree->addNode(Root, id) == id);
    REQUIRE(tree->addNode(1, 10) == 10);
    REQUIRE(tree->addNode(10, 100) == 100);
    REQUIRE(tree->addNode(1, 11) == 11);
    REQUIRE(tree->addNode(2, 20) == 20);
    tree->setExpanded(1, true);
    tree->setExpanded(10, true);
    scene.settle();
    tree->focus();

    // an expanded parent (Right would go to its first child's row) and a
    // leaf with a parent (Left would scan up from its row), each moved under
    // the collapsed node 2: hidden, NoItem
    for(const Id id : {Id(10), Id(11)})
    {
        CAPTURE(id);
        REQUIRE(tree->setSelectedNode(id));
        REQUIRE(tree->moveNode(id, 2, 0));
        scene.frame();
        REQUIRE(tree->getSelectedNode() == id);
        REQUIRE(tree->getSelectedItem() == Tree::NoItem);
        CHECK(tree->onKeyRight(true));
        CHECK(tree->getSelectedNode() == id);
        if(id == 11)
        {
            CHECK(tree->onKeyLeft(true));
            CHECK(tree->getSelectedNode() == id);
        }
        scene.frame();
    }
}

TEST_CASE("TreeTable model: a move under a loading node is its answer and matches a rebuild")
{
    for(const bool sorted : {false, true})
    {
        CAPTURE(sorted);
        Scene scene;
        Tree *tree=scene.tree;
        REQUIRE(tree->addNode(Root, 1, true) == 1);
        REQUIRE(tree->addNode(Root, 2) == 2);
        REQUIRE(tree->addNode(Root, 3) == 3);
        if(sorted)
        {
            tree->setSortComparator(0, [](Id a, Id b) { return a < b; });
            tree->setSortColumn(0, controls::SortDirection::Ascending);
        }
        tree->setExpanded(1, true);
        scene.settle();
        REQUIRE(scene.tree->getVisibleNodes().size() == 4);
        REQUIRE(tree->nodeForItem(1) == Tree::NoNode);

        REQUIRE(tree->moveNode(3, 1, 0));
        scene.frame();
        auto ids=[&]()
            {
                std::vector<Id> result;
                for(const auto &node : tree->getVisibleNodes())
                    result.push_back(node.id);
                return result;
            };
        // the moved node takes the placeholder's place
        CHECK(ids() == std::vector<Id>{1, 3, 2});
        CHECK_FALSE(tree->getNode(1)->loading);

        // an insert whose app index and sorted place disagree, before any
        // rebuild: sorted, it goes after 3 (the group the move answered is
        // sorted at once, not left stale)
        REQUIRE(tree->insertNode(1, 0, 50) == 50);
        scene.frame();
        const std::vector<Id> withInsert=sorted?std::vector<Id>{1, 3, 50, 2}
            :std::vector<Id>{1, 50, 3, 2};
        CHECK(ids() == withInsert);
        tree->collapseAll();
        tree->expandAll();
        scene.frame();
        CHECK(ids() == withInsert);

        // a later answer adds to it
        REQUIRE(tree->insertNodes(1, 99, {100, 101}));
        scene.frame();
        const std::vector<Id> answered=sorted?std::vector<Id>{1, 3, 50, 100, 101, 2}
            :std::vector<Id>{1, 50, 3, 100, 101, 2};
        CHECK(ids() == answered);
        tree->collapseAll();
        tree->expandAll();
        scene.frame();
        CHECK(ids() == answered);
    }
}

TEST_CASE("TreeTable model: sorted siblings renamed before their nodeChanged end in sorted order")
{
    struct Case
    {
        std::vector<double> keys;
        std::vector<std::pair<Id, double>> renames;
        std::vector<Id> expected;
    };
    // the second is the dashboard's case: several keys change, then each
    // node is told of, the first nodeChanged seeing another out of place
    const std::vector<Case> cases{
        {{1, 2, 5, 6, 7, 6.5, 3, 4}, {{3, 0}, {0, 3.5}, {6, 9}}, {3, 1, 0, 7, 2, 5, 4, 6}},
        {{1, 2, 5, 6, 7, 6.5}, {{0, 3}, {3, 0}}, {3, 1, 0, 2, 5, 4}}};
    for(const Case &test : cases)
    {
        CAPTURE(test.expected.size());
        Scene scene;
        Tree *tree=scene.tree;
        std::map<Id, double> key;
        for(Id id=0; id < test.keys.size(); ++id)
        {
            REQUIRE(tree->addNode(Root, id) == id);
            key[id]=test.keys[id];
        }
        tree->setSortComparator(0, [&key](Id a, Id b) { return key.at(a) < key.at(b); });
        tree->setSortColumn(0, controls::SortDirection::Ascending);
        scene.settle();
        for(const auto &rename : test.renames)
            key[rename.first]=rename.second;
        for(const auto &rename : test.renames)
            tree->nodeChanged(rename.first);
        scene.frame();
        std::vector<Id> shown;
        for(const auto &node : tree->getVisibleNodes())
            shown.push_back(node.id);
        CHECK(shown == test.expected);
        CHECK(tree->getShownChildren(Root) == test.expected);
        // the app's order untouched
        tree->setSortColumn(-1, controls::SortDirection::None);
        std::vector<Id> app;
        for(Id id=0; id < test.keys.size(); ++id)
            app.push_back(id);
        CHECK(tree->getShownChildren(Root) == app);
    }
}

TEST_CASE("TreeTable model: a lazy answer whose placeholder is the top row shows its children there")
{
    Scene scene;
    Tree *tree=scene.tree;
    for(Id id=0; id < 40; ++id)
        REQUIRE(tree->addNode(Root, id, id == 5) == id);
    tree->setExpanded(5, true);
    scene.settle();
    REQUIRE(tree->nodeForItem(6) == Tree::NoNode);
    tree->getVirtualBody()->setVerticalScrollOffset(6*20);
    scene.settle();
    REQUIRE(tree->getVirtualBody()->firstVisibleItem() == 6);

    REQUIRE(tree->insertNodes(5, 0, {100, 101, 102}));
    scene.frame();
    CHECK(tree->getVirtualBody()->firstVisibleItem() == 6);
    CHECK(scene.top() == 100);
}

TEST_CASE("TreeTable model: expandSubtree keeps a first shown node inside the subtree at the top")
{
    Scene scene;
    Tree *tree=scene.tree;
    REQUIRE(tree->addNode(Root, 1) == 1);
    for(Id i=0; i < 30; ++i)
    {
        REQUIRE(tree->addNode(1, 100+i) == 100+i);
        REQUIRE(tree->addNode(100+i, 1000+i) == 1000+i);
    }
    REQUIRE(tree->addNode(Root, 2) == 2);
    tree->setExpanded(1, true);
    scene.settle();
    tree->getVirtualBody()->setVerticalScrollOffset(5*20+7);
    scene.settle();
    const Id before=scene.top();
    REQUIRE(before == 104);

    tree->expandSubtree(1);
    scene.frame();
    CHECK(scene.top() == before);
    CHECK(tree->getVirtualBody()->getVerticalScrollOffset()%20 == 7);
}

TEST_CASE("TreeTable model: the stripes on screen survive a sort and then an insert above the viewport")
{
    for(const int startTop : {37, 40, 41})
    {
        CAPTURE(startTop);
        Scene scene;
        Tree *tree=scene.tree;
        tree->setStriped(true);
        for(Id id=0; id < 100; ++id)
        {
            REQUIRE(tree->addNode(Root, id) == id);
            scene.names[id]=String(1, char('a'+(id*7)%26))+std::to_string(1000+id);
        }
        scene.settle();
        tree->getVirtualBody()->setVerticalScrollOffset(startTop*20);
        scene.settle();
        tree->setSortComparator(0, [&scene](Id a, Id b) { return scene.names.at(a) < scene.names.at(b); });
        tree->setSortColumn(0, controls::SortDirection::Ascending);
        scene.settle();

        std::map<controls::DataTableRow *, bool> stripes;
        for(controls::Base *control : tree->getVirtualBody()->getRows())
        {
            auto *row=gweni_cast<controls::DataTableRow>(control);
            if(!row->hidden())
                stripes[row]=row->isStriped();
        }
        REQUIRE_FALSE(stripes.empty());
        // sorted to the very front: one row above the viewport
        scene.names[5000]="!first";
        REQUIRE(tree->insertNode(Root, 0, 5000) == 5000);
        scene.frame();
        REQUIRE(tree->itemForNode(5000) == 0);
        int flipped=0;
        for(const auto &entry : stripes)
            flipped+=entry.first->isStriped() != entry.second;
        CHECK(flipped == 0);
    }
}

TEST_CASE("TreeTable model: collapseAll with a selection hidden already fires no selection event")
{
    Scene scene;
    Tree *tree=scene.tree;
    Selections selections;
    for(Id id : {Id(1), Id(2)})
        REQUIRE(tree->addNode(Root, id) == id);
    REQUIRE(tree->addNode(1, 10) == 10);
    REQUIRE(tree->addNode(1, 11) == 11);
    REQUIRE(tree->addNode(2, 20) == 20);
    REQUIRE(tree->addNode(20, 200) == 200);
    tree->setExpanded(1, true);
    tree->setExpanded(20, true);
    scene.settle();
    REQUIRE(tree->setSelectedNode(10));
    // 2 is collapsed: 10 hidden under 2 > 20
    REQUIRE(tree->moveNode(10, 20, 0));
    scene.frame();
    tree->onSelectionChange.add(&selections, &Selections::changed);

    tree->collapseAll();
    scene.frame();
    CHECK(selections.ids.empty());
    CHECK(tree->getSelectedNode() == 10);
    // shown again, it is selected on its row
    tree->expandAll();
    scene.frame();
    CHECK(selections.ids.empty());
    CHECK(tree->getSelectedNode() == 10);
    CHECK(tree->getSelectedItem() == tree->itemForNode(10));
    // a shown selection a collapse hides goes to its shown ancestor, once
    tree->collapseAll();
    scene.frame();
    CHECK(selections.ids == std::vector<Id>{2});
    CHECK(tree->getSelectedNode() == 2);
}

TEST_CASE("TreeTable model: an edit's anchor leaves a pending scrollToItem to the next layout")
{
    Scene scene;
    Tree *tree=scene.tree;
    for(Id id=0; id < 100; ++id)
        REQUIRE(tree->addNode(Root, id) == id);
    REQUIRE(tree->addNode(3, 300) == 300);
    scene.settle();
    tree->scrollToItem(80);
    // moveNode restores its anchor
    REQUIRE(tree->moveNode(300, 4, 0));
    scene.frame();
    controls::VirtualList *body=tree->getVirtualBody();
    size_t first=body->firstVisibleItem();
    CHECK(first <= 80);
    CHECK(80 < first+200/20);

    // a move of the anchor's own node, which moves the anchor: the pending
    // scroll still wins
    scene.settle();
    tree->scrollToItem(0);
    scene.settle();
    tree->scrollToItem(80);
    REQUIRE(tree->moveNode(0, Root, 50));
    scene.frame();
    first=body->firstVisibleItem();
    CHECK(first <= 80);
    CHECK(80 < first+200/20);
}

TEST_CASE("TreeTable model: a parent whose last child goes is a collapsed leaf")
{
    for(const bool byMove : {false, true})
    {
        CAPTURE(byMove);
        Scene scene;
        Tree *tree=scene.tree;
        struct Events: event::Handler
        {
            std::vector<Id> collapsed;
            void onCollapsed(event::Info info) { collapsed.push_back(info.integer); }
        } events;
        tree->onCollapsed.add(&events, &Events::onCollapsed);
        for(Id id : {Id(1), Id(2)})
            REQUIRE(tree->addNode(Root, id) == id);
        REQUIRE(tree->addNode(1, 10) == 10);
        tree->setExpanded(1, true);
        scene.settle();
        if(byMove)
            REQUIRE(tree->moveNode(10, 2, 0));
        else
            REQUIRE(tree->removeNode(10));
        scene.frame();
        CHECK_FALSE(tree->isExpanded(1));
        CHECK_FALSE(tree->getNode(1)->hasChildren);
        CHECK(events.collapsed == std::vector<Id>{1});
        // Left on it is a leaf's at the top level: nothing
        REQUIRE(tree->setSelectedNode(1));
        tree->focus();
        tree->onKeyLeft(true);
        CHECK(events.collapsed == std::vector<Id>{1});
        scene.frame();
    }
}

namespace
{
std::vector<Id> shownIds(const Tree *tree)
{
    std::vector<Id> result;

    for(const auto &node : tree->getVisibleNodes())
        result.push_back(node.id);
    return result;
}

//! An independent sorted preorder of the app's hierarchy: each group in the
//! app's order, stably sorted by the table's sort column, direction and
//! folders-first, column 0 by name and column 1 by a key that follows a
//! folder's child count (TreeTableDemo's Size).
struct SortedRebuild
{
    Tree *tree;
    const std::map<Id, String> *names;
    const std::map<Id, double> *bytes;

    double key(Id id) const
    {
        const Tree::Node *node=tree->getNode(id);

        return node->hasChildren?static_cast<double>(node->children.size()):bytes->at(id);
    }

    bool less(int column, Id a, Id b) const
    {
        return column == 0?names->at(a) < names->at(b):key(a) < key(b);
    }

    void append(std::vector<Id> &into, std::vector<Id> group) const
    {
        if(tree->isSorted())
        {
            const int column=tree->getSortColumn();
            const bool descending=tree->getSortDirection() == controls::SortDirection::Descending;
            const bool folders=tree->getFoldersFirst();

            std::stable_sort(group.begin(), group.end(), [&](Id a, Id b)
                {
                    const bool fa=tree->getNode(a)->hasChildren;
                    const bool fb=tree->getNode(b)->hasChildren;

                    if(folders && fa != fb)
                        return fa;
                    return descending?less(column, b, a):less(column, a, b);
                });
        }
        for(Id id : group)
        {
            into.push_back(id);
            if(tree->getNode(id)->expanded)
                append(into, tree->getNode(id)->children);
        }
    }
};
}//namespace

// Folders first by name: one move turns the folder "c" into a leaf and the
// leaf "m" into a folder, two keys of one group changing in one edit.
TEST_CASE("TreeTable model: a sorted move between siblings whose keys both change")
{
    Scene scene;
    Tree *tree=scene.tree;

    scene.names={{1, "c"}, {2, "d"}, {3, "e"}, {4, "m"}, {5, "z"}, {10, "x"}};
    for(Id id=1; id <= 5; ++id)
        REQUIRE(tree->addNode(Root, id) == id);
    REQUIRE(tree->addNode(1, 10) == 10);
    tree->setSortComparator(0, [&scene](Id a, Id b) { return scene.names.at(a) < scene.names.at(b); });
    tree->setFoldersFirst(true);
    tree->setSortColumn(0, controls::SortDirection::Ascending);
    scene.settle();
    REQUIRE(shownIds(tree) == std::vector<Id>{1, 2, 3, 4, 5});

    REQUIRE(tree->moveNode(10, 4, 0));
    scene.frame();
    // "m" (4) is the one folder now, "c" (1) the first leaf by name
    CHECK(tree->getShownChildren(Root) == std::vector<Id>{4, 1, 2, 3, 5});
    CHECK(shownIds(tree) == std::vector<Id>{4, 1, 2, 3, 5});
    // the same as a sort from scratch
    tree->setSortColumn(-1, controls::SortDirection::None);
    tree->setSortColumn(0, controls::SortDirection::Ascending);
    scene.frame();
    CHECK(shownIds(tree) == std::vector<Id>{4, 1, 2, 3, 5});
}

// A key following the child count: a child moved up into the group holding
// its old parent, whose key the move lowers.
TEST_CASE("TreeTable model: a sorted move up into the group holding its old parent")
{
    Scene scene;
    Tree *tree=scene.tree;
    const std::map<Id, double> bytes{{1, 5.5}, {3, 7}, {10, 5.2}, {11, 1}, {12, 1}, {13, 1}, {14, 1}, {15, 1}};

    // the roots: 1 (a leaf of 5.5), 2 (a folder of 6), 3 (a leaf of 7)
    for(Id id=1; id <= 3; ++id)
        REQUIRE(tree->addNode(Root, id) == id);
    for(Id id=10; id <= 15; ++id)
        REQUIRE(tree->addNode(2, id) == id);

    const auto key=[tree, &bytes](Id id)
        {
            const Tree::Node *node=tree->getNode(id);

            return node->hasChildren?static_cast<double>(node->children.size()):bytes.at(id);
        };

    tree->setSortComparator(0, [key](Id a, Id b) { return key(a) < key(b); });
    tree->setSortColumn(0, controls::SortDirection::Ascending);
    scene.settle();
    REQUIRE(shownIds(tree) == std::vector<Id>{1, 2, 3});

    // 10 (5.2) up to the roots: 2 becomes 5
    REQUIRE(tree->moveNode(10, Root, 0));
    scene.frame();
    CHECK(tree->getShownChildren(Root) == std::vector<Id>{2, 10, 1, 3});
    CHECK(shownIds(tree) == std::vector<Id>{2, 10, 1, 3});
}

// Seeded random edits while sorted (inserts, batches, removes, moves,
// renames, expand toggles, sort changes; by name and by a child-count key,
// folders first or not, either direction), each compared with a sorted
// rebuild, ties in the app's order included.
TEST_CASE("TreeTable model: sorted random edits match a sorted rebuild")
{
    for(unsigned seed=1; seed <= 300; ++seed)
    {
        CAPTURE(seed);
        Scene scene;
        Tree *tree=scene.tree;
        std::mt19937 rng(seed);
        std::map<Id, double> bytes;
        std::vector<Id> roots; // the app's order of the roots
        Id next=1;
        const auto existing=[&]()
            {
                std::vector<Id> ids;

                for(const auto &entry : bytes)
                    if(tree->getNode(entry.first))
                        ids.push_back(entry.first);
                return ids;
            };
        const auto add=[&](Id parent, size_t at, size_t count)
            {
                std::vector<Id> ids;

                for(size_t i=0; i < count; ++i)
                {
                    ids.push_back(next);
                    scene.names[next]="n"+std::to_string(rng()%12);
                    bytes[next]=(rng()%12)*0.5;
                    ++next;
                }
                REQUIRE(tree->insertNodes(parent, at, ids));
                if(parent == Root)
                    roots.insert(roots.begin()+std::min(at, roots.size()), ids.begin(), ids.end());
            };

        for(int i=0; i < 12; ++i)
            add(Root, static_cast<size_t>(i), 1);
        for(int i=0; i < 30; ++i)
        {
            const auto ids=existing();

            add(ids[rng()%ids.size()], rng()%4, 1+rng()%2);
        }
        for(Id id : existing())
            if(rng()%2)
                tree->setExpanded(id, true);

        const SortedRebuild rebuild{tree, &scene.names, &bytes};
        const auto direction=[&]()
            {
                return rng()%2?controls::SortDirection::Ascending:controls::SortDirection::Descending;
            };

        tree->setSortComparator(0, [&scene](Id a, Id b) { return scene.names.at(a) < scene.names.at(b); });
        tree->setSortComparator(1, [&rebuild](Id a, Id b) { return rebuild.key(a) < rebuild.key(b); });
        tree->setFoldersFirst(rng()%2 != 0);
        tree->setSortColumn(static_cast<int>(rng()%2), direction());
        scene.settle();

        for(int step=0; step < 120; ++step)
        {
            const auto ids=existing();
            const unsigned op=rng()%10;
            std::string what="frame";

            if(op <= 1)
            {
                const Id parent=rng()%5 == 0?Root:ids[rng()%ids.size()];
                add(parent, rng()%6, 1+rng()%3);
                what="insert under "+std::to_string(parent);
            }
            else if(op == 2 && ids.size() > 10)
            {
                const Id id=ids[rng()%ids.size()];
                const auto root=std::find(roots.begin(), roots.end(), id);

                if(root != roots.end())
                    roots.erase(root);
                REQUIRE(tree->removeNode(id));
                what="remove "+std::to_string(id);
            }
            else if(op <= 4)
            {
                const Id id=ids[rng()%ids.size()];
                const Id parent=rng()%5 == 0?Root:ids[rng()%ids.size()];

                const size_t at=rng()%6;
                const auto root=std::find(roots.begin(), roots.end(), id);

                if(tree->moveNode(id, parent, at))
                {
                    if(root != roots.end())
                        roots.erase(root);
                    if(parent == Root)
                        roots.insert(roots.begin()+std::min(at, roots.size()), id);
                }
                what="move "+std::to_string(id)+" to "+std::to_string(parent);
            }
            else if(op <= 6)
            {
                const Id id=ids[rng()%ids.size()];

                scene.names[id]="n"+std::to_string(rng()%12);
                bytes[id]=(rng()%12)*0.5;
                tree->nodeChanged(id);
                what="rename "+std::to_string(id);
            }
            else if(op == 7)
            {
                const Id id=ids[rng()%ids.size()];

                tree->setExpanded(id, !tree->getNode(id)->expanded);
                what="toggle "+std::to_string(id);
            }
            else if(op == 8 && rng()%4 == 0)
            {
                tree->setSortColumn(static_cast<int>(rng()%2), direction());
                what="sort";
            }
            if(step%3 == 0)
                scene.frame();

            std::vector<Id> expected;

            rebuild.append(expected, roots);
            CAPTURE(step);
            CAPTURE(what);
            REQUIRE(shownIds(tree) == expected);
        }
    }
}


// Batched renames can turn previously different keys into a single tie run.
TEST_CASE("TreeTable model: siblings renamed into a tie keep the app order")
{
    Scene scene;
    Tree *tree=scene.tree;
    scene.names={{1, "k"}, {2, "k"}, {3, "z"}, {4, "a"}};
    for(Id id=1; id <= 4; ++id)
        REQUIRE(tree->addNode(Root, id) == id);
    tree->setSortComparator(0, [&scene](Id a, Id b) { return scene.names.at(a) < scene.names.at(b); });
    tree->setSortColumn(0, controls::SortDirection::Ascending);
    scene.settle();
    REQUIRE(visibleIds(tree) == std::vector<Id>{4, 1, 2, 3});
    scene.names[3]="k";
    scene.names[4]="k";
    tree->nodeChanged(3);
    tree->nodeChanged(4);
    scene.frame();
    CHECK(tree->getShownChildren(Root) == std::vector<Id>{1, 2, 3, 4});
    CHECK(visibleIds(tree) == std::vector<Id>{1, 2, 3, 4});
}

TEST_CASE("TreeTable model: a resort rebinds placeholders whose depth changed")
{
    Scene scene;
    Tree *tree=scene.tree;
    scene.names={{1, "b"}, {2, "a"}, {3, "c"}, {4, "x"}};
    REQUIRE(tree->addNode(Root, 1, true) == 1);
    REQUIRE(tree->addNode(Root, 2) == 2);
    REQUIRE(tree->addNode(Root, 3) == 3);
    REQUIRE(tree->addNode(2, 4, true) == 4);
    for(Id id : {1, 2, 4})
        tree->setExpanded(id, true);
    tree->setSortComparator(0, [&scene](Id a, Id b) { return scene.names.at(a) < scene.names.at(b); });
    tree->setSortColumn(0, controls::SortDirection::Ascending);
    scene.settle();
    REQUIRE(visibleIds(tree) == std::vector<Id>{2, 4, P, 1, P, 3});
    REQUIRE(treeRow(tree, 2)->getDepth() == 2);
    scene.names[2]="z";
    scene.names[3]="0";
    tree->nodeChanged(2);
    tree->nodeChanged(3);
    scene.frame();
    REQUIRE(visibleIds(tree) == std::vector<Id>{3, 1, P, 2, 4, P});
    for(size_t item=0; item < 6; ++item)
    {
        const auto *row=treeRow(tree, item);
        REQUIRE(row != nullptr);
        CAPTURE(item);
        CHECK(row->getDepth() == tree->getVisibleNodes()[item].depth);
    }
}

TEST_CASE("TreeTable model: a lazy answer inside a viewport at the end keeps its top")
{
    for(bool move : {false, true})
    {
        CAPTURE(move);
        Scene scene;
        Tree *tree=scene.tree;
        for(Id id=1; id <= 30; ++id)
            REQUIRE(tree->addNode(Root, id, id == 25) == id);
        if(move)
        {
            REQUIRE(tree->addNode(1, 100) == 100);
            REQUIRE(tree->addNode(100, 101) == 101);
            tree->setExpanded(100, true);
        }
        tree->setExpanded(25, true);
        scene.settle();
        auto *body=tree->getVirtualBody();
        body->setVerticalScrollOffset(100000);
        scene.settle();
        const Id top=scene.top();
        const int offset=body->getVerticalScrollOffset();
        REQUIRE(offset == body->getMaxVerticalScrollOffset());
        REQUIRE(tree->itemForNode(top) < tree->itemForNode(25));
        if(move)
            REQUIRE(tree->moveNode(100, 25, 0));
        else
            REQUIRE(tree->insertNodes(25, 0, {100, 101}));
        scene.frame();
        CHECK(scene.top() == top);
        CHECK(body->getVerticalScrollOffset() == offset);
    }
}

TEST_CASE("TreeTable model: placeholders hide pooled cell contents and restore their visibility")
{
    Scene scene;
    Tree *tree=scene.tree;
    std::map<controls::DataTableRow *, std::pair<controls::CheckBox *, controls::CheckBox *>> contents;
    scene.roots(5);
    tree->setHasChildren(2, true);
    scene.settle();
    // The pool is already made: cell controls are installed outside layout,
    // including rows which have not bound an item yet.
    for(auto *control : tree->getVirtualBody()->getRows())
    {
        auto *row=gweni_cast<controls::DataTableRow>(control);
        REQUIRE(row != nullptr);
        auto *shown=scene.canvas.newChild<controls::CheckBox>();
        auto *hidden=scene.canvas.newChild<controls::CheckBox>();
        row->setCellContents(1, shown);
        row->setCellContents(1, hidden);
        shown->sendToBack(); // app cell contents need not follow the caption
        hidden->setHidden(true);
        contents[row]={shown, hidden};
    }
    scene.settle();
    const auto pool=tree->getVirtualBody()->getRows();
    const auto controls=controlsIn(tree);
    tree->setExpanded(2, true);
    scene.frame();
    auto *placeholder=treeRow(tree, 3);
    REQUIRE(placeholder != nullptr);
    REQUIRE(placeholder->isPlaceholder());
    REQUIRE(contents.count(placeholder));
    const auto held=contents.at(placeholder);
    CHECK_FALSE(placeholder->getCell(0)->getTextControl()->hidden());
    CHECK_FALSE(placeholder->getCell(1)->getTextControl()->hidden());
    CHECK(held.first->hidden());
    CHECK_FALSE(held.first->visible());
    CHECK(held.second->hidden());
    REQUIRE(tree->insertNodes(2, 0, {20, 21}));
    scene.frame();
    CHECK_FALSE(placeholder->isPlaceholder());
    CHECK_FALSE(held.first->hidden());
    CHECK(held.first->visible());
    CHECK(held.second->hidden());
    CHECK(tree->getVirtualBody()->getRows() == pool);
    CHECK(controlsIn(tree) == controls);
}

TEST_CASE("TreeTable model: a widget-only binder clears the loading caption on reuse")
{
    Scene scene;
    Tree *tree=scene.tree;
    // The app fills the cells with controls, leaving Label captions empty.
    tree->setNodeBinder([](controls::DataTableRow *, Id, size_t) {});
    scene.roots(5);
    tree->setHasChildren(2, true);
    scene.settle();
    std::map<controls::DataTableRow *, controls::CheckBox *> contents;
    for(auto *control : tree->getVirtualBody()->getRows())
    {
        auto *row=gweni_cast<controls::DataTableRow>(control);
        REQUIRE(row != nullptr);
        auto *box=scene.canvas.newChild<controls::CheckBox>();
        row->setCellContents(0, box);
        contents[row]=box;
    }
    scene.settle();
    tree->setExpanded(2, true);
    scene.frame();
    auto *placeholder=treeRow(tree, 3);
    REQUIRE(placeholder != nullptr);
    REQUIRE(placeholder->isPlaceholder());
    REQUIRE(placeholder->getCellText(0) == Tree::PlaceholderText);
    REQUIRE(tree->insertNode(2, 0, 20) == 20);
    scene.frame();
    CHECK_FALSE(placeholder->isPlaceholder());
    CHECK(contents.at(placeholder)->visible());
    CHECK(placeholder->getCellText(0) == "");

    // A real node may also be named Loading…: clear before the app binds,
    // so an explicitly supplied caption with that value remains intact.
    tree->setNodeBinder([](controls::DataTableRow *row, Id, size_t)
        {
            row->setCellText(0, Tree::PlaceholderText);
        });
    tree->resetChildren(2);
    tree->setExpanded(2, true);
    scene.frame();
    REQUIRE(tree->insertNode(2, 0, 21) == 21);
    scene.frame();
    CHECK_FALSE(treeRow(tree, 3)->isPlaceholder());
    CHECK(treeRow(tree, 3)->getCellText(0) == Tree::PlaceholderText);
}

namespace
{

constexpr Id NoNode=Tree::NoNode;

// App-side lazy hierarchy and selection, separate from the control store.
struct MNode
{
    Id parent=Root;
    std::vector<Id> children;
    bool expanded=false;
    bool hasChildren=false;
    bool requested=false;
    bool loading=false;
};

struct LiveModelScene: Scene, event::Handler
{
    std::mt19937 rng;

    std::map<Id, MNode> m;
    std::vector<Id> roots;
    std::map<Id, double> bytes;
    Id next=1;
    Id selected=NoNode;
    int selectionEvents=0;
    std::vector<Id> requests;
    std::vector<Id> pending;
    std::string what;
    bool answering=false;

    explicit LiveModelScene(unsigned seed): rng(seed)
    {
        tree->setStriped(true);
        tree->setNodeBinder([this](controls::DataTableRow *row, Id id, size_t)
            {
                row->setCellText(0, names.at(id));
                row->setCellText(1, std::to_string(bytes.at(id)));
            });
        tree->onSelectionChange.add(this, &LiveModelScene::selectionChanged);
        tree->onChildrenRequested.add(this, &LiveModelScene::childrenRequested);
    }

    void selectionChanged(event::Info info)
    {
        ++selectionEvents;
        CHECK(static_cast<Id>(info.integer) == tree->getSelectedNode());
    }

    std::vector<Id> &kids(Id parent) { return parent == Root?roots:m.at(parent).children; }

    bool inSubtree(Id id, Id ancestor) const
    {
        for(Id at=id; at != Root && at != NoNode; at=m.at(at).parent)
            if(at == ancestor)
                return true;
        return false;
    }

    Id fresh()
    {
        const Id id=next++;
        names[id]="n"+std::to_string(rng()%8);
        bytes[id]=(rng()%8)*0.5;
        m[id];
        return id;
    }

    // the model's half of an insert under parent (the answer included)
    void modelInsert(Id parent, size_t at, const std::vector<Id> &ids)
    {
        if(parent != Root)
        {
            MNode &p=m.at(parent);
            if(p.loading)
            {
                p.loading=false;
                if(ids.empty())
                {
                    p.hasChildren=false;
                    p.expanded=false;
                }
            }
        }
        if(ids.empty())
            return;
        auto &siblings=kids(parent);
        siblings.insert(siblings.begin()+std::min(at, siblings.size()), ids.begin(), ids.end());
        for(Id id : ids)
            m.at(id).parent=parent;
        if(parent != Root)
            m.at(parent).hasChildren=true;
    }

    void insert(Id parent, size_t at, size_t count, bool lazy)
    {
        std::vector<Id> ids;
        for(size_t i=0; i < count; ++i)
            ids.push_back(fresh());
        modelInsert(parent, at, ids);
        if(count == 1 && lazy)
        {
            // addNode appends with an advertised expander
            auto &siblings=kids(parent);
            siblings.erase(std::find(siblings.begin(), siblings.end(), ids[0]));
            siblings.push_back(ids[0]);
            m.at(ids[0]).hasChildren=true;
            REQUIRE(tree->addNode(parent, ids[0], true) == ids[0]);
        }
        else if(count == 1)
            REQUIRE(tree->insertNode(parent, at, ids[0]) == ids[0]);
        else
            REQUIRE(tree->insertNodes(parent, at, ids));
    }

    void childrenRequested(event::Info info)
    {
        const Id id=static_cast<Id>(info.integer);
        CAPTURE(id);
        REQUIRE(m.count(id));
        // once per node until resetChildren
        CHECK(std::find(requests.begin(), requests.end(), id) == requests.end());
        requests.push_back(id);
        CHECK(m.at(id).children.empty());
        CHECK(m.at(id).hasChildren);
        CHECK(m.at(id).requested);
        if(answering || rng()%3 != 0)
        {
            pending.push_back(id);
            return;
        }
        answer(id);
    }

    void answer(Id id)
    {
        const unsigned kind=rng()%4;
        if(kind == 0)
        {
            // an empty answer
            modelInsert(id, 0, {});
            REQUIRE(tree->insertNodes(id, 0, {}));
        }
        else if(kind == 1 && m.at(id).children.empty())
        {
            MNode &node=m.at(id);
            if(node.loading)
            {
                node.loading=false;
                node.hasChildren=false;
                node.expanded=false;
            }
            else if(node.hasChildren)
            {
                node.hasChildren=false;
                node.expanded=false;
            }
            tree->setHasChildren(id, false);
        }
        else
            insert(id, rng()%3, 1+rng()%3, false);
    }

    // the model's setExpanded(id, true), with the request in the middle
    void expand(Id id)
    {
        MNode &node=m.at(id);
        if(node.hasChildren && node.children.empty() && !node.requested)
        {
            node.requested=true;
            node.loading=true;
            tree->setExpanded(id, true);
            // the handler has run (and maybe answered); the table carries on
            // unless the node expanded, went or lost its expander
            if(m.count(id) && !m.at(id).expanded && m.at(id).hasChildren)
                m.at(id).expanded=true;
            return;
        }
        if(node.hasChildren)
            node.expanded=true;
        tree->setExpanded(id, true);
    }

    void collapse(Id id)
    {
        MNode &node=m.at(id);
        if(!node.expanded)
        {
            tree->setExpanded(id, false);
            return;
        }
        const bool shown=itemOf(id) != Tree::NoItem;
        bool hasShownBelow=shown && (node.loading || !node.children.empty());
        node.expanded=false;
        if(shown && hasShownBelow && selected != id && selected != NoNode && inSubtree(selected, id))
            selected=id;
        tree->setExpanded(id, false);
    }

    size_t itemOf(Id id) const { return tree->itemForNode(id); }

    void eraseSubtree(Id id)
    {
        std::vector<Id> stack{id};
        while(!stack.empty())
        {
            const Id at=stack.back();
            stack.pop_back();
            stack.insert(stack.end(), m.at(at).children.begin(), m.at(at).children.end());
            m.erase(at);
            pending.erase(std::remove(pending.begin(), pending.end(), at), pending.end());
            requests.erase(std::remove(requests.begin(), requests.end(), at), requests.end());
        }
    }

    void emptied(Id parent)
    {
        if(parent != Root && m.at(parent).children.empty())
        {
            m.at(parent).hasChildren=false;
            m.at(parent).expanded=false;
        }
    }

    bool sorted() const { return tree->isSorted(); }

    double key(Id id) const
    {
        const MNode &node=m.at(id);
        return node.hasChildren?static_cast<double>(node.children.size()):bytes.at(id);
    }

    bool lessIds(Id a, Id b) const
    {
        if(tree->getFoldersFirst())
        {
            const bool fa=m.at(a).hasChildren, fb=m.at(b).hasChildren;
            if(fa != fb)
                return fa;
        }
        const bool desc=tree->getSortDirection() == controls::SortDirection::Descending;
        const int column=tree->getSortColumn();
        const auto less=[&](Id x, Id y) { return column == 0?names.at(x) < names.at(y):key(x) < key(y); };
        return desc?less(b, a):less(a, b);
    }

    std::vector<Id> shownOrder(Id parent) const
    {
        std::vector<Id> group=parent == Root?roots:m.at(parent).children;
        if(sorted())
            std::stable_sort(group.begin(), group.end(), [this](Id a, Id b) { return lessIds(a, b); });
        return group;
    }

    void remove(Id id)
    {
        const Id parent=m.at(id).parent;
        std::vector<Id> allowedFallbacks;
        const auto fallback=[&](const std::vector<Id> &order)
            {
                const size_t index=std::find(order.begin(), order.end(), id)-order.begin();
                return index+1 < order.size()?order[index+1]:index > 0?order[index-1]:parent;
            };

        if(selected != NoNode && inSubtree(selected, id))
        {
            // A shown group has its current sorted order. A hidden group may
            // still use the app order until its next expand; either policy is
            // allowed, but both fallbacks come from this independent model.
            const bool groupShown=parent == Root || (m.at(parent).expanded && itemOf(parent) != Tree::NoItem);
            if(groupShown)
                selected=fallback(shownOrder(parent));
            else
            {
                allowedFallbacks={fallback(kids(parent)), fallback(shownOrder(parent))};
                selected=Id(-2); // read after
            }
        }
        auto &siblings=kids(parent);
        siblings.erase(std::find(siblings.begin(), siblings.end(), id));
        eraseSubtree(id);
        emptied(parent);
        REQUIRE(tree->removeNode(id));
        if(selected == Id(-2))
        {
            selected=tree->getSelectedNode();
            CHECK(std::find(allowedFallbacks.begin(), allowedFallbacks.end(), selected) != allowedFallbacks.end());
        }
    }

    void move(Id id, Id parent, size_t at)
    {
        if(parent != Root && inSubtree(parent, id))
        {
            CHECK_FALSE(tree->moveNode(id, parent, at));
            return;
        }
        const Id from=m.at(id).parent;
        auto &old=kids(from);
        const size_t oldIndex=std::find(old.begin(), old.end(), id)-old.begin();
        const size_t finalSize=kids(parent).size()-(from == parent?1:0);
        const size_t index=std::min(at, finalSize);
        if(!(from == parent && oldIndex == index))
        {
            old.erase(old.begin()+oldIndex);
            emptied(from);
            if(parent != Root)
            {
                MNode &p=m.at(parent);
                p.loading=false;
                p.hasChildren=true;
            }
            auto &dest=kids(parent);
            dest.insert(dest.begin()+index, id);
            m.at(id).parent=parent;
        }
        REQUIRE(tree->moveNode(id, parent, at));
    }

    void reset(Id id)
    {
        if(selected != NoNode && selected != id && inSubtree(selected, id))
            selected=id;
        MNode &node=m.at(id);
        const auto children=node.children;
        for(Id child : children)
            eraseSubtree(child);
        MNode &again=m.at(id);
        again.children.clear();
        again.expanded=false;
        again.loading=false;
        again.requested=false;
        again.hasChildren=true;
        requests.erase(std::remove(requests.begin(), requests.end(), id), requests.end());
        pending.erase(std::remove(pending.begin(), pending.end(), id), pending.end());
        tree->resetChildren(id);
    }

    void setAll(bool expanded)
    {
        const bool wasShown=selected != NoNode && itemOf(selected) != Tree::NoItem;
        for(auto &entry : m)
        {
            MNode &node=entry.second;
            if(node.expanded != expanded && (!expanded || node.hasChildren))
            {
                if(expanded && node.hasChildren && node.children.empty() && !node.requested)
                {
                    node.requested=true;
                    node.loading=true;
                }
                node.expanded=expanded;
            }
        }
        if(wasShown && !expanded)
        {
            // the topmost collapsed ancestor shows in its place
            Id shown=selected;
            for(Id at=m.at(selected).parent; at != Root; at=m.at(at).parent)
                shown=at;
            selected=shown;
        }
        answering=true;
        if(expanded)
            tree->expandAll();
        else
            tree->collapseAll();
        answering=false;
    }

    void expandSubtree(Id id)
    {
        std::vector<Id> stack{id};
        while(!stack.empty())
        {
            const Id at=stack.back();
            stack.pop_back();
            MNode &node=m.at(at);
            if(node.hasChildren && !node.expanded)
            {
                if(node.children.empty() && !node.requested)
                {
                    node.requested=true;
                    node.loading=true;
                }
                node.expanded=true;
            }
            stack.insert(stack.end(), node.children.begin(), node.children.end());
        }
        answering=true;
        tree->expandSubtree(id);
        answering=false;
    }

    struct Row
    {
        Id id;
        size_t depth;
    };

    std::vector<Row> rebuild() const
    {
        std::vector<Row> rows;
        std::vector<std::pair<Id, size_t>> stack;
        const auto push=[&](Id parent, size_t depth)
            {
                const auto order=shownOrder(parent);
                for(auto it=order.rbegin(); it != order.rend(); ++it)
                    stack.emplace_back(*it, depth);
            };
        push(Root, 0);
        while(!stack.empty())
        {
            const auto entry=stack.back();
            stack.pop_back();
            rows.push_back({entry.first, entry.second});
            const MNode &node=m.at(entry.first);
            if(node.expanded && node.loading)
                rows.push_back({NoNode, entry.second+1});
            else if(node.expanded)
                push(entry.first, entry.second+1);
        }
        return rows;
    }

    void check()
    {
        CAPTURE(what);
        // the store
        REQUIRE(tree->getNodeCount() == m.size());
        for(const auto &entry : m)
        {
            CAPTURE(entry.first);
            const Tree::Node *node=tree->getNode(entry.first);
            REQUIRE(node != nullptr);
            CHECK(node->parent == entry.second.parent);
            CHECK(node->children == entry.second.children);
            CHECK(node->expanded == entry.second.expanded);
            CHECK(node->hasChildren == entry.second.hasChildren);
            CHECK(node->loading == entry.second.loading);
            CHECK(node->childrenRequested == entry.second.requested);
        }
        // the projection
        const auto rows=rebuild();
        const auto &visible=tree->getVisibleNodes();
        REQUIRE(visible.size() == rows.size());
        REQUIRE(tree->getItemCount() == rows.size());
        for(size_t i=0; i < rows.size(); ++i)
        {
            CAPTURE(i);
            REQUIRE(visible[i].id == rows[i].id);
            CHECK(visible[i].depth == rows[i].depth);
        }
        // the selection
        CHECK(tree->getSelectedNode() == selected);
        const size_t selItem=selected == NoNode?Tree::NoItem:itemOf(selected);
        CHECK(tree->getSelectedItem() == selItem);
    }

    // after a frame: every pooled row that shows an item shows it right
    void checkRows()
    {
        CAPTURE(what);
        controls::VirtualList *body=tree->getVirtualBody();
        std::map<size_t, controls::TreeTableRow *> shown;
        for(controls::Base *control : body->getRows())
        {
            auto *row=gweni_cast<controls::TreeTableRow>(control);
            REQUIRE(row);
            const size_t item=body->itemForRow(control);
            if(item == Tree::NoItem || row->hidden())
                continue;
            REQUIRE(item < tree->getVisibleNodes().size());
            shown[item]=row;
            const auto &node=tree->getVisibleNodes()[item];
            CAPTURE(item);
            CAPTURE(node.id);
            CHECK(row->getDepth() == node.depth);
            if(node.id == NoNode)
            {
                CHECK(row->isPlaceholder());
                CHECK(row->getCellText(0) == String(Tree::PlaceholderText));
                CHECK(row->getExpander()->hidden());
            }
            else
            {
                CHECK_FALSE(row->isPlaceholder());
                CHECK(row->getCellText(0) == names.at(node.id));
                CHECK(row->getCellText(1) == std::to_string(bytes.at(node.id)));
                CHECK(row->getExpander()->hidden() == !m.at(node.id).hasChildren);
                CHECK(row->getExpander()->isExpanded() == m.at(node.id).expanded);
            }
            CHECK(row->isSelected() == (node.id != NoNode && node.id == selected));
        }
        // stripes alternate down the rows on screen
        for(auto it=shown.begin(); it != shown.end(); ++it)
        {
            auto after=std::next(it);
            if(after != shown.end() && after->first == it->first+1)
                CHECK(after->second->isStriped() != it->second->isStriped());
        }
    }

    Id top() const { return tree->nodeForItem(tree->getVirtualBody()->firstVisibleItem()); }


};

}//namespace

TEST_CASE("TreeTable model: combined lazy sorted selected and scrolled edits match an independent model")
{
    const char *firstEnv=std::getenv("GWENI_TREETABLE_SEED_FIRST");
    const char *countEnv=std::getenv("GWENI_TREETABLE_SEED_COUNT");
    const unsigned first=firstEnv?static_cast<unsigned>(std::stoul(firstEnv)):1;
    const unsigned count=countEnv?static_cast<unsigned>(std::stoul(countEnv)):1000;
    REQUIRE(first > 0);
    REQUIRE(count > 0);
    for(unsigned i=0; i < count; ++i)
    {
        const unsigned seed=first+i;
        CAPTURE(seed);
        LiveModelScene p(seed);
        Tree *tree=p.tree;

        tree->setSortComparator(0, [&p](Id a, Id b) { return p.names.at(a) < p.names.at(b); });
        // the table's own state for a folder's child count, as the demo
        const auto tableKey=[&p](Id id)
            {
                const Tree::Node *node=p.tree->getNode(id);
                return node->hasChildren?static_cast<double>(node->children.size()):p.bytes.at(id);
            };
        tree->setSortComparator(1, [tableKey](Id a, Id b) { return tableKey(a) < tableKey(b); });
        for(int i=0; i < 15; ++i)
            p.insert(Root, i, 1, p.rng()%4 == 0);
        for(int i=0; i < 30; ++i)
        {
            std::vector<Id> ids;
            for(const auto &entry : p.m)
                ids.push_back(entry.first);
            const Id parent=ids[p.rng()%ids.size()];
            if(!p.m.at(parent).loading)
                p.insert(parent, p.rng()%3, 1+p.rng()%2, p.rng()%4 == 0);
        }
        const unsigned sortMode=p.rng()%3;
        tree->setFoldersFirst(p.rng()%2 != 0);
        if(sortMode > 0)
            tree->setSortColumn(static_cast<int>(sortMode-1),
                p.rng()%2?controls::SortDirection::Ascending:controls::SortDirection::Descending);
        p.check();
        p.frame();
        p.frame();

        for(int step=0; step < 150; ++step)
        {
            CAPTURE(step);
            std::vector<Id> ids;
            for(const auto &entry : p.m)
                ids.push_back(entry.first);
            const auto any=[&]() { return ids[p.rng()%ids.size()]; };
            const unsigned op=p.rng()%22;
            const Id topBefore=p.top();
            const int offsetBefore=tree->getVirtualBody()->getVerticalScrollOffset();
            bool anchorKept=true;
            p.selectionEvents=0;
            const Id selBefore=p.selected;
            if(op <= 2)
            {
                const Id parent=p.rng()%6 == 0?Root:any();
                const size_t count=p.rng()%3 == 0?0:1+p.rng()%3;
                p.what="insert "+std::to_string(count)+" under "+std::to_string(parent);
                if(count == 0)
                {
                    p.modelInsert(parent, 0, {});
                    REQUIRE(tree->insertNodes(parent, 0, {}));
                }
                else
                    p.insert(parent, p.rng()%5, count, p.rng()%4 == 0);
            }
            else if(op == 3 && ids.size() > 12)
            {
                const Id id=any();
                p.what="remove "+std::to_string(id);
                anchorKept=!(topBefore != NoNode && (p.inSubtree(topBefore, id)));
                p.remove(id);
            }
            else if(op <= 5)
            {
                const Id id=any();
                const Id parent=p.rng()%5 == 0?Root:any();
                p.what="move "+std::to_string(id)+" to "+std::to_string(parent);
                p.move(id, parent, p.rng()%5);
            }
            else if(op == 6)
            {
                // several renames, then nodeChanged on each
                const size_t count=1+p.rng()%3;
                std::vector<Id> changed;
                for(size_t i=0; i < count; ++i)
                {
                    const Id id=any();
                    p.names[id]="n"+std::to_string(p.rng()%8);
                    p.bytes[id]=(p.rng()%8)*0.5;
                    changed.push_back(id);
                }
                p.what="rename x"+std::to_string(count);
                for(Id id : changed)
                    tree->nodeChanged(id);
            }
            else if(op <= 8)
            {
                const Id id=any();
                p.what="expand "+std::to_string(id);
                p.expand(id);
            }
            else if(op == 9)
            {
                const Id id=any();
                p.what="collapse "+std::to_string(id);
                anchorKept=!(topBefore != NoNode && topBefore != id && p.inSubtree(topBefore, id));
                p.collapse(id);
            }
            else if(op == 10 && !p.pending.empty())
            {
                const size_t at=p.rng()%p.pending.size();
                const Id id=p.pending[at];
                p.pending.erase(p.pending.begin()+at);
                p.what="answer "+std::to_string(id);
                if(p.m.count(id))
                    p.answer(id);
            }
            else if(op == 11)
            {
                const Id id=any();
                p.what="reset "+std::to_string(id);
                anchorKept=!(topBefore != NoNode && topBefore != id && p.inSubtree(topBefore, id));
                p.reset(id);
            }
            else if(op == 12)
            {
                const Id id=any();
                const bool flag=p.rng()%2 != 0;
                p.what="setHasChildren "+std::to_string(id)+" "+std::to_string(flag);
                MNode &node=p.m.at(id);
                if(node.children.empty() && node.hasChildren != flag)
                {
                    if(node.loading)
                    {
                        node.loading=false;
                        node.hasChildren=false;
                        node.expanded=false;
                    }
                    else
                    {
                        node.hasChildren=flag;
                        node.expanded=node.expanded && flag;
                    }
                }
                tree->setHasChildren(id, flag);
            }
            else if(op == 13)
            {
                const Id id=any();
                p.what="select "+std::to_string(id);
                if(tree->setSelectedNode(id))
                    p.selected=id;
            }
            else if(op == 14)
            {
                p.what="scroll";
                tree->getVirtualBody()->setVerticalScrollOffset(static_cast<int>(p.rng()%(20*static_cast<unsigned>(ids.size()+2))));
                anchorKept=false;
            }
            else if(op == 15 && p.rng()%3 == 0)
            {
                const unsigned mode=p.rng()%3;
                p.what="sort "+std::to_string(mode);
                if(mode == 0)
                    tree->setSortColumn(-1, controls::SortDirection::None);
                else
                    tree->setSortColumn(static_cast<int>(mode-1),
                        p.rng()%2?controls::SortDirection::Ascending:controls::SortDirection::Descending);
            }
            else if(op == 16 && p.rng()%3 == 0)
            {
                p.what="folders first";
                tree->setFoldersFirst(!tree->getFoldersFirst());
            }
            else if(op == 17 && p.rng()%4 == 0)
            {
                const bool expanded=p.rng()%2 != 0;
                p.what=expanded?"expandAll":"collapseAll";
                anchorKept=false;
                p.setAll(expanded);
            }
            else if(op == 18 && p.rng()%2 == 0)
            {
                const Id id=any();
                p.what="expandSubtree "+std::to_string(id);
                p.expandSubtree(id);
            }
            else if(op == 19)
            {
                // A single rename between the batch updates.
                const Id id=any();
                p.what="rename one "+std::to_string(id);
                p.names[id]="n"+std::to_string(p.rng()%8);
                p.bytes[id]=(p.rng()%8)*0.5;
                tree->nodeChanged(id);
            }
            else
                p.what="frame";

            p.check();
            // the selection's events: one per change of the selection by an
            // edit, none for setSelectedNode or no change
            if(p.what.rfind("select ", 0) != 0)
            {
                CAPTURE(selBefore);
                CHECK(p.selectionEvents == (p.selected != selBefore?1:0));
            }
            p.frame();
            p.checkRows();
            // the first shown node stays at the top, unless the edit removed
            // or hid it, scrolled, or the content no longer reaches as far
            controls::VirtualList *body=tree->getVirtualBody();
            const bool clamped=body->getVerticalScrollOffset() >= body->getMaxVerticalScrollOffset()
                || offsetBefore >= body->getMaxVerticalScrollOffset();
            if(anchorKept && topBefore != NoNode && p.m.count(topBefore) && p.itemOf(topBefore) != Tree::NoItem && !clamped)
            {
                CAPTURE(topBefore);
                CAPTURE(p.what);
                CAPTURE(offsetBefore);
                const int offsetAfter=body->getVerticalScrollOffset();
                const int maxAfter=body->getMaxVerticalScrollOffset();
                CAPTURE(offsetAfter);
                CAPTURE(maxAfter);
                const size_t itemAfter=p.itemOf(topBefore);
                CAPTURE(itemAfter);
                CHECK(p.top() == topBefore);
            }
        }
    }
}

TEST_CASE("TreeTable model: a sorted batch above a viewport that fits restores its final anchor")
{
    for(bool sorted : {false, true})
    {
        CAPTURE(sorted);
        Scene scene;
        Tree *tree=scene.tree;
        for(Id id=1; id <= 9; ++id)
        {
            scene.names[id]="m"+std::to_string(id);
            REQUIRE(tree->addNode(Root, id) == id);
        }
        scene.names[100]="a1";
        scene.names[101]="a2";
        if(sorted)
        {
            tree->setSortComparator(0, [&scene](Id a, Id b) { return scene.names.at(a) < scene.names.at(b); });
            tree->setSortColumn(0, controls::SortDirection::Ascending);
        }
        tree->setSelectedNode(1);
        scene.settle();
        auto *body=tree->getVirtualBody();
        REQUIRE(body->getMaxVerticalScrollOffset() == 0);
        REQUIRE(scene.top() == 1);

        REQUIRE(tree->insertNodes(Root, 0, {100, 101}));
        scene.frame();
        REQUIRE(visibleIds(tree) == std::vector<Id>{100, 101, 1, 2, 3, 4, 5, 6, 7, 8, 9});
        // The anchor's target is 40. Only the final list's maximum may clamp
        // it, not the first splice's maximum (19).
        REQUIRE(body->getMaxVerticalScrollOffset() == 39);
        CHECK(body->getVerticalScrollOffset() == 39);
        CHECK(tree->getSelectedNode() == 1);
        CHECK(tree->getSelectedItem() == 2);
    }
}

TEST_CASE("TreeTable model: a lazy answer that re-sorts its folder keeps the expansion anchor")
{
    // Answer now, later, and now with a nested expansion from the handler.
    // The last variant returns early from the outer setExpanded call.
    for(int answerMode=0; answerMode < 3; ++answerMode)
    {
        CAPTURE(answerMode);
        Scene scene;
        Tree *tree=scene.tree;
        std::map<Id, int> bytes;
        for(Id id=0; id <= 12; ++id)
        {
            bytes[id]=static_cast<int>(id);
            REQUIRE(tree->addNode(Root, id, id == 1) == id);
        }
        for(Id id=100; id <= 104; ++id)
            bytes[id]=0;
        const auto key=[tree, &bytes](Id id)
            {
                const Tree::Node *node=tree->getNode(id);
                return node->hasChildren?static_cast<int>(node->children.size()):bytes.at(id);
            };
        tree->setSortComparator(0, [key](Id a, Id b) { return key(a) < key(b); });
        tree->setSortColumn(0, controls::SortDirection::Ascending);
        Requests requests;
        requests.listen(tree);
        const auto answer=[&](Id id)
            {
                REQUIRE(tree->insertNodes(id, 0, {100, 101, 102, 103, 104}));
                if(answerMode == 2)
                    tree->setExpanded(id, true);
            };
        if(answerMode != 1)
            requests.answer=answer;
        tree->setSelectedNode(1);
        scene.settle();
        REQUIRE(visibleIds(tree) == std::vector<Id>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
        auto *body=tree->getVirtualBody();
        body->setVerticalScrollOffset(39);
        scene.settle();
        REQUIRE(scene.top() == 1);
        tree->setExpanded(1, true);
        scene.frame();
        if(answerMode == 1)
        {
            CHECK(tree->getNode(1)->loading);
            answer(1);
            scene.frame();
        }
        REQUIRE(visibleIds(tree) == std::vector<Id>{0, 2, 3, 4, 1, 100, 101, 102, 103, 104,
            5, 6, 7, 8, 9, 10, 11, 12});
        REQUIRE(body->getMaxVerticalScrollOffset() >= 4*20+19);
        CHECK(scene.top() == 1);
        CHECK(body->getVerticalScrollOffset() == 4*20+19);
        CHECK(tree->getSelectedNode() == 1);
        CHECK(tree->getSelectedItem() == 4);
        CHECK(tree->isExpanded(1));
        CHECK(requests.ids == std::vector<Id>{1});
        CHECK(requests.expanded == std::vector<Id>{1});
    }
}
