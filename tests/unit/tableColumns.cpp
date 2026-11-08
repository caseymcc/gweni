/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  layout::Table sizes its auto columns (no setColumnWidth, or 0) from their
 *  content: with `free` the table's width less its fixed columns and C the
 *  sum of the auto columns' content widths (each the widest cell's Label
 *  preferred width, its text measure and padding), an auto column is its
 *  content plus an even share of free-C when C <= free, floor(content*free/C)
 *  when C > free, and 0 when free <= 0. A fixed column keeps its width. What
 *  the integer division leaves goes one unit each to the auto columns, lowest
 *  index first, so the columns add up to the table's width exactly. Each
 *  cell's Label is placed at its column's width, side by side from the row's
 *  left edge.
 *
 *  After the first frame a cell's new text, a new row and a removed row
 *  re-size the columns of every row, the clean ones included, in that frame's
 *  one layout pass (nothing re-dirtied, layout verification finding nothing),
 *  and a column whose widest cell got narrower or went away shrinks back:
 *  the Labels' preferred widths stay their content's, whatever width the
 *  Table gave them last. StubRenderer's measureText is renderer::Base's,
 *  0.4*size per byte, so the widths are known here.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
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

const int Columns=3;

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        HoveredControl=nullptr;
        delete canvas;
    }

    controls::LayoutStats frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i<10; ++i)
        {
            if(frame().passes == 0)
                break;
        }
    }

    //! A table of `width` on the canvas, sized by its request.
    Table *table(int width)
    {
        Table *table=canvas->newChild<Table>();
        table->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        table->setBounds(10, 10, width, 200);
        table->setColumnCount(Columns);
        return table;
    }

    TableRow *row(Table *table, const char *a, const char *b, const char *c)
    {
        TableRow *row=table->addRow();
        row->setCellText(0, a);
        row->setCellText(1, b);
        row->setCellText(2, c);
        return row;
    }
};

//! What a cell holding `text` takes: the text's measure and the Label's 3+3
//! padding.
int cellWidth(test::StubRenderer &renderer, controls::Label *label, const String &text)
{
    return renderer.measureText(label->getFont(), text).x+
        label->getPadding().left+label->getPadding().right;
}

//! The widest cell of each column over the table's rows, measured here.
std::vector<int> contentWidths(test::StubRenderer &renderer, Table *table)
{
    std::vector<int> content(Columns, 0);

    for(auto &&child : table->getChildren())
    {
        TableRow *row=gweni_cast<TableRow>(child);

        if(!row || row->hidden())
            continue;

        for(int i=0; i<Columns; ++i)
        {
            controls::Label *label=row->getCellContents(i);
            content[i]=std::max(content[i], cellWidth(renderer, label, label->getText()));
        }
    }
    return content;
}

//! The column rule as the task states it, worked out independently of the
//! Table: fixed[i] > 0 is a fixed column.
std::vector<int> expectedWidths(const std::vector<int> &content, const std::vector<int> &fixed, int width)
{
    std::vector<int> result(Columns, 0);
    int free=width;
    int autoColumns=0;
    int sum=0;

    for(int i=0; i<Columns; ++i)
    {
        if(fixed[i] > 0)
        {
            result[i]=fixed[i];
            free-=fixed[i];
        }
        else
        {
            ++autoColumns;
            sum+=content[i];
        }
    }

    if(free <= 0 || autoColumns == 0)
        return result;

    int given=0;

    for(int i=0; i<Columns; ++i)
    {
        if(fixed[i] > 0)
            continue;

        if(sum <= free)
            result[i]=content[i]+(free-sum)/autoColumns;
        else
            result[i]=content[i]*free/sum;
        given+=result[i];
    }

    for(int i=0; i<Columns && given < free; ++i)
    {
        if(fixed[i] <= 0)
        {
            ++result[i];
            ++given;
        }
    }
    return result;
}

