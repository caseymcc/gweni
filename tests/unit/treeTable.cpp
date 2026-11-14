/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  TreeTable's model and source-mode projection on a real Canvas with the
 *  headless StubSkin. Nodes keep app ids, parents and ordered children as
 *  data. Nested, out-of-order and hidden expansions flatten to preorder with
 *  the right depth; collapse preserves descendant expansion, while the bulk
 *  operations change every parent once. Binders receive node ids and depths,
 *  including after scrolling and a rebind. An expand/collapse binds only the
 *  changed suffix on screen, an offscreen change binds nothing, and neither
 *  makes/deletes controls or ids. At 1 000 and 100 000 nodes the pool, subtree
 *  and live ids are identical, with every id returned at teardown. No GL
 *  context or resources are needed.
 */
#include "headlessFixture.h"

#include <gweni/controls/treeTable.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <tuple>

using namespace gweni;

namespace
{

using TreeTable=controls::TreeTable;
using NodeId=TreeTable::NodeId;
constexpr NodeId Root=TreeTable::Root;

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    TreeTable *table=nullptr;
    using Bind=std::tuple<controls::DataTableRow *, NodeId, size_t>;
    std::vector<Bind> binds;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 400);
        table=canvas->newChild<TreeTable>();
        table->setBounds(10, 10, 400, 200);
        table->setColumnCount(3);
        table->setColumnCaption(0, "Name");
        table->setColumnCaption(1, "Size");
        table->setColumnCaption(2, "Modified");
        table->setRowHeight(20);
        table->setNodeBinder([this](controls::DataTableRow *row, NodeId id, size_t depth)
            {
                binds.emplace_back(row, id, depth);
                row->setCellText(0, "node "+std::to_string(id));
                row->setCellText(1, std::to_string(depth));
                row->setCellText(2, "2026-11-14");
            });
    }

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
        CHECK(canvas->getLayoutStats().passes <= 1);
        CHECK(canvas->getLayoutStats().requests == 0);
        CHECK(!canvas->getLayoutStats().renderLayout);
    }

    void settle()
    {
        frame();
        frame();
        binds.clear();
    }

    //! Noncontiguous app ids: a binder must not confuse an id with an item.
    void roots(size_t count)
    {
        for(size_t i=0; i < count; ++i)
            REQUIRE(table->addNode(Root, 100+i*7) == 100+i*7);
    }
};

std::vector<NodeId> ids(const TreeTable *table)
{
    std::vector<NodeId> result;

    for(const TreeTable::VisibleNode &node : table->getVisibleNodes())
        result.push_back(node.id);
    return result;
}

void checkIndex(TreeTable *table, const std::vector<NodeId> &expected)
{
    CHECK(ids(table) == expected);
    CHECK(table->getItemCount() == expected.size());
    for(size_t item=0; item < expected.size(); ++item)
    {
        CHECK(table->nodeForItem(item) == expected[item]);
        CHECK(table->itemForNode(expected[item]) == item);
        const TreeTable::Node *node=table->getNode(expected[item]);

        REQUIRE(node != nullptr);
        CHECK(table->getVisibleNodes()[item].depth == node->depth);
    }
    CHECK(table->nodeForItem(expected.size()) == TreeTable::NoNode);
}

void collect(controls::Base *control, std::vector<controls::Base *> &all)
{
    all.push_back(control);
    for(controls::Base *child : control->getActualChildren())
        collect(child, all);
}

struct Events: event::Handler
{
    std::vector<NodeId> expanded;
    std::vector<NodeId> collapsed;
    void expand(event::Info info) { expanded.push_back(info.integer); }
    void collapse(event::Info info) { collapsed.push_back(info.integer); }
};

}//namespace

