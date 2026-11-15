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
 */
#include "headlessFixture.h"

#include <gweni/controls/treeTable.h>
#include <doctest/doctest.h>

#include <cmath>
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

    void remove(Id id)
    {
        auto &siblings=children(nodes.at(id).parent);
        siblings.erase(std::find(siblings.begin(), siblings.end(), id));
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
        auto &old=children(nodes.at(id).parent);
        old.erase(std::find(old.begin(), old.end(), id));
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
