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
 *  and live ids are identical, with every id returned at teardown.
 *
 *  The tree column: every pooled row is a TreeTableRow made with its
 *  expander; the tree cell's left padding is the cell padding plus one Indent
 *  per depth plus the expander's column (a leaf's too), the other cells keep
 *  theirs, the expander sits after the indent, ExpanderSize square, centred,
 *  clipped to the tree cell, hidden for a node with no children and showing
 *  its node's expansion, also on the toggled row that is not bound again; an
 *  expand that adds no row marks the expander alone, with no binding or
 *  layout; setTreeColumn moves all of it to another column. A one-row scroll
 *  that binds a row to a node at another depth enters as many controls as one
 *  at the same depth but the moved expander. A click on the expander toggles
 *  without selecting the row; a click on the row selects it; a double-click
 *  toggles a parent and activates a leaf (onNodeActivated). The textured
 *  renderer draws no surface at rest, TreeTableExpanderHover hovered and
 *  TreeTableExpanderDown pressed, and a kind-1 chevron pointing right while
 *  collapsed and down while expanded in the TreeTableExpander, -Hover and
 *  -Disabled text colours, all of them in the theme's registries.
 *
 *  The selection is a node: setSelectedNode fires nothing and refuses a
 *  hidden or unknown node; expands and collapses above it move its item, not
 *  it; collapsing an ancestor that hides it (setExpanded or collapse-all)
 *  moves it to that ancestor (the topmost collapsed one) with one
 *  onSelectionChange carrying the id; expand-all and expandSubtree keep it;
 *  scrolled off and back the row showing it is the only one selected; a click
 *  selects the row's node and focuses the table. A plain source-mode
 *  DataTable holds the focus and the Tab stop (its pooled rows neither):
 *  Up/Down a row, Home/End, PageUp/PageDown by pageRows(), clamped at the
 *  ends, each firing onSelectionChange and leaving the row wholly in the
 *  viewport, Up or End with nothing selected taking the first or the last
 *  item; an empty or row-mode table leaves the keys to its parents. The tree
 *  keys: Right expands a collapsed parent then moves to its first child and
 *  does nothing at a leaf or an empty advertised parent; Left moves a leaf or
 *  a collapsed node to its parent, collapses an expanded one and does nothing
 *  at a collapsed root; `*` expands the whole subtree with one onExpanded per
 *  node, once; Return activates the selected node, parent or leaf; with no
 *  selection none of them is taken. With a TextBox in a cell focused, the row
 *  and tree keys are not the table's: they go on past it, moving, expanding
 *  and activating nothing. With the columns wider than the table, a column's
 *  new width marks no cell left of it (the cells keep Label's clip rects; the
 *  row does not report child clip rects). A resting mouse on an expander
 *  whose row a keyboard collapse binds to a leaf no longer hovers it, and a
 *  click there without a move selects the leaf. No GL context or resources
 *  are needed.
 */
#include "headlessFixture.h"

