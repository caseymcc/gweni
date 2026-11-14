/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  DataTable reordering, driven through the input handler: two tables side
 *  by side (2 columns, 6 rows or 1 000 items each), the right one the other
 *  member of the left one's ReorderGroup.
 *
 *  Row mode (setRowsReorderable): a press on a row moved 5 units drags it,
 *  keyed by its control; a drop reports its indexes and the app applying it
 *  with setRowOrder shows the new order. While the table has a sort column it
 *  takes no row of its own (no gap, the No cursor, a release cancels), but a
 *  row dragged out of it drops on the other table. Source mode
 *  (setItemsReorderable): a pooled row drags its item by the app's key, a
 *  drop reports the indexes, a rebind() after the app reordered its records
 *  during the drag looks the item up again, and a sorted table refuses its own
 *  items likewise. With reordering on, a header click goes ascending,
 *  descending, then no sort; without, it alternates as before.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/dataTable.h>
#include <gweni/controls/reorderable.h>
#include <gweni/dragAndDrop.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace gweni;
using controls::DataTable;
using controls::DataTableRow;
using controls::ItemDrop;
using controls::Reorderable;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;
    DataTable *left=nullptr;
    DataTable *right=nullptr;
    controls::ReorderGroup group;
    std::vector<ItemDrop> leftDrops;
    std::vector<ItemDrop> rightDrops;
    std::vector<uint64_t> keys;

    //! Two 180-wide tables on a 400x300 canvas, docked left and right; in
    //! row mode 6 rows each, in source mode 1 000 items (keys 7000 on) each.
    explicit Scene(bool sourceMode)
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        left=table(DockPosition::Left, sourceMode, leftDrops);
        right=table(DockPosition::Right, sourceMode, rightDrops);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        draganddrop::cancel();
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    DataTable *table(DockPosition dock, bool sourceMode, std::vector<ItemDrop> &drops)
    {
        DataTable *table=canvas->newChild<DataTable>();
        table->setDock(dock);
        table->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
        table->setWidth(180);
        table->setColumnCount(2);
        table->setColumnCaption(0, "Name");
        table->setColumnCaption(1, "Size");

        Reorderable *reorder=nullptr;

        if(sourceMode)
        {
            if(keys.empty())
            {
                for(uint64_t i=0; i < 1000; ++i)
                    keys.push_back(7000+i);
            }
            table->setItemCount(keys.size());
            table->setRowBinder([this](DataTableRow *row, size_t item) {
                row->setCellText(0, std::to_string(keys[item]));
            });
            reorder=table->setItemsReorderable([this](size_t index) { return keys[index]; },
                [this](uint64_t key) {
                    auto found=std::find(keys.begin(), keys.end(), key);
                    return found == keys.end() ? DataTable::NoItem : static_cast<size_t>(found-keys.begin());
                });
        }
        else
        {
            for(int i=0; i < 6; ++i)
                table->addRow()->setCellText(0, "row "+std::to_string(i));
            reorder=table->setRowsReorderable(true);
        }

        REQUIRE(reorder != nullptr);
        group.add(reorder);
        reorder->setOnItemDropped([&drops](const ItemDrop &drop) { drops.push_back(drop); });
        return table;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void move(Point to)
    {
        canvas->inputMouseMoved(to.x, to.y, 0, 0);
        frame();
    }

    static Point centre(controls::Base *control, int y=-1)
    {
        return control->localPosToCanvas(Point(control->getWidth()/2, y < 0 ? control->getHeight()/2 : y));
    }

    void click(Point at)
    {
        clock+=1.0f;
        move(at);
        input::onMouseClicked(canvas, 0, true);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    //! Presses \p row a quarter of the way down and moves 6 units right
    void startDrag(controls::Base *row)
    {
        clock+=1.0f;
        const Point at=centre(row, row->getHeight()/4);
        move(at);
        input::onMouseClicked(canvas, 0, true);
        frame();
        move(Point(at.x+6, at.y));
    }

    void release()
    {
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    controls::Base *headerCell(DataTable *table, int column) const
    {
        return table->getHeader()->getCells().at(static_cast<size_t>(column));
    }
};

}//namespace

TEST_CASE("DataTable reorder: a row-mode row drags by its control, and setRowOrder applies the drop")
{
    Scene scene(false);
    const std::vector<DataTableRow *> rows=scene.left->getRows();

    scene.startDrag(rows[1]);
    REQUIRE(draganddrop::isDragging());
    CHECK(Reorderable::dragSource() == scene.left->getReorderable());
    CHECK(scene.left->getReorderable()->dragKey() == controls::CardReorder::keyOf(rows[1]));

    // below row 3: gap 4, index 3 once row 1 has left
    scene.move(Scene::centre(rows[3], rows[3]->getHeight()-2));
    CHECK(scene.left->getReorderable()->gap() == 4);
    scene.release();

    REQUIRE(scene.leftDrops.size() == 1);
    const ItemDrop &drop=scene.leftDrops[0];
    CHECK(drop.sourceIndex == 1);
    CHECK(drop.targetIndex == 3);
    // the table moved nothing; the app does
    CHECK(scene.left->getRows() == rows);

    std::vector<DataTableRow *> order=rows;
    order.erase(order.begin()+1);
    order.insert(order.begin()+3, rows[1]);
    scene.left->setRowOrder(order);
    scene.frame();
    CHECK(scene.left->getRows() == order);
    CHECK(rows[1]->getY() > rows[3]->getY());
}

TEST_CASE("DataTable reorder: a sorted table takes no row of its own, but its rows drop elsewhere")
{
    SUBCASE("row mode")
    {
        Scene scene(false);
        scene.left->setSortColumn(0, controls::SortDirection::Ascending, false);
        const std::vector<DataTableRow *> rows=scene.left->getRows();

        scene.startDrag(rows[0]);
        REQUIRE(draganddrop::isDragging());
        scene.move(Scene::centre(rows[4]));
        CHECK(Reorderable::dropTarget() == nullptr);
        CHECK(scene.left->getReorderable()->getIndicator() == nullptr);
        CHECK(draganddrop::currentCursor() == cursor::No);
        scene.release();
        CHECK(scene.leftDrops.empty());

        // out of it, onto the other table
        scene.startDrag(rows[2]);
        scene.move(Scene::centre(scene.right->getRows()[1], 2));
        CHECK(Reorderable::dropTarget() == scene.right->getReorderable());
        scene.release();
        REQUIRE(scene.rightDrops.size() == 1);
        CHECK(scene.rightDrops[0].source == scene.left->getReorderable());
        CHECK(scene.rightDrops[0].key == controls::CardReorder::keyOf(rows[2]));
        CHECK(scene.rightDrops[0].sourceIndex == 2);
        CHECK(scene.rightDrops[0].targetIndex == 1);
    }

    SUBCASE("source mode")
    {
        Scene scene(true);
        scene.left->setSortColumn(1, controls::SortDirection::Descending, false);

        scene.startDrag(scene.left->rowForItem(2));
        REQUIRE(draganddrop::isDragging());
        scene.move(Scene::centre(scene.left->rowForItem(6)));
        CHECK(Reorderable::dropTarget() == nullptr);
        CHECK(draganddrop::currentCursor() == cursor::No);
        scene.release();
        CHECK(scene.leftDrops.empty());

        // unsorted again, the same drag reorders it
        scene.left->setSortColumn(-1, controls::SortDirection::None, false);
        scene.startDrag(scene.left->rowForItem(2));
        scene.move(Scene::centre(scene.left->rowForItem(6), 2));
        CHECK(Reorderable::dropTarget() == scene.left->getReorderable());
        scene.release();
        REQUIRE(scene.leftDrops.size() == 1);
        CHECK(scene.leftDrops[0].key == 7002);
        CHECK(scene.leftDrops[0].targetIndex == 5);
    }
}

TEST_CASE("DataTable reorder: a source-mode item drags by the app's key and rebind() looks it up again")
{
    Scene scene(true);

    scene.startDrag(scene.left->rowForItem(3));
    REQUIRE(draganddrop::isDragging());
    Reorderable *reorder=scene.left->getReorderable();
    CHECK(reorder->dragKey() == 7003);
    CHECK(reorder->dragIndex() == 3);

    // the app puts that record first and says so
    scene.keys.erase(scene.keys.begin()+3);
    scene.keys.insert(scene.keys.begin(), 7003);
    scene.left->rebind();
    scene.frame();
    CHECK(draganddrop::isDragging());
    CHECK(reorder->dragIndex() == 0);

    scene.move(Scene::centre(scene.left->rowForItem(5), 2));
    scene.release();
    REQUIRE(scene.leftDrops.size() == 1);
    CHECK(scene.leftDrops[0].key == 7003);
    CHECK(scene.leftDrops[0].sourceIndex == 0);
    CHECK(scene.leftDrops[0].targetIndex == 4);
}

TEST_CASE("DataTable reorder: with reordering on a header click goes ascending, descending, then no sort")
{
    Scene scene(false);
    controls::Base *name=scene.headerCell(scene.left, 0);

    scene.click(Scene::centre(name));
    CHECK(scene.left->getSortColumn() == 0);
    CHECK(scene.left->getSortDirection() == controls::SortDirection::Ascending);
    scene.click(Scene::centre(name));
    CHECK(scene.left->getSortDirection() == controls::SortDirection::Descending);
    scene.click(Scene::centre(name));
    CHECK(scene.left->getSortColumn() == -1);
    CHECK(scene.left->getSortDirection() == controls::SortDirection::None);
    scene.click(Scene::centre(name));
    CHECK(scene.left->getSortDirection() == controls::SortDirection::Ascending);

    // a table without reordering alternates as before
    scene.right->setRowsReorderable(false);
    CHECK_FALSE(scene.right->isReorderable());
    controls::Base *other=scene.headerCell(scene.right, 0);
    scene.click(Scene::centre(other));
    scene.click(Scene::centre(other));
    scene.click(Scene::centre(other));
    CHECK(scene.right->getSortColumn() == 0);
    CHECK(scene.right->getSortDirection() == controls::SortDirection::Ascending);
}