//! Every shown row hands each column the same width, and its Labels sit at
//! those widths side by side from the row's left edge.
std::vector<int> columnWidths(Table *table)
{
    std::vector<int> widths;
    bool first=true;

    for(auto &&child : table->getChildren())
    {
        TableRow *row=gweni_cast<TableRow>(child);

        if(!row || row->hidden())
            continue;

        int x=row->getPadding().left;

        for(int i=0; i<Columns; ++i)
        {
            controls::Label *label=row->getCellContents(i);

            INFO("row ", row->getText(0), ", column ", i);
            CHECK(label->getX() == x);
            CHECK(label->getWidth() == row->getColumnWidth(i));
            x+=label->getWidth();

            if(first)
                widths.push_back(label->getWidth());
            else
                CHECK(label->getWidth() == widths[i]);
        }
        first=false;
    }
    return widths;
}

int sum(const std::vector<int> &values)
{
    int total=0;
    for(int v : values)
        total+=v;
    return total;
}

}//namespace

TEST_CASE("Table columns: the cells measure their text and padding")
{
    Scene scene;
    Table *table=scene.table(201);
    scene.row(table, "Baked Beans", "Heinz", "$3.50");
    scene.settle();

    // 0.4*11 per byte: 48, 22 and 22, and 3+3 padding each
    CHECK(contentWidths(scene.renderer, table) == std::vector<int>{54, 28, 28});

    for(int i=0; i<Columns; ++i)
    {
        TableRow *row=table->getRow(0);
        CHECK(row->getColumnContentWidth(i) == contentWidths(scene.renderer, table)[i]);
    }
}

TEST_CASE("Table columns: content that fits gets its width and an even share, the rest lowest index first")
{
    Scene scene;
    Table *table=scene.table(201);
    scene.row(table, "Baked Beans", "Heinz", "$3.50");
    scene.row(table, "Bananas", "Trees", "$1.27");
    scene.settle();

    // C = 54+28+28 = 110 of 201: 91 more, 30 each and 1 left for column 0
    const std::vector<int> widths=columnWidths(table);
    CHECK(widths == std::vector<int>{85, 58, 58});
    CHECK(sum(widths) == table->getWidth());

    // the old even split dropped the remainder; every width here divides
    // unevenly by 3
    for(int width : {150, 152, 199, 200, 110, 111})
    {
        table->setBounds(10, 10, width, 200);
        scene.settle();

        const std::vector<int> content=contentWidths(scene.renderer, table);
        const std::vector<int> found=columnWidths(table);

        INFO("table width ", width);
        CHECK(found == expectedWidths(content, {0, 0, 0}, width));
        CHECK(sum(found) == width);

        for(int i=0; i<Columns; ++i)
            CHECK(found[i] >= content[i]);
    }
}

TEST_CASE("Table columns: content wider than the table shrinks in proportion")
{
    Scene scene;
    Table *table=scene.table(80);
    scene.row(table, "Baked Beans", "Heinz", "$3.50");
    scene.settle();

    // C = 110 of 80: floor(54*80/110)=39, floor(28*80/110)=20 twice, 1 left
    const std::vector<int> widths=columnWidths(table);
    CHECK(widths == std::vector<int>{40, 20, 20});
    CHECK(sum(widths) == 80);

    for(int width : {109, 100, 55, 3, 1})
    {
        table->setBounds(10, 10, width, 200);
        scene.settle();

        const std::vector<int> content=contentWidths(scene.renderer, table);
        const std::vector<int> found=columnWidths(table);

        INFO("table width ", width);
        CHECK(found == expectedWidths(content, {0, 0, 0}, width));
        CHECK(sum(found) == width);

        // each is cut by the same fraction, to within the unit it rounds by
        for(int i=0; i<Columns; ++i)
        {
            const double exact=double(content[i])*width/sum(content);
            CHECK(found[i] >= int(exact));
            CHECK(found[i] <= int(exact)+1);
        }
    }
}