TEST_CASE("TreeTable: nested and out-of-order expansion keeps the visible preorder")
{
    Scene scene;
    TreeTable *table=scene.table;

    REQUIRE(table->isSourceMode());
    REQUIRE(table->addNode(Root, 0) == 0);
    REQUIRE(table->addNode(Root, 42) == 42);
    REQUIRE(table->addNode(0, 7) == 7);
    REQUIRE(table->addNode(42, 90) == 90);
    REQUIRE(table->addNode(0, 3) == 3);
    REQUIRE(table->addNode(7, 11) == 11);
    REQUIRE(table->addNode(11, 25) == 25);
    REQUIRE(table->addNode(7, 12) == 12);
    checkIndex(table, {0, 42});
    CHECK(table->getNode(0)->children == std::vector<NodeId>{7, 3});
    CHECK(table->getNode(7)->parent == 0);
    CHECK(table->getNode(25)->depth == 3);

    CHECK(table->addNode(Root, 7) == TreeTable::NoNode);
    CHECK(table->addNode(999, 30) == TreeTable::NoNode);
    CHECK(table->addNode(Root, Root) == TreeTable::NoNode);
    CHECK(table->getNodeCount() == 8);
    CHECK(table->getNode(999) == nullptr);
    table->setExpanded(999, true);
    table->setExpanded(25, true);
    CHECK(!table->isExpanded(25));

    // Hidden expansion changes data; the parent exposes it when expanded.
    table->setExpanded(11, true);
    table->setExpanded(7, true);
    CHECK(table->itemForNode(25) == TreeTable::NoItem);
    checkIndex(table, {0, 42});
    table->setExpanded(42, true);
    checkIndex(table, {0, 42, 90});
    table->setExpanded(0, true);
    checkIndex(table, {0, 7, 11, 25, 12, 3, 42, 90});
    table->setExpanded(7, false);
    checkIndex(table, {0, 7, 3, 42, 90});
    CHECK(table->isExpanded(11));
    table->setExpanded(7, true);
    checkIndex(table, {0, 7, 11, 25, 12, 3, 42, 90});
    table->setExpanded(0, false);
    table->setExpanded(42, false);
    checkIndex(table, {0, 42});
    CHECK(table->isExpanded(7));
}

TEST_CASE("TreeTable: expand-all and collapse-all change hidden parents and notify once")
{
    Scene scene;
    TreeTable *table=scene.table;
    Events events;

    table->onExpanded.add(&events, &Events::expand);
    table->onCollapsed.add(&events, &Events::collapse);
    table->addNode(Root, 1);
    table->addNode(1, 2);
    table->addNode(2, 3);
    table->addNode(Root, 4, true); // advertised children, none supplied yet
    table->addNode(Root, 5);

    table->expandAll();
    checkIndex(table, {1, 2, 3, 4, 5});
    CHECK(table->isExpanded(1));
    CHECK(table->isExpanded(2));
    CHECK(table->isExpanded(4));
    CHECK(!table->isExpanded(3));
    CHECK(!table->isExpanded(5));
    CHECK(events.expanded == std::vector<NodeId>{1, 2, 4});
    table->expandAll();
    CHECK(events.expanded.size() == 3);

    table->setExpanded(1, false);
    checkIndex(table, {1, 4, 5});
    CHECK(events.collapsed == std::vector<NodeId>{1});
    table->setExpanded(1, false);
    CHECK(events.collapsed.size() == 1);

    table->collapseAll();
    checkIndex(table, {1, 4, 5});
    CHECK(!table->isExpanded(2));
    CHECK(!table->isExpanded(4));
    CHECK(events.collapsed == std::vector<NodeId>{1, 2, 4});
    table->collapseAll();
    CHECK(events.collapsed.size() == 3);
    table->setExpanded(1, true);
    checkIndex(table, {1, 2, 4, 5});

    // Flatten a deep hierarchy without recursive calls on the process stack.
    Scene deep;
    for(size_t i=0; i < 10000; ++i)
        deep.table->addNode(i == 0?Root:i-1, i);
    deep.table->expandAll();
    REQUIRE(deep.table->getItemCount() == 10000);
    CHECK(deep.table->getVisibleNodes().back().depth == 9999);
    deep.table->collapseAll();
    checkIndex(deep.table, {0});
}

TEST_CASE("TreeTable: the binder receives node ids and depths across scrolling and rebind")
{
    Scene scene;
    TreeTable *table=scene.table;

    table->addNode(Root, 900);
    table->addNode(900, 41);
    table->addNode(41, 8);
    table->addNode(900, 75);
    scene.roots(100);
    table->setExpanded(900, true);
    table->setExpanded(41, true);
    // Building under an already-expanded parent appends after its subtree.
    REQUIRE(table->addNode(900, 901) == 901);
    checkIndex(table, [&]()
        {
            std::vector<NodeId> expected{900, 41, 8, 75, 901};
            for(size_t i=0; i < 100; ++i)
                expected.push_back(100+i*7);
            return expected;
        }());
    scene.frame();
    REQUIRE(!scene.binds.empty());

    auto checkBinds=[&]()
        {
            for(const auto &bind : scene.binds)
            {
                auto *row=std::get<0>(bind);
                const NodeId id=std::get<1>(bind);
                const size_t depth=std::get<2>(bind);
                const size_t item=table->itemForRow(row);

                REQUIRE(table->getNode(id) != nullptr);
                CHECK(table->nodeForItem(item) == id);
                CHECK(depth == table->getNode(id)->depth);
                CHECK(row->getCellText(0) == "node "+std::to_string(id));
                CHECK(row->getCellText(1) == std::to_string(depth));
                CHECK(row->getCellText(2) == "2026-11-14");
            }
            scene.binds.clear();
        };
    checkBinds();
    CHECK(table->rowForItem(2)->getCellText(0) == "node 8");
    CHECK(table->rowForItem(2)->getCellText(1) == "2");

    table->getVirtualBody()->scrollByRows(1);
    scene.frame();
    REQUIRE(scene.binds.size() == 1);
    checkBinds();
    table->scrollToItem(80);
    scene.frame();
    checkBinds();
    table->scrollToItem(0);
    scene.frame();
    checkBinds();

    table->rebindItem(2);
    scene.frame();
    REQUIRE(scene.binds.size() == 1);
    CHECK(std::get<1>(scene.binds.front()) == 8);
    checkBinds();
    table->rebind();
    scene.frame();
    REQUIRE(!scene.binds.empty());
    checkBinds();

}

