/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  layout::Table::getControlAt binary-searches its rows instead of trying
 *  every one, and must find exactly what Base::getControlAt's walk finds (the
 *  same table, through a qualified call) at every point over and around it,
 *  with and without bOnlyIfMouseEnabled: before the first layout; laid out
 *  with rows of mixed heights, gaps between them and room past the last;
 *  with rows hidden, with and without a layout since; with zero-height rows;
 *  with rows added, deleted outright or with delayedDelete(), and moved by
 *  setPos() since the last layout; with cell contents that do and do not take
 *  the mouse and a table that does not; and with a child that is not docked
 *  Top, which makes it walk. The search is checked to be the path taken where
 *  it should be (a hit test of a laid-out table visits a few controls, not one
 *  per row) and the walk where it should not.
 *
 *  input::hitTestVisits() counts one visit per getControlAt call: a mouse
 *  move onto a row of a laid-out ListBox costs the same number of visits at
 *  1 000 and 10 000 rows, a few dozen at most, and hovers that row.
 */
#include "headlessFixture.h"

#include <gweni/controls/layout/table.h>
#include <gweni/controls/listBox.h>
#include <gweni/inputHandler.h>
#include <gweni/gweni.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using controls::layout::Table;
using controls::layout::TableRow;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    Table *table;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 1000);
        table=canvas->newChild<Table>();
        table->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        table->setBounds(10, 10, 200, 960);
    }

    ~Scene()
    {
        // the hovered control is process-global
        HoveredControl=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int i=0; i<10; ++i)
        {
            frame();
            if(canvas->getLayoutStats().passes == 0)
                break;
        }
    }

    //! Rows of mixed heights (their vertical padding) with gaps between some
    //! of them (their margins).
    std::vector<TableRow *> addRows(int count)
    {
        std::vector<TableRow *> rows;

        for(int i=0; i<count; ++i)
        {
            TableRow *row=table->addRow();
            row->setCellText(0, "Row "+std::to_string(i));
            row->setPadding(Padding(0, i%3, 0, 2*(i%2)));
            if(i%4 == 1)
                row->setMargin(Margin(0, 3, 0, 2));
            rows.push_back(row);
        }
        return rows;
    }
};

//! Base::getControlAt on the same table: the walk over every row.
controls::Base *walk(Table *table, int x, int y, bool onlyIfMouseEnabled)
{
    return table->controls::Base::getControlAt(x, y, onlyIfMouseEnabled);
}

//! Compares the table's own hit test with the walk at every row of the
//! table, and a few beyond each edge, across a set of columns.
void checkSameAsWalk(Table *table, const char *state)
{
    INFO(state);

    const int w=table->getWidth();
    const int h=table->getHeight();
    const int xs[]={-3, -1, 0, 1, w/2, w-1, w, w+3};
    size_t mismatches=0;
    std::string first;

    for(bool onlyIfMouseEnabled : {true, false})
    {
        for(int y=-5; y<h+5; ++y)
        {
            for(int x : xs)
            {
                controls::Base *expected=walk(table, x, y, onlyIfMouseEnabled);
                controls::Base *found=table->getControlAt(x, y, onlyIfMouseEnabled);

                if(found == expected)
                    continue;

                if(mismatches++ == 0)
                {
                    first="at "+std::to_string(x)+","+std::to_string(y)+
                        (onlyIfMouseEnabled ? " (mouse-enabled only)" : "")+": walk "+
                        (expected ? expected->getTypeName() : "nullptr")+", table "+
                        (found ? found->getTypeName() : "nullptr");
                }
            }
        }
    }

    INFO(first);
    CHECK(mismatches == 0);
}

//! The hit-test visits of one table->getControlAt at the middle of \p row.
size_t visitsAt(Table *table, controls::Base *row)
{
    const size_t before=input::hitTestVisits();
    table->getControlAt(row->getX()+row->getWidth()/2, row->getY()+row->getHeight()/2);
    return input::hitTestVisits()-before;
}

//! The search is the path taken: a hit on the second row costs what one on
//! a late row does, far fewer visits than the walk's one per row after it.
void checkSearches(Table *table, controls::Base *early, controls::Base *late)
{
    const size_t visits=visitsAt(table, early);
    CHECK(visits == visitsAt(table, late));
    CHECK(visits < 20);
}

//! The walk is the path taken: a hit on the second row of 40 tries every
//! row after it first.
void checkWalks(Table *table, controls::Base *early)
{
    CHECK(visitsAt(table, early) >= 39);
}

}//namespace

TEST_CASE("Table hit test: before the first layout it walks, and finds what the walk finds")
{
    Scene scene;
    std::vector<TableRow *> rows=scene.addRows(40);

    REQUIRE(scene.table->needsLayout());
    checkSameAsWalk(scene.table, "before the first layout");
}

TEST_CASE("Table hit test: laid out, mixed heights, gaps and room past the last row")
{
    Scene scene;
    std::vector<TableRow *> rows=scene.addRows(40);
    scene.settle();

    // the state is what it says: heights vary, some rows have a gap above,
    // the table goes on past the last row
    bool mixed=false, gap=false;
    for(size_t i=1; i<rows.size(); ++i)
    {
        mixed|=rows[i]->getHeight() != rows[0]->getHeight();
        gap|=rows[i]->getY() > rows[i-1]->getY()+rows[i-1]->getHeight();
    }
    REQUIRE(mixed);
    REQUIRE(gap);
    REQUIRE(rows.back()->getY()+rows.back()->getHeight() < scene.table->getHeight());

    checkSameAsWalk(scene.table, "laid out");

    // the search: the table, the row and what is under it, whatever the count
    checkSearches(scene.table, rows[1], rows[38]);
}