#include <gweni/controls/textBox.h>
#include <gweni/controls/treeTable.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <set>
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
    float clock=nextClock();

    //! The input handler keeps the last click's time and place across cases,
    //! so each scene's clock starts well after the last one's.
    static float nextClock()
    {
        static float start=0.0f;

        start+=1000.0f;
        return start;
    }

    Scene()
    {
        // a second apart per click, so two clicks are a double click only
        // when a test makes them one
        platform::setTimeSource([this]() { return clock; });
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

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    static Point centreOf(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        return Point(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    //! Presses on the control's centre, a second after the last click.
    void press(controls::Base *control)
    {
        const Point at=centreOf(control);

        clock+=1.0f;
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        input::onMouseClicked(canvas, 0, true);
    }

    void release()
    {
        input::onMouseClicked(canvas, 0, false);
    }

    void click(controls::Base *control)
    {
        press(control);
        release();
        frame();
    }

    //! Two presses at one place and time: the second is a double click.
    void doubleClick(controls::Base *control)
    {
        press(control);
        release();
        input::onMouseClicked(canvas, 0, true);
        release();
        frame();
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    void character(UnicodeChar c)
    {
        canvas->inputCharacter(c);
        frame();
    }

    controls::TreeTableRow *row(size_t item) const
    {
        return gweni_cast<controls::TreeTableRow>(table->rowForItem(item));
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
    std::vector<NodeId> activated;
    std::vector<size_t> selections;
    void expand(event::Info info) { expanded.push_back(info.integer); }
    void select(event::Info info) { selections.push_back(info.integer); }
    void collapse(event::Info info) { collapsed.push_back(info.integer); }
    void activate(event::Info info) { activated.push_back(info.integer); }
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

TEST_CASE("TreeTable: the tree cell is indented by depth and a parent's expander shows its state")
{
    Scene scene;
    TreeTable *table=scene.table;
    using Row=controls::DataTableRow;

    table->addNode(Root, 10);
    table->addNode(10, 11);
    table->addNode(11, 12);
    table->addNode(10, 13);
    table->addNode(Root, 20, true); // advertised children, none supplied
    table->addNode(Root, 30);
    table->setExpanded(10, true);
    table->setExpanded(11, true);
    scene.settle();
    checkIndex(table, {10, 11, 12, 13, 20, 30});

    // every pooled row is a TreeTableRow with its expander, spare included
    for(controls::Base *control : table->getVirtualBody()->getRows())
    {
        auto *row=gweni_cast<controls::TreeTableRow>(control);

        REQUIRE(row != nullptr);
        REQUIRE(row->getExpander() != nullptr);
        CHECK(row->getExpander()->getParent() == row);
    }

    auto checkRow=[&](size_t item, int treeColumn)
        {
            CAPTURE(item);
            controls::TreeTableRow *row=scene.row(item);
            REQUIRE(row != nullptr);
            const TreeTable::Node *node=table->getNode(table->nodeForItem(item));
            REQUIRE(node != nullptr);
            CHECK(row->getDepth() == node->depth);

            for(int column=0; column < row->getCellCount(); ++column)
            {
                CAPTURE(column);
                const Padding padding=row->getCell(column)->getPadding();
                const int left=(column == treeColumn)?
                    Row::CellPaddingX+int(node->depth)*TreeTable::Indent+TreeTable::Indent:Row::CellPaddingX;

                CHECK(padding.left == left);
                CHECK(padding.right == Row::CellPaddingX);
                CHECK(padding.top == Row::CellPaddingY);
                CHECK(padding.bottom == Row::CellPaddingY);
            }

            controls::internal::TreeTableExpander *expander=row->getExpander();
            const Rect cell=row->getCell(treeColumn)->getBounds();
            const Rect bounds=expander->getBounds();

            CHECK(expander->hidden() == !node->hasChildren);
            CHECK(expander->isExpanded() == node->expanded);
            CHECK(bounds.x == cell.x+Row::CellPaddingX+int(node->depth)*TreeTable::Indent);
            CHECK(bounds.w == TreeTable::ExpanderSize);
            CHECK(bounds.h == TreeTable::ExpanderSize);
            CHECK(bounds.y == (row->getHeight()-TreeTable::ExpanderSize)/2);
            const Rect *clip=row->getChildClipRect(expander);
            REQUIRE((clip != nullptr));
            CHECK((*clip == Rect(cell.x, 0, cell.w, row->getHeight())));
            // the cells keep Label's rects: the row's bounds
            const Rect *cellClip=row->getChildClipRect(row->getCell(treeColumn));
            REQUIRE((cellClip != nullptr));
            CHECK((*cellClip == Rect(0, 0, row->getWidth(), row->getHeight())));
        };
    for(size_t item=0; item < 6; ++item)
        checkRow(item, 0);

    // an expand that adds no row (advertised children) marks its expander
    // alone, with no binding and no layout
    controls::TreeTableRow *advertised=scene.row(4);
    table->setExpanded(20, true);
    CHECK(advertised->getExpander()->isExpanded());
    CHECK(advertised->getExpander()->getStateChange() == controls::StateChange_Background);
    scene.frame();
    CHECK(scene.binds.empty());
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    checkRow(4, 0);

    // a collapse shows the toggled row's new state without binding it
    controls::TreeTableRow *toggled=scene.row(0);
    table->setExpanded(10, false);
    scene.frame();
    CHECK(scene.row(0) == toggled);
    CHECK_FALSE(toggled->getExpander()->isExpanded());
    for(const auto &bind : scene.binds)
        CHECK(std::get<0>(bind) != toggled);
    checkIndex(table, {10, 20, 30});
    for(size_t item=0; item < 3; ++item)
        checkRow(item, 0);
    table->expandAll();
    scene.frame();
    for(size_t item=0; item < 6; ++item)
        checkRow(item, 0);
    table->collapseAll();
    scene.frame();
    for(size_t item=0; item < 3; ++item)
        checkRow(item, 0);
    table->expandAll();
    scene.settle();

    // the tree in another column; out of range is refused
    table->setTreeColumn(1);
    CHECK(table->getTreeColumn() == 1);
    table->setTreeColumn(3);
    table->setTreeColumn(-1);
    CHECK(table->getTreeColumn() == 1);
    scene.frame();
    CHECK(scene.binds.empty());
    for(size_t item=0; item < 6; ++item)
        checkRow(item, 1);

    // a narrow tree column cuts the expander: the clip follows the column
    table->setColumnWidth(1, TreeTable::DefaultMinColumnWidth);
    scene.frame();
    for(size_t item=0; item < 6; ++item)
        checkRow(item, 1);
}

TEST_CASE("TreeTable: a row bound to a node at another depth lays out that row alone")
{
    // Two scrolls by one row with the same text lengths: one brings in a row
    // at the depth of the item that left, the other one at another depth.
    // Both bind and lay out one row, so they enter as many controls but the
    // moved expander of the row at another depth (its entries, at most one
    // per calculateSize and arrange in each axis).
    auto scrollVisited=[](bool otherDepth)
        {
            Scene scene;
            TreeTable *table=scene.table;

            // ids of one digit count, so the rows' strings are as long, and
            // each a collapsed parent, so every row shows its expander
            table->addNode(Root, 100);
            for(NodeId id=101; id < 200; ++id)
                table->addNode(otherDepth?NodeId(100):Root, id);
            for(NodeId id=100; id < 200; ++id)
                table->addNode(id, id+100);
            table->setExpanded(100, true);
            // the depth in no cell's text: only the tree cell's padding
            // follows it
            table->setNodeBinder([&scene](controls::DataTableRow *row, NodeId id, size_t depth)
                {
                    scene.binds.emplace_back(row, id, depth);
                    row->setCellText(0, "node "+std::to_string(id));
                    row->setCellText(1, "size");
                    row->setCellText(2, "2026-11-14");
                });
            scene.settle();
            const auto pool=table->getVirtualBody()->getRows();
            controls::TreeTableRow *leaving=scene.row(0);
            const size_t depthBefore=leaving->getDepth();

            table->getVirtualBody()->scrollByRows(1);
            scene.frame();
            REQUIRE(scene.binds.size() == 1);
            CHECK(std::get<0>(scene.binds.front()) == leaving);
            CHECK(table->getVirtualBody()->getRows() == pool);
            CHECK(scene.canvas->getLayoutStats().passes == 1);
            CHECK((leaving->getDepth() != depthBefore) == otherDepth);
            CHECK(leaving->getCell(0)->getPadding().left
                == controls::DataTableRow::CellPaddingX+int(leaving->getDepth())*TreeTable::Indent+TreeTable::Indent);
            return scene.canvas->getLayoutStats().visited;
        };

    const size_t same=scrollVisited(false);
    const size_t other=scrollVisited(true);

    CHECK(same > 0);
    CHECK(other >= same);
    CHECK(other-same <= 4);
}

TEST_CASE("TreeTable: the expander toggles its node and a double-click toggles a parent or activates a leaf")
{
    Scene scene;
    TreeTable *table=scene.table;
    Events events;

    table->onExpanded.add(&events, &Events::expand);
    table->onCollapsed.add(&events, &Events::collapse);
    table->onNodeActivated.add(&events, &Events::activate);
    table->addNode(Root, 10);
    table->addNode(10, 11);
    table->addNode(10, 12);
    table->addNode(Root, 20);
    scene.settle();

    controls::TreeTableRow *parent=scene.row(0);
    REQUIRE(parent != nullptr);
    scene.click(parent->getExpander());
    CHECK(table->isExpanded(10));
    checkIndex(table, {10, 11, 12, 20});
    CHECK(events.expanded == std::vector<NodeId>{10});
    // the expander takes the press: the row is not selected, nothing focused
    CHECK(table->getSelectedItem() == TreeTable::NoItem);
    CHECK(KeyboardFocus != parent->getExpander());
    scene.frame();
    CHECK(parent->getExpander()->isExpanded());

    scene.click(parent->getExpander());
    CHECK_FALSE(table->isExpanded(10));
    checkIndex(table, {10, 20});
    CHECK(events.collapsed == std::vector<NodeId>{10});
    scene.frame();

    // a click on the row selects it and toggles nothing
    scene.click(parent->getCell(1));
    CHECK(table->getSelectedItem() == 0);
    CHECK_FALSE(table->isExpanded(10));

    // a double-click on a parent's row toggles it, once per double-click
    scene.doubleClick(parent->getCell(1));
    CHECK(table->isExpanded(10));
    checkIndex(table, {10, 11, 12, 20});
    CHECK(events.expanded == std::vector<NodeId>{10, 10});
    CHECK(events.activated.empty());
    scene.frame();
    scene.doubleClick(parent->getCell(1));
    CHECK_FALSE(table->isExpanded(10));
    CHECK(events.collapsed == std::vector<NodeId>{10, 10});
    scene.frame();

    // on a leaf it activates the node
    controls::TreeTableRow *leaf=scene.row(1);
    REQUIRE(table->nodeForItem(1) == 20);
    CHECK(leaf->getExpander()->hidden());
    scene.doubleClick(leaf->getCell(0));
    CHECK(events.activated == std::vector<NodeId>{20});
    CHECK(table->getSelectedItem() == 1);
    CHECK(events.expanded.size() == 2);
    CHECK(events.collapsed.size() == 2);

    // a single click on a leaf activates nothing
    scene.click(leaf->getCell(1));
    CHECK(events.activated.size() == 1);

    // toggleExpanded on a leaf or an unknown id does nothing
    table->toggleExpanded(20);
    table->toggleExpanded(999);
    CHECK(events.expanded.size() == 2);
}

namespace
{

//! Records each slot's last chevron and rounded-rect fill, and hides.
class ExpanderRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        bool hidden;
        bool icon;
        int kind;
        int direction;
        Color color;
    };

    void drawRoundedRect(size_t id, Rect, float, float, Color top, Color, Color, int) override
    {
        draws.push_back({id, false, false, 0, 0, top});
    }

    void drawIcon(size_t id, Rect, int kind, int direction, Color color, int) override
    {
        draws.push_back({id, false, true, kind, direction, color});
    }

    void hidePrimitive(size_t id) override
    {
        draws.push_back({id, true, false, 0, 0, Color()});
    }

    const Draw *last(size_t id) const
    {
        for(auto it=draws.rbegin(); it != draws.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    std::vector<Draw> draws;
};

}//namespace

TEST_CASE("TreeTable: the textured expander draws a chevron by its state and a surface while hovered or pressed")
{
    namespace tokyonight=skin::textured::tokyonight;
    float clock=Scene::nextClock();
    platform::setTimeSource([&clock]() { return clock; });
    ExpanderRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    auto *canvas=new controls::Canvas(&skin);
    canvas->setSize(600, 400);
    auto *table=canvas->newChild<TreeTable>();
    table->setBounds(10, 10, 400, 200);
    table->setColumnCount(2);
    table->addNode(Root, 1);
    table->addNode(1, 2);
    table->addNode(Root, 3);
    auto frame=[&]()
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
        };
    frame();
    frame();

    auto *parent=gweni_cast<controls::TreeTableRow>(table->rowForItem(0));
    REQUIRE(parent != nullptr);
    controls::internal::TreeTableExpander *expander=parent->getExpander();
    std::vector<size_t> ids, textIds;
    REQUIRE(expander->getSkinControl() != nullptr);
    expander->getSkinControl()->appendIds(ids, textIds);
    REQUIRE(ids.size() >= 2);
    const auto &colors=skin.Colors.TreeTable;

    auto check=[&](bool surface, const tokyonight::Surface *expected, int direction, Color glyph)
        {
            const ExpanderRecorder::Draw *fill=renderer.last(ids[0]);
            const ExpanderRecorder::Draw *chevron=renderer.last(ids[1]);

            REQUIRE(fill != nullptr);
            REQUIRE(chevron != nullptr);
            CHECK(fill->hidden == !surface);
            if(surface)
                CHECK((fill->color == expected->top));
            CHECK_FALSE(chevron->hidden);
            CHECK(chevron->icon);
            CHECK(chevron->kind == 1);
            CHECK(chevron->direction == direction);
            CHECK((chevron->color == glyph));
        };

    // at rest: no surface, a right chevron
    check(false, nullptr, 3, colors.Expander);
    // hovered
    const Point at=Scene::centreOf(expander);
    canvas->inputMouseMoved(at.x, at.y, 0, 0);
    frame();
    check(true, &tokyonight::TreeTableExpanderHover, 3, colors.ExpanderHover);
    // pressed
    input::onMouseClicked(canvas, 0, true);
    frame();
    check(true, &tokyonight::TreeTableExpanderDown, 3, colors.ExpanderHover);
    // released: the node expands and the chevron points down
    input::onMouseClicked(canvas, 0, false);
    frame();
    CHECK(table->isExpanded(1));
    check(true, &tokyonight::TreeTableExpanderHover, 0, colors.ExpanderHover);
    // the mouse away: at rest, down
    canvas->inputMouseMoved(500, 350, 0, 0);
    frame();
    check(false, nullptr, 0, colors.Expander);
    // disabled
    expander->setDisabled(true);
    frame();
    check(false, nullptr, 0, colors.ExpanderDisabled);

    // the names are in the theme's registries
    CHECK(tokyonight::surfaceRegistry().count("TreeTableExpanderHover") == 1);
    CHECK(tokyonight::surfaceRegistry().count("TreeTableExpanderDown") == 1);
    const auto textColors=skin.textColorRegistry();
    CHECK(textColors.count("TreeTableExpander") == 1);
    CHECK(textColors.count("TreeTableExpanderHover") == 1);
    CHECK(textColors.count("TreeTableExpanderDisabled") == 1);

    HoveredControl=nullptr;
    KeyboardFocus=nullptr;
    MouseFocus=nullptr;
    delete canvas;
    platform::setTimeSource(nullptr);
}

TEST_CASE("TreeTable: the selection is a node kept across expand collapse and scroll and moved to the ancestor that hides it")
{
    Scene scene;
    TreeTable *table=scene.table;
    Events events;

    table->onSelectionChange.add(&events, &Events::select);
    table->addNode(Root, 10);
    table->addNode(10, 11);
    table->addNode(11, 12);
    table->addNode(10, 13);
    table->addNode(Root, 20);
    table->addNode(20, 21);
    table->addNode(Root, 30);
    scene.roots(40);
    scene.settle();

    auto checkSelected=[&](NodeId expected)
        {
            CAPTURE(expected);
            CHECK(table->getSelectedNode() == expected);
            const size_t item=(expected == TreeTable::NoNode)?TreeTable::NoItem:table->itemForNode(expected);

            CHECK(table->getSelectedItem() == item);
            // the pooled rows show it, and only it, as selected
            for(controls::Base *control : table->getVirtualBody()->getRows())
            {
                auto *row=gweni_cast<controls::DataTableRow>(control);
                const size_t shows=table->itemForRow(row);

                if(shows != TreeTable::NoItem)
                    CHECK(row->isSelected() == (shows == item));
            }
        };

    // no event from the app's selection
    CHECK(table->setSelectedNode(30));
    scene.frame();
    checkSelected(30);
    CHECK(table->getSelectedItem() == 2);

    // an expand above it moves its item, not the selection
    table->setExpanded(10, true);
    scene.frame();
    checkSelected(30);
    CHECK(table->getSelectedItem() == 4);
    table->setExpanded(11, true);
    scene.frame();
    checkSelected(30);
    CHECK(table->getSelectedItem() == 5);
    table->setExpanded(11, false);
    scene.frame();
    checkSelected(30);
    // a collapse not hiding it moves it back up
    table->setExpanded(10, false);
    scene.frame();
    checkSelected(30);
    CHECK(events.selections.empty());

    // hidden and unknown nodes are refused, NoNode clears it
    CHECK_FALSE(table->setSelectedNode(12));
    CHECK_FALSE(table->setSelectedNode(999));
    checkSelected(30);
    CHECK(table->setSelectedNode(TreeTable::NoNode));
    scene.frame();
    checkSelected(TreeTable::NoNode);

    // collapsing a hidden node's ancestor moves the selection to it, once
    table->setExpanded(10, true);
    table->setExpanded(11, true);
    REQUIRE(table->setSelectedNode(12));
    scene.frame();
    table->setExpanded(11, false);
    scene.frame();
    checkSelected(11);
    CHECK(events.selections == std::vector<size_t>{11});
    table->setExpanded(11, true);
    scene.frame();
    checkSelected(11);
    REQUIRE(table->setSelectedNode(12));
    table->setExpanded(10, false);
    scene.frame();
    checkSelected(10);
    CHECK(events.selections == std::vector<size_t>{11, 10});
    // expanding again keeps it on the ancestor
    table->setExpanded(10, true);
    scene.frame();
    checkSelected(10);

    // collapse-all moves it to the root that shows, expand-all keeps it
    table->setExpanded(20, true);
    REQUIRE(table->setSelectedNode(21));
    table->collapseAll();
    scene.frame();
    checkSelected(20);
    CHECK(events.selections == std::vector<size_t>{11, 10, 20});
    table->expandAll();
    scene.frame();
    checkSelected(20);
    REQUIRE(table->setSelectedNode(12));
    table->expandSubtree(10);
    scene.frame();
    checkSelected(12);
    CHECK(events.selections.size() == 3);

    // scrolled away and back it is still the node, on whichever row shows it
    table->scrollToItem(table->getItemCount()-1);
    scene.frame();
    CHECK(table->rowForItem(table->itemForNode(12)) == nullptr);
    checkSelected(12);
    table->scrollToItem(0);
    scene.frame();
    REQUIRE(table->rowForItem(table->itemForNode(12)) != nullptr);
    checkSelected(12);

    // a click selects the row's node, with the node's id
    controls::TreeTableRow *row=scene.row(table->itemForNode(20));
    REQUIRE(row != nullptr);
    scene.click(row->getCell(1));
    checkSelected(20);
    CHECK(events.selections.back() == 20);
    CHECK(KeyboardFocus == table);
}

TEST_CASE("DataTable source mode: row keys move the selection and keep it in view")
{
    float clock=Scene::nextClock();
    platform::setTimeSource([&clock]() { return clock; });
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    auto *canvas=new controls::Canvas(&skin);
    canvas->setSize(600, 400);
    auto *table=canvas->newChild<controls::DataTable>();
    table->setBounds(10, 10, 400, 200);
    table->setColumnCount(2);
    table->setRowHeight(20);
    table->setItemCount(100);
    table->setRowBinder([](controls::DataTableRow *row, size_t item)
        {
            row->setCellText(0, std::to_string(item));
        });
    Events events;
    table->onSelectionChange.add(&events, &Events::select);
    auto frame=[&]()
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
        };
    auto key=[&](int key)
        {
            input::onKeyEvent(canvas, key, true);
            input::onKeyEvent(canvas, key, false);
            frame();
        };
    frame();
    frame();

    // the table takes the focus and the Tab stop, its pooled rows neither
    CHECK(table->getKeyboardInputEnabled());
    CHECK(table->isTabable());
    for(controls::Base *row : table->getVirtualBody()->getRows())
    {
        CHECK_FALSE(row->getKeyboardInputEnabled());
        CHECK_FALSE(row->isTabable());
    }
    controls::DataTableRow *third=table->rowForItem(2);
    REQUIRE(third != nullptr);
    const Point at=Scene::centreOf(third->getCell(1));
    canvas->inputMouseMoved(at.x, at.y, 0, 0);
    input::onMouseClicked(canvas, 0, true);
    input::onMouseClicked(canvas, 0, false);
    frame();
    CHECK(KeyboardFocus == table);
    CHECK(table->getSelectedItem() == 2);
    events.selections.clear();

    const size_t page=table->pageRows();
    CHECK(page == size_t(table->getVirtualBody()->getViewportHeight()/20));
    REQUIRE(page > 1);

    auto check=[&](size_t item)
        {
            CAPTURE(item);
            CHECK(table->getSelectedItem() == item);
            controls::DataTableRow *row=table->rowForItem(item);
            REQUIRE(row != nullptr);
            CHECK(row->isSelected());
            // fully in the viewport
            const int top=row->getY();
            CHECK(top >= 0);
            CHECK(top+row->getHeight() <= table->getVirtualBody()->getViewportHeight());
        };

    key(keys::Down);
    check(3);
    key(keys::Up);
    check(2);
    key(keys::Up);
    key(keys::Up);
    check(0);
    key(keys::Up);
    check(0);
    key(keys::PageDown);
    check(page);
    key(keys::PageDown);
    check(2*page);
    key(keys::PageUp);
    check(page);
    key(keys::End);
    check(99);
    key(keys::Down);
    check(99);
    key(keys::PageDown);
    check(99);
    key(keys::PageUp);
    check(99-page);
    key(keys::Home);
    check(0);
    key(keys::PageUp);
    check(0);
    CHECK(events.selections == std::vector<size_t>{3, 2, 1, 0, page, 2*page, page, 99, 99-page, 0});

    // nothing selected: every key selects the first item, End the last
    table->setSelectedItem(controls::DataTable::NoItem, false);
    key(keys::Up);
    check(0);
    table->setSelectedItem(controls::DataTable::NoItem, false);
    key(keys::End);
    check(99);

    // an empty table and a row-mode table leave the keys to their parents
    table->setItemCount(0);
    frame();
    CHECK_FALSE(table->onKeyDown(true));
    auto *rows=canvas->newChild<controls::DataTable>();
    rows->setColumnCount(1);
    rows->addRow();
    rows->addRow();
    CHECK_FALSE(rows->onKeyDown(true));
    CHECK_FALSE(rows->onKeyend(true));
    CHECK_FALSE(rows->getKeyboardInputEnabled());

    HoveredControl=nullptr;
    KeyboardFocus=nullptr;
    MouseFocus=nullptr;
    delete canvas;
    platform::setTimeSource(nullptr);
}

TEST_CASE("TreeTable: Right Left star and Return expand collapse move and activate")
{
    Scene scene;
    TreeTable *table=scene.table;
    Events events;

    table->onExpanded.add(&events, &Events::expand);
    table->onCollapsed.add(&events, &Events::collapse);
    table->onNodeActivated.add(&events, &Events::activate);
    table->onSelectionChange.add(&events, &Events::select);
    table->addNode(Root, 1);
    table->addNode(1, 2);
    table->addNode(2, 3);
    table->addNode(2, 4);
    table->addNode(1, 5);
    table->addNode(Root, 6);
    table->addNode(Root, 7, true); // advertised children, none supplied
    scene.settle();
    table->focus();

    // nothing selected: the tree keys are not taken
    CHECK_FALSE(table->onKeyRight(true));
    CHECK_FALSE(table->onKeyLeft(true));
    CHECK_FALSE(table->onKeyReturn(true));
    CHECK_FALSE(table->onChar('*'));

    REQUIRE(table->setSelectedNode(1));
    // Right expands a collapsed parent, then moves to its first child
    scene.key(keys::Right);
    CHECK(table->isExpanded(1));
    checkIndex(table, {1, 2, 5, 6, 7});
    CHECK(table->getSelectedNode() == 1);
    scene.key(keys::Right);
    CHECK(table->getSelectedNode() == 2);
    scene.key(keys::Right);
    CHECK(table->isExpanded(2));
    scene.key(keys::Right);
    CHECK(table->getSelectedNode() == 3);
    // at a leaf Right does nothing
    scene.key(keys::Right);
    CHECK(table->getSelectedNode() == 3);
    checkIndex(table, {1, 2, 3, 4, 5, 6, 7});
    CHECK(events.expanded == std::vector<NodeId>{1, 2});
    CHECK(events.selections == std::vector<size_t>{2, 3});

    // Down and Up are DataTable's, through the visible rows
    scene.key(keys::Down);
    CHECK(table->getSelectedNode() == 4);
    scene.key(keys::Down);
    CHECK(table->getSelectedNode() == 5);
    scene.key(keys::Up);
    scene.key(keys::Up);
    CHECK(table->getSelectedNode() == 3);

    // Left at a leaf moves to its parent, at an expanded node collapses it,
    // at a collapsed one moves to its parent, at a collapsed root does nothing
    scene.key(keys::Left);
    CHECK(table->getSelectedNode() == 2);
    scene.key(keys::Left);
    CHECK_FALSE(table->isExpanded(2));
    CHECK(table->getSelectedNode() == 2);
    scene.key(keys::Left);
    CHECK(table->getSelectedNode() == 1);
    scene.key(keys::Left);
    CHECK_FALSE(table->isExpanded(1));
    checkIndex(table, {1, 6, 7});
    scene.key(keys::Left);
    CHECK(table->getSelectedNode() == 1);
    checkIndex(table, {1, 6, 7});
    CHECK(events.collapsed == std::vector<NodeId>{2, 1});
    // a root leaf: neither key moves
    REQUIRE(table->setSelectedNode(6));
    scene.key(keys::Left);
    scene.key(keys::Right);
    CHECK(table->getSelectedNode() == 6);
    checkIndex(table, {1, 6, 7});

    // an advertised parent expands with nothing to show, then Right stops
    REQUIRE(table->setSelectedNode(7));
    scene.key(keys::Right);
    CHECK(table->isExpanded(7));
    scene.key(keys::Right);
    CHECK(table->getSelectedNode() == 7);
    checkIndex(table, {1, 6, 7});

    // * expands the whole subtree once per node, building it at once
    REQUIRE(table->setSelectedNode(1));
    events.expanded.clear();
    scene.character('*');
    checkIndex(table, {1, 2, 3, 4, 5, 6, 7});
    CHECK(events.expanded == std::vector<NodeId>{1, 2});
    CHECK(table->getSelectedNode() == 1);
    scene.character('*');
    CHECK(events.expanded.size() == 2);

    // Return activates the selected node, a parent as a leaf
    scene.key(keys::Return);
    REQUIRE(table->setSelectedNode(4));
    scene.key(keys::Return);
    CHECK(events.activated == std::vector<NodeId>{1, 4});
    // an activation changes nothing else
    checkIndex(table, {1, 2, 3, 4, 5, 6, 7});
    CHECK(table->getSelectedNode() == 4);
}

TEST_CASE("TreeTable: keys from a focused control in a cell go on past the table")
{
    Scene scene;
    TreeTable *table=scene.table;
    Events events;

    table->onExpanded.add(&events, &Events::expand);
    table->onCollapsed.add(&events, &Events::collapse);
    table->onNodeActivated.add(&events, &Events::activate);
    table->onSelectionChange.add(&events, &Events::select);
    table->addNode(Root, 1);
    table->addNode(1, 2);
    table->addNode(Root, 3);
    table->addNode(3, 4);
    table->addNode(Root, 5);
    table->addNode(Root, 6);
    scene.settle();
    REQUIRE(table->setSelectedNode(3));

    controls::TextBox *box=scene.canvas->newChild<controls::TextBox>();
    box->setBounds(4, 2, 60, 14);
    scene.row(3)->setCellContents(1, box);
    scene.settle();
    REQUIRE(box->getParent() == scene.row(3)->getCell(1));
    box->focus();
    REQUIRE(KeyboardFocus == box);

    // the table's own handlers refuse them while another control has the
    // focus, so they bubble on (a dialog's default button gets the Return)
    CHECK_FALSE(table->onKeyReturn(true));
    CHECK_FALSE(table->onKeyRight(true));
    CHECK_FALSE(table->onKeyLeft(true));
    CHECK_FALSE(table->onKeyDown(true));
    CHECK_FALSE(table->onKeyUp(true));
    CHECK_FALSE(table->onKeyPageDown(true));
    CHECK_FALSE(table->onKeyHome(true));
    CHECK_FALSE(table->onChar('*'));

    // pressed in the box, none of them moves, expands or activates anything;
    // Return last, since its release is the box's Tab to the next stop
    for(int key : {keys::Down, keys::Up, keys::PageDown, keys::PageUp})
        scene.key(key);
    CHECK(KeyboardFocus == box);
    scene.key(keys::Return);
    CHECK(table->getSelectedNode() == 3);
    CHECK_FALSE(table->isExpanded(3));
    CHECK(events.activated.empty());
    CHECK(events.expanded.empty());
    CHECK(events.selections.empty());

    // focused itself, the table takes them again
    table->focus();
    scene.key(keys::Down);
    CHECK(table->getSelectedNode() == 5);
    scene.key(keys::Return);
    CHECK(events.activated == std::vector<NodeId>{5});
}

TEST_CASE("TreeTable: an overflowing column drag marks nothing left of the column")
{
    for(int column : {1, 2})
    {
        CAPTURE(column);
        Scene scene;
        TreeTable *table=scene.table;

        table->setColumnCount(4);
        for(int c=0; c < 4; ++c)
            table->setColumnWidth(c, 150);
        scene.roots(40);
        for(size_t i=0; i < 40; ++i)
            table->addNode(100+i*7, 1000+i);
        scene.settle();
        REQUIRE(table->contentWidth() > table->getVirtualBody()->getViewport().w);

        table->setColumnWidth(column, 170);
        scene.canvas->updateCanvas();
        CHECK(table->getColumnWidth(column) == 170);

        size_t rows=0;
        std::vector<std::string> wrong;

        for(controls::Base *control : table->getVirtualBody()->getRows())
        {
            auto *row=gweni_cast<controls::TreeTableRow>(control);

            REQUIRE(row != nullptr);
            if(row->hidden())
                continue;
            ++rows;
            for(int c=0; c < column; ++c)
            {
                std::vector<controls::Base *> cells;

                collect(row->getCell(c), cells);
                for(controls::Base *cell : cells)
                {
                    if(cell->getStateChange() & controls::StateChange_Geometry)
                        wrong.push_back(std::string("marked left of the column: ")+cell->getTypeName()
                            +" in column "+std::to_string(c));
                }
            }
            if(!(row->getCell(column)->getStateChange() & controls::StateChange_Geometry))
                wrong.push_back("the resized cell not marked");
        }
        CHECK(rows > 0);
        CHECK(wrong.empty());
        for(const std::string &line : wrong)
            MESSAGE(line);
        scene.canvas->renderCanvas();
    }
}

TEST_CASE("TreeTable: a resting mouse is hit-tested again when its row is bound to another node")
{
    Scene scene;
    TreeTable *table=scene.table;

    // a root holding parents, then leaf roots: collapsed, the rows below
    // the root show the leaves, so the viewport stays full
    table->addNode(Root, 1);
    for(NodeId id=2; id < 22; ++id)
    {
        table->addNode(1, id);
        table->addNode(id, id+100);
    }
    for(NodeId id=300; id < 320; ++id)
        table->addNode(Root, id);
    table->setExpanded(1, true);
    scene.settle();

    controls::TreeTableRow *row=scene.row(1);
    REQUIRE(row != nullptr);
    REQUIRE(table->nodeForItem(1) == 2);
    controls::internal::TreeTableExpander *expander=row->getExpander();
    REQUIRE_FALSE(expander->hidden());

    const Point at=Scene::centreOf(expander);
    scene.canvas->inputMouseMoved(at.x, at.y, 0, 0);
    scene.frame();
    REQUIRE(HoveredControl == expander);

    // Left on the selected root collapses it: the row under the mouse now
    // shows a leaf, bound in place, its expander hidden
    REQUIRE(table->setSelectedNode(1));
    table->focus();
    scene.key(keys::Left);
    REQUIRE_FALSE(table->isExpanded(1));
    REQUIRE(scene.row(1) == row);
    REQUIRE(table->nodeForItem(1) == 300);
    CHECK(expander->hidden());
    CHECK(HoveredControl != expander);
    CHECK(HoveredControl != nullptr);

    // a click there without moving selects the leaf
    scene.clock+=1.0f;
    input::onMouseClicked(scene.canvas, 0, true);
    scene.release();
    scene.frame();
    CHECK(table->getSelectedNode() == 300);
}