TEST_CASE("TreeTable: expand and collapse bind only changed on-screen rows without making controls")
{
    Scene scene;
    TreeTable *table=scene.table;

    scene.roots(1000);
    const NodeId parent=100+3*7;
    for(size_t i=0; i < 50; ++i)
        table->addNode(parent, 1000000+i);
    const NodeId offscreen=100+900*7;
    table->addNode(offscreen, 2000000);
    scene.settle();

    std::vector<controls::Base *> before;
    collect(table, before);
    const auto bankBefore=test::IdBankCounts::now();
    const size_t treeSerial=scene.canvas->treeSerial();
    const auto pool=table->getVirtualBody()->getRows();
    const size_t firstChanged=table->itemForNode(parent)+1;
    size_t expected=0;
    for(controls::Base *row : pool)
    {
        const size_t item=table->getVirtualBody()->itemForRow(row);
        if(item != TreeTable::NoItem && item >= firstChanged)
            ++expected;
    }
    REQUIRE(expected > 0);

    for(bool expanded : {true, false, true, false})
    {
        CAPTURE(expanded);
        scene.binds.clear();
        table->setExpanded(parent, expanded);
        scene.frame();
        CHECK(scene.binds.size() == expected);
        std::set<controls::DataTableRow *> seen;
        for(const auto &bind : scene.binds)
        {
            auto *row=std::get<0>(bind);
            CHECK(table->itemForRow(row) >= firstChanged);
            CHECK(!row->hidden());
            CHECK(seen.insert(row).second);
            CHECK(table->nodeForItem(table->itemForRow(row)) == std::get<1>(bind));
        }
        std::vector<controls::Base *> after;
        collect(table, after);
        CHECK(after == before);
        CHECK(scene.canvas->treeSerial() == treeSerial);
        CHECK(table->getVirtualBody()->getRows() == pool);
        const auto bankAfter=test::IdBankCounts::now();
        CHECK(bankAfter.livePrimitives() == bankBefore.livePrimitives());
        CHECK(bankAfter.liveTexts() == bankBefore.liveTexts());
    }

    scene.binds.clear();
    table->setExpanded(offscreen, true);
    scene.frame();
    CHECK(scene.binds.empty());
    table->setExpanded(offscreen, false);
    scene.frame();
    CHECK(scene.binds.empty());
    table->setExpanded(parent, false); // unchanged state
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
}

TEST_CASE("TreeTable: 1000 and 100000 nodes keep the same pool and live ids and return them all")
{
    using Counts=std::tuple<size_t, size_t, size_t, size_t>;
    std::vector<Counts> counts;

    for(size_t count : {1000, 100000})
    {
        CAPTURE(count);
        const auto baseline=test::IdBankCounts::now();
        {
            Scene scene;
            scene.roots(count-50);
            for(size_t i=0; i < 50; ++i)
                scene.table->addNode(100, 1000000+i);
            scene.table->setExpanded(100, true);
            REQUIRE(scene.table->getNodeCount() == count);
            scene.settle();
            const auto pool=scene.table->getVirtualBody()->getRows();
            const int viewport=scene.table->getVirtualBody()->getViewportHeight();

            CHECK(pool.size() == static_cast<size_t>((viewport+19)/20)+1);
            std::vector<controls::Base *> subtree;
            collect(scene.table, subtree);
            const auto now=test::IdBankCounts::now();

            counts.emplace_back(pool.size(), subtree.size(),
                now.livePrimitives()-baseline.livePrimitives(),
                now.liveTexts()-baseline.liveTexts());
            CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
            CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
        }
        const auto after=test::IdBankCounts::now();
        CHECK(after.livePrimitives() == baseline.livePrimitives());
        CHECK(after.liveTexts() == baseline.liveTexts());
    }
    REQUIRE(counts.size() == 2);
    CHECK(counts[0] == counts[1]);
}
