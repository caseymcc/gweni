/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  ListBox keeps its selection in m_selectedRows, and taking a row out or
 *  selecting one again must keep that list true (#489): removeItem of a row
 *  that is not selected removes it from the table and leaves the selection as
 *  it was (it erased the list's end(), freeing its sentinel, and aborted);
 *  removeItem of a selected row in a multi-selection takes it out of
 *  getSelectedRows(), getSelectedRow() then being the next selected row, or
 *  nullptr once none is left; and setSelectedRow(row, false) or
 *  selectByString(name, false) on a row already selected keeps it in the
 *  selection once (it was pushed again, and a removeItem left the copy behind,
 *  pointing at the deleted row), still firing onRowSelectedCaller.
 */
#include "headlessFixture.h"

#include <gweni/controls/layout/table.h>
#include <gweni/controls/listBox.h>
#include <gweni/gweni.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace gweni;
using controls::ListBox;
using controls::layout::TableRow;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    ListBox *list;
    std::vector<TableRow *> rows;

    explicit Scene(int count)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        list=canvas->newChild<ListBox>();
        list->setBounds(10, 10, 200, 280);
        list->setAllowMultiSelect(true);

        for(int i=0; i<count; ++i)
            rows.push_back(list->addItem("Row "+std::to_string(i), "row"+std::to_string(i)));

        frame();
    }

    ~Scene()
    {
        // the hovered control is process-global
        HoveredControl=nullptr;
        delete canvas;
    }

    // the update phase runs the delayed deletes Table::remove queues
    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    bool inTable(TableRow *row)
    {
        controls::Base::List &children=list->getTable()->getChildren();
        return std::find(children.begin(), children.end(), row) != children.end();
    }

    size_t timesSelected(TableRow *row)
    {
        const ListBox::Rows &selected=list->getSelectedRows();
        return (size_t)std::count(selected.begin(), selected.end(), row);
    }

    std::vector<TableRow *> selected()
    {
        const ListBox::Rows &rows=list->getSelectedRows();
        return std::vector<TableRow *>(rows.begin(), rows.end());
    }
};

}//namespace

TEST_CASE("ListBox removeItem: a row that is not selected leaves the selection as it was")
{
    Scene scene(4);
    scene.list->setSelectedRow(scene.rows[1]);
    REQUIRE(scene.selected() == std::vector<TableRow *>{scene.rows[1]});

    scene.list->removeItem(scene.rows[2]);
    CHECK(scene.selected() == std::vector<TableRow *>{scene.rows[1]});

    scene.frame();
    CHECK_FALSE(scene.inTable(scene.rows[2]));
    CHECK(scene.list->getTable()->numChildren() == 3);
    CHECK(scene.list->getSelectedRow() == scene.rows[1]);

    // with nothing selected at all
    scene.list->unselectAll();
    scene.list->removeItem(scene.rows[0]);
    scene.frame();
    CHECK_FALSE(scene.inTable(scene.rows[0]));
    CHECK(scene.list->getSelectedRows().empty());
    CHECK(scene.list->getSelectedRow() == nullptr);
}

TEST_CASE("ListBox removeItem: a selected row leaves a multi-selection")
{
    Scene scene(4);
    scene.list->setSelectedRow(scene.rows[0]);
    scene.list->setSelectedRow(scene.rows[2], false);
    scene.list->setSelectedRow(scene.rows[3], false);
    REQUIRE(scene.selected() == std::vector<TableRow *>{scene.rows[0], scene.rows[2], scene.rows[3]});

    scene.list->removeItem(scene.rows[0]);
    CHECK(scene.selected() == std::vector<TableRow *>{scene.rows[2], scene.rows[3]});
    CHECK(scene.list->getSelectedRow() == scene.rows[2]);
    scene.frame();
    CHECK_FALSE(scene.inTable(scene.rows[0]));

    scene.list->removeItem(scene.rows[2]);
    CHECK(scene.list->getSelectedRow() == scene.rows[3]);
    scene.list->removeItem(scene.rows[3]);
    CHECK(scene.list->getSelectedRows().empty());
    CHECK(scene.list->getSelectedRow() == nullptr);

    scene.frame();
    CHECK(scene.list->getTable()->numChildren() == 1);
    CHECK(scene.inTable(scene.rows[1]));
}

TEST_CASE("ListBox selection: a row selected again is in it once")
{
    Scene scene(4);
    int selectedCalls=0;
    scene.list->onRowSelectedCaller.addCallback(scene.canvas,
        [&selectedCalls](event::Handler &, event::Info) { ++selectedCalls; });

    SUBCASE("through setSelectedRow")
    {
        scene.list->setSelectedRow(scene.rows[1]);
        scene.list->setSelectedRow(scene.rows[2], false);
        scene.list->setSelectedRow(scene.rows[1], false);
        CHECK(selectedCalls == 3);
    }

    SUBCASE("through selectByString")
    {
        scene.list->selectByString("Row 1");
        scene.list->selectByString("Row 2", false);
        scene.list->selectByString("Row 1", false);
        CHECK(selectedCalls == 3);
    }

    CHECK(scene.timesSelected(scene.rows[1]) == 1);
    CHECK(scene.timesSelected(scene.rows[2]) == 1);
    CHECK(scene.list->getSelectedRows().size() == 2);
    CHECK(static_cast<controls::ListBoxRow *>(scene.rows[1])->isSelected());

    scene.list->removeItem(scene.rows[1]);
    CHECK(scene.timesSelected(scene.rows[1]) == 0);
    CHECK(scene.selected() == std::vector<TableRow *>{scene.rows[2]});

    scene.frame();
    CHECK_FALSE(scene.inTable(scene.rows[1]));
    CHECK(scene.list->getSelectedRow() == scene.rows[2]);
}