TEST_CASE("Table columns: fixed columns keep their width, the auto ones share the rest")
{
    Scene scene;
    Table *table=scene.table(201);
    scene.row(table, "Baked Beans", "Heinz", "$3.50");
    table->setColumnWidth(1, 50);
    scene.settle();

    // 151 free for 54 and 28: 69 more, 34 each and 1 left for column 0
    std::vector<int> widths=columnWidths(table);
    CHECK(widths == std::vector<int>{89, 50, 62});
    CHECK(sum(widths) == 201);

    // 50 free for 82: floor(54*50/82)=32, floor(28*50/82)=17, 1 left
    table->setBounds(10, 10, 100, 200);
    scene.settle();
    widths=columnWidths(table);
    CHECK(widths == std::vector<int>{33, 50, 17});
    CHECK(sum(widths) == 100);

    // the fixed one alone fills it, and then is wider than it: never shrunk,
    // the auto ones are 0
    for(int width : {50, 30})
    {
        table->setBounds(10, 10, width, 200);
        scene.settle();

        INFO("table width ", width);
        CHECK(columnWidths(table) == std::vector<int>{0, 50, 0});
    }

    // every column fixed: nothing to share
    table->setBounds(10, 10, 201, 200);
    table->setColumnWidth(0, 60);
    table->setColumnWidth(2, 40);
    scene.settle();
    CHECK(columnWidths(table) == std::vector<int>{60, 50, 40});

    // back to auto
    table->setColumnWidth(1, 0);
    scene.settle();
    widths=columnWidths(table);
    CHECK(widths == expectedWidths(contentWidths(scene.renderer, table), {60, 0, 40}, 201));
    CHECK(sum(widths) == 201);
}

TEST_CASE("Table columns: edits after the first frame re-size every row in that frame's one pass")
{
    const bool verify=controls::layoutVerifyEnabled();
    controls::setLayoutVerify(true);
    const size_t verifyFailures=controls::layoutVerifyFailures();

    {
        Scene scene;
        controls::ListBox *list=scene.canvas->newChild<controls::ListBox>();
        list->setBounds(10, 10, 200, 200);
        list->setColumnCount(Columns);

        TableRow *beans=list->addItem("Baked Beans");
        beans->setCellText(1, "Heinz");
        beans->setCellText(2, "$3.50");
        TableRow *bananas=list->addItem("Bananas");
        bananas->setCellText(1, "Trees");
        bananas->setCellText(2, "$1.27");
        TableRow *chicken=list->addItem("Chicken");
        chicken->setCellText(1, "Farm");
        chicken->setCellText(2, "$8.95");

        Table *table=list->getTable();
        scene.settle();

        const int width=table->getWidth();
        REQUIRE(width > 0);

        // The columns as the rule gives them for what the cells hold now.
        auto ruled=[&](const char *step)
        {
            INFO(step);
            CHECK(table->getWidth() == width);

            const std::vector<int> found=columnWidths(table);
            CHECK(found == expectedWidths(contentWidths(scene.renderer, table), {0, 0, 0}, width));
            CHECK(sum(found) == width);
            return found;
        };

        // ... laid out by this frame in one pass, and the next frame has
        // nothing to do.
        auto check=[&](const char *step, const controls::LayoutStats &stats)
        {
            INFO(step);
            CHECK(stats.passes == 1);
            CHECK_MESSAGE(!stats.redirtiedType, "re-dirtied by ",
                (stats.redirtiedType ? stats.redirtiedType : ""));
            CHECK(!stats.renderLayout);

            const std::vector<int> found=ruled(step);
            CHECK(scene.frame().passes == 0);
            return found;
        };

        const std::vector<int> start=ruled("settled");

        // a wider cell in one row: its column grows in every row
        bananas->setCellText(1, "Rainforest trees");
        const std::vector<int> wider=check("a wider cell", scene.frame());
        CHECK(wider[1] > start[1]);
        CHECK(wider[0] < start[0]);

        // narrower again: the column shrinks back to where it was
        bananas->setCellText(1, "Trees");
        CHECK(check("narrower again", scene.frame()) == start);

        // a new row wider than every other in column 0
        TableRow *widest=list->addItem("Baked Beans in tomato sauce");
        widest->setCellText(1, "Heinz");
        widest->setCellText(2, "$9");
        const std::vector<int> added=check("a wider row added", scene.frame());
        CHECK(added[0] > start[0]);

        // the widest row removed: column 0 shrinks back. The removal is a
        // delayedDelete, processed at the start of the next frame.
        table->remove(widest);
        CHECK(check("the widest row removed", scene.frame()) == start);
    }

    CHECK(controls::layoutVerifyFailures() == verifyFailures);
    controls::setLayoutVerify(verify);
}