TEST_CASE("Table hit test: hidden rows, with and without a layout since")
{
    Scene scene;
    std::vector<TableRow *> rows=scene.addRows(40);
    scene.settle();

    rows[0]->hide();
    rows[7]->hide();
    rows[39]->hide();
    checkSameAsWalk(scene.table, "hidden, no layout since");

    scene.settle();
    checkSameAsWalk(scene.table, "hidden, laid out");
    checkSearches(scene.table, rows[1], rows[35]);

    // shown again: the last row comes back where it was and nothing else
    // moves, so only its own flag says the stack changed
    rows[39]->show();
    checkSameAsWalk(scene.table, "last row shown, no layout since");
    scene.settle();
    checkSameAsWalk(scene.table, "last row shown, laid out");

    rows[0]->show();
    rows[7]->show();
    scene.settle();
    checkSameAsWalk(scene.table, "all shown, laid out");
}

TEST_CASE("Table hit test: zero-height rows")
{
    Scene scene;
    std::vector<TableRow *> rows=scene.addRows(40);

    for(size_t i : {size_t(0), size_t(5), size_t(6), size_t(19)})
    {
        rows[i]->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
        rows[i]->setHeight(0);
    }
    scene.settle();

    REQUIRE(rows[5]->getHeight() == 0);
    REQUIRE(rows[6]->getY() == rows[7]->getY());
    checkSameAsWalk(scene.table, "zero-height rows");
    checkSearches(scene.table, rows[1], rows[35]);
}

TEST_CASE("Table hit test: rows added, deleted and moved since the last layout")
{
    Scene scene;
    std::vector<TableRow *> rows=scene.addRows(40);
    scene.settle();

    // a new row sits at its default bounds, over the first rows, until laid out
    scene.table->addRow()->setCellText(0, "added");
    checkSameAsWalk(scene.table, "added, no layout since");
    scene.settle();
    checkSameAsWalk(scene.table, "added, laid out");

    delete rows[3];
    checkSameAsWalk(scene.table, "deleted outright, no layout since");
    scene.settle();
    checkSameAsWalk(scene.table, "deleted outright, laid out");

    scene.table->remove(rows[10]);
    checkSameAsWalk(scene.table, "delayedDelete, not yet processed");
    scene.frame();
    checkSameAsWalk(scene.table, "delayedDelete, processed");
    scene.settle();
    checkSameAsWalk(scene.table, "delayedDelete, laid out");

    // moved sideways: still a stack
    rows[12]->setPos(40, rows[12]->getY());
    checkSameAsWalk(scene.table, "moved sideways");
    checkSearches(scene.table, rows[1], rows[35]);

    // moved over its neighbour: not a stack any more, so it walks
    rows[20]->setPos(0, rows[19]->getY()+2);
    checkSameAsWalk(scene.table, "moved over a neighbour");
    checkWalks(scene.table, rows[1]);

    // moved past the end, out of child order
    rows[21]->setPos(0, scene.table->getHeight()-10);
    checkSameAsWalk(scene.table, "moved past the end");

    scene.settle();
    checkSameAsWalk(scene.table, "moved, laid out again");
    checkSearches(scene.table, rows[1], rows[35]);
}

TEST_CASE("Table hit test: cell contents with and without the mouse, a table without it")
{
    Scene scene;
    std::vector<TableRow *> rows=scene.addRows(40);

    for(size_t i : {size_t(4), size_t(9)})
    {
        controls::Base *content=scene.canvas->newChild<controls::Base>();
        content->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        content->setBounds(0, 0, 30, 8);
        rows[i]->setCellContents(0, content, i == 9);
    }
    rows[14]->setMouseInputEnabled(false);
    scene.settle();

    checkSameAsWalk(scene.table, "cell contents");

    scene.table->setMouseInputEnabled(false);
    checkSameAsWalk(scene.table, "table not mouse-enabled");

    scene.table->hide();
    checkSameAsWalk(scene.table, "table hidden");
}

TEST_CASE("Table hit test: a child not docked Top makes it walk")
{
    Scene scene;
    std::vector<TableRow *> rows=scene.addRows(40);

    controls::Base *floating=scene.table->newChild<controls::Base>();
    floating->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    floating->setBounds(20, 30, 60, 100);
    scene.settle();

    REQUIRE((floating->getDock() != DockPosition::Top));
    checkSameAsWalk(scene.table, "a floating child");
    checkWalks(scene.table, rows[1]);
}

TEST_CASE("Hit test visits: a move over a ListBox costs the same at 1 000 and 10 000 rows")
{
    size_t visits[2]={};
    const int counts[2]={1000, 10000};

    for(int i=0; i<2; ++i)
    {
        test::StubRenderer renderer;
        test::StubSkin skin{&renderer};
        controls::Canvas *canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        controls::ListBox *list=canvas->newChild<controls::ListBox>();
        list->setBounds(10, 10, 200, 280);

        std::vector<controls::Base *> rows;
        for(int r=0; r<counts[i]; ++r)
            rows.push_back(list->addItem("Row "+std::to_string(r)));

        for(int f=0; f<10; ++f)
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
            if(canvas->getLayoutStats().passes == 0)
                break;
        }

        controls::Base *row=rows[3];
        const Point at=row->localPosToCanvas(Point(row->getWidth()/2, row->getHeight()/2));

        const size_t before=input::hitTestVisits();
        input::onMouseMoved(canvas, at.x, at.y, 0, 0);
        visits[i]=input::hitTestVisits()-before;

        // the row or a control inside it
        controls::Base *up=HoveredControl;
        while(up && up != row)
            up=up->getParent();
        CHECK(up == row);

        HoveredControl=nullptr;
        delete canvas;
    }

    CHECK(visits[0] > 0);
    CHECK(visits[0] <= 40);
    CHECK(visits[1] == visits[0]);
}
