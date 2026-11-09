/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  DataTable on a real Canvas, driven through the input handler. Columns take
 *  their widths, clamped to their minimums, and the last one stretches to the
 *  body's width (or not, setLastColumnStretch(false)); every row's cells sit
 *  exactly under their header cells. Dragging a header cell's right edge (its
 *  grip) resizes that column and does not sort, from the width it is laid out
 *  at, so the stretched last column's edge moves with the first unit of a drag;
 *  a click elsewhere on the cell sorts by it, ascending and then descending,
 *  its chevron alone showing, and onSortChanged fires once per click and never
 *  for a no-op. While the columns fit the body, a column drag moves only that
 *  column's cells and the following columns' (header and body): every other
 *  control keeps its bounds and has no StateChange_Geometry pending. Once
 *  they overflow it (scrolled to x=0), every row and the inner panel are as
 *  wide as the columns, so a new width changes their size at the same origin;
 *  the region they clip to within the viewport stays, so they are marked
 *  themselves and nothing in the cells left of the column is (Base::
 *  onBoundsChanged marks a size-only change's subtree only when that region
 *  changes). A control put in a cell takes the mouse as its own
 *  flag says: a Button's click presses it and selects nothing, a click on a
 *  Label or elsewhere on the cell selects the row. A sort click with the app re-ordering its rows, or setting their
 *  cells again, is laid out in one update-phase pass. The stripes follow the
 *  row order (setRowOrder, removeRow, setStriped) and a re-bind keeps them;
 *  selection goes through the table's SelectableGroup, one row at a time, and a
 *  removed or deleted selected row leaves none. The header stays put while the
 *  body scrolls down, and follows it sideways. On a recording StubRenderer the
 *  textured renderers fill a striped odd row and hide an idle even one, draw
 *  the sorted column's chevron up then down at the cell's z+1, draw the row and
 *  column rules with DataTableBorders::All, and draw nothing on a settled frame
 *  but, on a hover, the hovered row's slots.
 */
#include "headlessFixture.h"

#include <gweni/controls/dataTable.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! The draws the textured DataTable renderers make: which slot, the rect, its
//! colour (a surface's top, an icon's colour), an icon's kind and direction
//! (-1 for a rect) and the z.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        Rect rect;
        Color color;
        int kind;
        int direction;
        int zIndex;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float, float, Color top, Color, Color,
        int zIndex) override
    {
        draws.push_back({primitiveId, rect, top, -1, -1, zIndex});
    }

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color, int zIndex) override
    {
        draws.push_back({primitiveId, rect, color, kind, direction, zIndex});
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.push_back(primitiveId);
    }

    std::vector<Draw> draws;
    std::vector<size_t> hidden;
};

//! The textured skin's DataTable renderers, or a StubControl for every type.
class TableSkin: public test::StubSkin
{
public:
    TableSkin(test::StubRenderer *renderer, bool textured)
        : StubSkin(renderer), m_texturedSkin(renderer), m_textured(textured)
    {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        static const std::set<std::string> names={"DataTable", "DataTableHeader",
            "DataTableHeaderCell", "DataTableColumnGrip", "DataTableRow"};

        if(m_textured && names.count(typeName))
            return m_texturedSkin.getControlRenderer(typeName);
        return StubSkin::getControlRenderer(typeName);
    }

private:
    skin::textured::TexturedSkin m_texturedSkin;
    bool m_textured;
};

struct Scene
{
    DrawRecorder renderer;
    TableSkin skin;
    controls::Canvas *canvas=nullptr;
    controls::DataTable *table=nullptr;
    float clock=10.0f;

    explicit Scene(int columns, int rows, bool textured=false): skin(&renderer, textured)
    {
        // a second apart per click, so two clicks are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 400);

        table=canvas->newChild<controls::DataTable>();
        table->setBounds(10, 10, 400, 200);
        table->setColumnCount(columns);
        for(int c=0; c < columns; ++c)
            table->setColumnCaption(c, "Column "+std::to_string(c));
        for(int r=0; r < rows; ++r)
        {
            controls::DataTableRow *row=table->addRow();

            for(int c=0; c < columns; ++c)
                row->setCellText(c, "r"+std::to_string(r)+"c"+std::to_string(c));
        }
        frame();
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        renderer.draws.clear();
        renderer.hidden.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    static Point centre(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        return Point(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    void moveTo(const Point &at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
    }

    //! A whole left click at the control's centre, then a frame.
    void click(controls::Base *control)
    {
        clock+=1.0f;
        moveTo(centre(control));
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    //! Presses column's grip, moves the mouse dx across and lets go, with no
    //! frame between.
    void dragGrip(int column, int dx)
    {
        clock+=1.0f;
        const Point at=centre(cell(column)->getGrip());

        moveTo(at);
        canvas->inputMouseButton(0, true);
        moveTo(Point(at.x+dx, at.y));
        canvas->inputMouseButton(0, false);
    }

    controls::internal::DataTableHeaderCell *cell(int column) const
    {
        return table->getHeader()->getCells().at(column);
    }

    //! A control's bounds on the canvas.
    static Rect canvasBounds(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        return Rect(at.x, at.y, control->getWidth(), control->getHeight());
    }
};

//! Counts a Caller's calls and keeps the last info.
class Counter: public event::Handler
{
public:
    void count(event::Info info)
    {
        ++calls;
        last=info.control;
        point=info.point;
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
    controls::Base *last=nullptr;
    Point point;
};

void collect(controls::Base *control, std::vector<controls::Base *> &all)
{
    all.push_back(control);
    for(controls::Base *child : control->getActualChildren())
        collect(child, all);
}

bool under(const controls::Base *control, const controls::Base *root)
{
    for(; control; control=control->getParent())
    {
        if(control == root)
            return true;
    }
    return false;
}

}//namespace

TEST_CASE("DataTable: columns take their widths, clamped to the minimum, the last one stretching")
{
    Scene scene(3, 4);
    controls::DataTable *table=scene.table;

    table->setColumnWidth(0, 100);
    table->setColumnWidth(1, 80);
    scene.frame();

    // 400 wide less the table's 1-unit padding on both sides: no bar shows
    const int viewport=398;

    CHECK(table->getColumnWidth(0) == 100);
    CHECK(table->getColumnWidth(1) == 80);
    CHECK(table->getColumnWidth(2) == controls::DataTable::DefaultColumnWidth);
    CHECK(table->getColumnLaidOutWidth(0) == 100);
    CHECK(table->getColumnLaidOutWidth(1) == 80);
    CHECK(table->getColumnLaidOutWidth(2) == viewport-180);
    CHECK(table->contentWidth() == 280);

    // every row's cells sit under their header cells
    for(controls::DataTableRow *row : table->getRows())
    {
        for(int c=0; c < 3; ++c)
        {
            const Rect header=Scene::canvasBounds(scene.cell(c));
            const Rect body=Scene::canvasBounds(row->getCell(c));

            CHECK(header.x == body.x);
            CHECK(header.w == body.w);
        }
    }

    // below the minimum: the minimum
    table->setColumnWidth(1, 5);
    CHECK(table->getColumnWidth(1) == controls::DataTable::DefaultMinColumnWidth);

    // a new minimum above the width raises it
    table->setColumnMinWidth(0, 120);
    CHECK(table->getColumnWidth(0) == 120);
    CHECK(table->getColumnMinWidth(0) == 120);
    table->setColumnWidth(0, 110);
    CHECK(table->getColumnWidth(0) == 120);

    scene.frame();
    CHECK(table->getColumnLaidOutWidth(2) == viewport-120-controls::DataTable::DefaultMinColumnWidth);

    // no stretch: the last column at its own width
    table->setLastColumnStretch(false);
    scene.frame();
    CHECK(table->getColumnLaidOutWidth(2) == controls::DataTable::DefaultColumnWidth);
    CHECK(table->getRow(0)->getCell(2)->getWidth() == controls::DataTable::DefaultColumnWidth);
}

TEST_CASE("DataTable: a drag at a header cell's edge resizes, a click inside it sorts")
{
    Scene scene(3, 4);
    controls::DataTable *table=scene.table;
    Counter sorts;

    sorts.listen(table->onSortChanged);

    const int before=table->getColumnWidth(0);

    scene.dragGrip(0, 30);
    scene.frame();
    CHECK(table->getColumnWidth(0) == before+30);
    // the edge does not sort
    CHECK(sorts.calls == 0);
    CHECK(table->getSortColumn() == -1);
    CHECK(scene.cell(0)->getSortDirection() == controls::SortDirection::None);

    // a drag past the minimum stops at it
    scene.dragGrip(0, -1000);
    scene.frame();
    CHECK(table->getColumnWidth(0) == controls::DataTable::DefaultMinColumnWidth);
    CHECK(sorts.calls == 0);

    // inside the cell: ascending, then descending, then ascending again
    scene.click(scene.cell(1));
    CHECK(sorts.calls == 1);
    CHECK(table->getSortColumn() == 1);
    CHECK(table->getSortDirection() == controls::SortDirection::Ascending);
    CHECK(sorts.point.x == 1);
    CHECK(sorts.point.y == 1);
    CHECK(scene.cell(1)->getSortDirection() == controls::SortDirection::Ascending);

    scene.click(scene.cell(1));
    CHECK(sorts.calls == 2);
    CHECK(table->getSortDirection() == controls::SortDirection::Descending);
    CHECK(sorts.point.x == 1);
    CHECK(sorts.point.y == -1);
    CHECK(scene.cell(1)->getSortDirection() == controls::SortDirection::Descending);

    scene.click(scene.cell(1));
    CHECK(sorts.calls == 3);
    CHECK(table->getSortDirection() == controls::SortDirection::Ascending);

    // another column starts ascending; the chevron is on it alone
    scene.click(scene.cell(2));
    CHECK(sorts.calls == 4);
    CHECK(table->getSortColumn() == 2);
    CHECK(table->getSortDirection() == controls::SortDirection::Ascending);
    CHECK(scene.cell(0)->getSortDirection() == controls::SortDirection::None);
    CHECK(scene.cell(1)->getSortDirection() == controls::SortDirection::None);
    CHECK(scene.cell(2)->getSortDirection() == controls::SortDirection::Ascending);

    // a no-op fires nothing, doEvents=false fires nothing
    table->setSortColumn(2, controls::SortDirection::Ascending);
    CHECK(sorts.calls == 4);
    table->setSortColumn(0, controls::SortDirection::Descending, false);
    CHECK(sorts.calls == 4);
    CHECK(table->getSortColumn() == 0);
    CHECK(scene.cell(2)->getSortDirection() == controls::SortDirection::None);
    CHECK(scene.cell(0)->getSortDirection() == controls::SortDirection::Descending);

    // none
    table->setSortColumn(-1, controls::SortDirection::None);
    CHECK(sorts.calls == 5);
    CHECK(sorts.point.x == -1);
    CHECK(sorts.point.y == 0);
    CHECK(table->getSortColumn() == -1);
    CHECK(scene.cell(0)->getSortDirection() == controls::SortDirection::None);
}

TEST_CASE("DataTable: the stretched last column's edge follows the mouse from its laid-out width")
{
    Scene scene(2, 4);
    controls::DataTable *table=scene.table;

    // 100 asked for, the rest of the 398-unit body given
    const int laidOut=398-controls::DataTable::DefaultColumnWidth;

    REQUIRE(table->getColumnWidth(1) == controls::DataTable::DefaultColumnWidth);
    REQUIRE(table->getColumnLaidOutWidth(1) == laidOut);

    // dragged left of the body's edge it asks for less and still fills the body
    scene.dragGrip(1, -50);
    scene.frame();
    scene.frame();
    CHECK(table->getColumnWidth(1) == laidOut-50);
    CHECK(table->getColumnLaidOutWidth(1) == laidOut);

    // the first unit of a drag to the right widens it: no dead zone of the
    // width the stretch adds to what it asks for
    scene.dragGrip(1, 1);
    scene.frame();
    scene.frame();
    CHECK(table->getColumnWidth(1) == laidOut+1);
    CHECK(table->getColumnLaidOutWidth(1) == laidOut+1);
    CHECK(table->getRow(0)->getCell(1)->getWidth() == laidOut+1);
    CHECK(table->getSortColumn() == -1);
}

TEST_CASE("DataTable: a column drag changes only that column's and the following columns' geometry")
{
    Scene scene(4, 5);
    controls::DataTable *table=scene.table;

    for(int c=0; c < 4; ++c)
        table->setColumnWidth(c, 80);
    scene.frame();
    scene.frame();

    std::vector<controls::Base *> all;
    collect(table, all);

    std::map<controls::Base *, Rect> before;
    for(controls::Base *control : all)
        before[control]=Scene::canvasBounds(control);

    // the controls a drag of column 1 may change: its cells and the following
    // columns' (and everything in them), in the header and every row
    std::vector<controls::Base *> movable;
    for(int c=1; c < 4; ++c)
    {
        movable.push_back(scene.cell(c));
        for(controls::DataTableRow *row : table->getRows())
            movable.push_back(row->getCell(c));
    }
    auto mayChange=[&movable](const controls::Base *control)
        {
            for(const controls::Base *root : movable)
            {
                if(under(control, root))
                    return true;
            }
            return false;
        };

    scene.dragGrip(1, 20);
    scene.canvas->updateCanvas();

    CHECK(table->getColumnWidth(1) == 100);

    std::vector<std::string> marked;
    for(controls::Base *control : all)
    {
        if((control->getStateChange() & controls::StateChange_Geometry) && !mayChange(control))
            marked.push_back(control->getTypeName());
    }
    CHECK(marked.empty());
    CAPTURE(marked.size());

    scene.canvas->renderCanvas();

    std::vector<std::string> changed;
    for(controls::Base *control : all)
    {
        if(!(Scene::canvasBounds(control) == before[control]) && !mayChange(control))
            changed.push_back(control->getTypeName());
    }
    CHECK(changed.empty());

    // the dragged column grew, the next ones moved by as much, the stretched
    // last one gave the room back
    for(controls::DataTableRow *row : table->getRows())
    {
        CHECK((Scene::canvasBounds(row->getCell(0)) == before[row->getCell(0)]));
        CHECK(row->getCell(1)->getWidth() == before[row->getCell(1)].w+20);
        CHECK((Scene::canvasBounds(row->getCell(2)).x == before[row->getCell(2)].x+20));
        CHECK((Scene::canvasBounds(row->getCell(3)).x == before[row->getCell(3)].x+20));
        CHECK(row->getCell(3)->getWidth() == before[row->getCell(3)].w-20);
    }
    CHECK(scene.cell(1)->getWidth() == before[scene.cell(1)].w+20);
}

// The columns overflow the body (4 x 150 in a 400-wide table, scrolled to x=0):
// a column's new width gives every row and the inner panel new bounds at the
// same origin, and the region each clips its children to (its bounds within
// the viewport) stays, so nothing but what the new width moved or resized is
// marked: the rows, the panel, the column's cells and the following columns'
// (with their subtrees once moved), header and body. The cells left of the
// column, and everything in them, keep their slots.
TEST_CASE("DataTable: an overflowing column drag marks nothing left of the column")
{
    for(int column : {1, 2})
    {
        CAPTURE(column);
        Scene scene(4, 10);
        controls::DataTable *table=scene.table;

        for(int c=0; c < 4; ++c)
            table->setColumnWidth(c, 150);
        scene.frame();
        scene.frame();

        controls::ScrollControl *body=table->getBody();
        REQUIRE(body != nullptr);
        REQUIRE(body->getInnerPanel()->getX() == 0);
        REQUIRE(table->contentWidth() > body->getViewport().w);

        std::vector<controls::Base *> all;
        collect(table, all);

        std::map<controls::Base *, Rect> before;
        for(controls::Base *control : all)
            before[control]=Scene::canvasBounds(control);

        // the cells of the columns left of it, header and body
        std::vector<controls::Base *> kept;
        // the cells the new width moves, header and body: they and their
        // subtrees are marked
        std::vector<controls::Base *> moved;
        // the column's own cells: marked themselves
        std::vector<controls::Base *> resized;
        for(int c=0; c < 4; ++c)
        {
            std::vector<controls::Base *> &cells=(c < column)?kept:(c == column)?resized:moved;

            cells.push_back(scene.cell(c));
            for(controls::DataTableRow *row : table->getRows())
                cells.push_back(row->getCell(c));
        }

        table->setColumnWidth(column, 170);
        scene.canvas->updateCanvas();

        CHECK(table->getColumnWidth(column) == 170);
        // still scrolled to the left edge
        CHECK(body->getInnerPanel()->getX() == 0);

        size_t marked=0;
        std::vector<std::string> wrong;
        for(controls::Base *control : all)
        {
            const bool geometry=(control->getStateChange() & controls::StateChange_Geometry) != 0;

            if(geometry)
                ++marked;

            bool inKept=false;
            bool inMoved=false;
            for(controls::Base *root : kept)
                inKept=inKept || under(control, root);
            for(controls::Base *root : moved)
                inMoved=inMoved || under(control, root);

            const bool isRow=std::find(table->getRows().begin(), table->getRows().end(), control)
                != table->getRows().end();
            const bool isResized=std::find(resized.begin(), resized.end(), control) != resized.end();

            if(inKept && geometry)
                wrong.push_back(std::string("marked left of the column: ")+control->getTypeName());
            if((inMoved || isRow || isResized) && !geometry)
                wrong.push_back(std::string("not marked: ")+control->getTypeName());
        }
        // 102 controls of them, when a size change marked a whole subtree
        MESSAGE("column ", column, ": ", marked, " of ", all.size(), " controls marked");
        CHECK(wrong.empty());
        for(const std::string &line : wrong)
            MESSAGE(line);

        scene.canvas->renderCanvas();

        for(controls::DataTableRow *row : table->getRows())
        {
            CHECK((Scene::canvasBounds(row->getCell(0)) == before[row->getCell(0)]));
            CHECK(row->getCell(column)->getWidth() == 170);
            CHECK(Scene::canvasBounds(row->getCell(3)).x == before[row->getCell(3)].x+20);
            CHECK(row->getWidth() == before[row].w+20);
        }
    }
}

TEST_CASE("DataTable: a sort click and the app's re-bind are laid out in one pass")
{
    Scene scene(2, 12);
    controls::DataTable *table=scene.table;

    // the app sorts its rows by column's text on every sort change
    struct Sorter: public event::Handler
    {
        controls::DataTable *table=nullptr;
        bool reorder=true;

        void sort(event::Info)
        {
            const int column=table->getSortColumn();
            const bool descending=table->getSortDirection() == controls::SortDirection::Descending;

            if(reorder)
            {
                std::vector<controls::DataTableRow *> rows=table->getRows();

                std::stable_sort(rows.begin(), rows.end(),
                    [column, descending](controls::DataTableRow *a, controls::DataTableRow *b)
                    {
                        return descending?b->getCellText(column) < a->getCellText(column):
                            a->getCellText(column) < b->getCellText(column);
                    });
                table->setRowOrder(rows);
                return;
            }

            // a re-bind: the same rows given the records in the new order
            std::vector<String> texts;
            for(controls::DataTableRow *row : table->getRows())
                texts.push_back(row->getCellText(column));
            std::sort(texts.begin(), texts.end());
            if(descending)
                std::reverse(texts.begin(), texts.end());
            for(size_t i=0; i < texts.size(); ++i)
                table->getRow(int(i))->setCellText(column, texts[i]);
        }
    } sorter;

    sorter.table=table;
    table->onSortChanged.add(&sorter, &Sorter::sort);

    for(bool reorder : {true, false})
    {
        CAPTURE(reorder);
        sorter.reorder=reorder;

        // ascending first, sorted by the app; the click then makes it
        // descending, which changes what every row shows
        table->setSortColumn(-1, controls::SortDirection::None, false);
        table->setSortColumn(0, controls::SortDirection::Ascending);
        scene.frame();

        scene.clock+=1.0f;
        scene.moveTo(Scene::centre(scene.cell(0)));
        scene.frame();
        scene.canvas->inputMouseButton(0, true);
        scene.canvas->inputMouseButton(0, false);
        scene.canvas->updateCanvas();

        const controls::LayoutStats stats=scene.canvas->getLayoutStats();

        CHECK(table->getSortDirection() == controls::SortDirection::Descending);
        CHECK(stats.passes == 1);
        CHECK(stats.redirtiedType == nullptr);
        scene.canvas->renderCanvas();
        CHECK(!scene.canvas->getLayoutStats().renderLayout);

        // the body shows the new order, top to bottom
        const std::vector<controls::DataTableRow *> &rows=table->getRows();
        for(size_t i=1; i < rows.size(); ++i)
        {
            CHECK(rows[i-1]->getCellText(0) >= rows[i]->getCellText(0));
            CHECK(Scene::canvasBounds(rows[i-1]).y < Scene::canvasBounds(rows[i]).y);
        }

        // and is settled
        scene.frame();
        CHECK(scene.canvas->getLayoutStats().passes == 0);
    }
}

TEST_CASE("DataTable: the stripes follow the row order")
{
    Scene scene(2, 6);
    controls::DataTable *table=scene.table;

    auto stripesByOrder=[table]()
        {
            for(int i=0; i < table->rowCount(); ++i)
            {
                if(table->getRow(i)->isStriped() != (table->isStriped() && i%2 == 1))
                    return false;
            }
            return true;
        };

    CHECK(table->isStriped());
    CHECK(stripesByOrder());
    CHECK(!table->getRow(0)->isStriped());
    CHECK(table->getRow(1)->isStriped());

    std::vector<controls::DataTableRow *> rows=table->getRows();
    controls::DataTableRow *first=rows[0];

    // a re-bind keeps the rows and their stripes
    first->setCellText(0, "zzz");
    scene.frame();
    CHECK(stripesByOrder());
    CHECK(!first->isStriped());

    // a new order: the stripes go by the new places, the body shows that order
    std::reverse(rows.begin(), rows.end());
    table->setRowOrder(rows);
    CHECK(table->getRows() == rows);
    CHECK(stripesByOrder());
    CHECK(first->isStriped());  // now the last of six
    scene.frame();
    for(size_t i=1; i < rows.size(); ++i)
        CHECK(Scene::canvasBounds(rows[i-1]).y < Scene::canvasBounds(rows[i]).y);

    // not the same rows: ignored
    std::vector<controls::DataTableRow *> wrong=rows;
    wrong[0]=wrong[1];
    table->setRowOrder(wrong);
    CHECK(table->getRows() == rows);

    // a removed row moves the stripes of those after it
    table->removeRow(rows[1]);
    CHECK(table->rowCount() == 5);
    CHECK(stripesByOrder());
    scene.frame();

    table->setStriped(false);
    for(controls::DataTableRow *row : table->getRows())
        CHECK(!row->isStriped());
    table->setStriped(true);
    CHECK(stripesByOrder());

    // a row added goes by its place too
    controls::DataTableRow *added=table->addRow();
    CHECK(table->getRowIndex(added) == 5);
    CHECK(added->isStriped());
    CHECK(added->getCellCount() == 2);
    scene.frame();
}

TEST_CASE("DataTable: one row selected through the group, a removed or deleted one leaves none")
{
    Scene scene(2, 6);
    controls::DataTable *table=scene.table;
    Counter changes;

    changes.listen(table->onSelectionChange);

    controls::DataTableRow *a=table->getRow(1);
    controls::DataTableRow *b=table->getRow(3);

    scene.click(a);
    CHECK(table->getSelectedRow() == a);
    CHECK(a->isSelected());
    CHECK(changes.calls == 1);
    CHECK(changes.last == a);

    // a second click keeps it (a group's rows do not toggle)
    scene.click(a);
    CHECK(table->getSelectedRow() == a);
    CHECK(changes.calls == 1);

    scene.click(b);
    CHECK(table->getSelectedRow() == b);
    CHECK(!a->isSelected());
    CHECK(changes.calls == 2);
    CHECK(changes.last == b);

    // removed while selected: none, once
    table->removeRow(b);
    CHECK(table->getSelectedRow() == nullptr);
    CHECK(changes.calls == 3);
    CHECK(changes.last == nullptr);
    CHECK(table->getRowIndex(b) == -1);
    scene.frame();

    table->setSelectedRow(a);
    CHECK(table->getSelectedRow() == a);
    CHECK(changes.calls == 4);
    table->setSelectedRow(nullptr);
    CHECK(table->getSelectedRow() == nullptr);
    CHECK(changes.calls == 5);

    // deleted outright while selected: the table forgets it, and has none
    table->setSelectedRow(a);
    const int rows=table->rowCount();
    delete a;
    CHECK(table->rowCount() == rows-1);
    CHECK(table->getSelectedRow() == nullptr);
    scene.frame();

    // cleared: no rows, no selection
    table->setSelectedRow(table->getRow(0));
    table->clear();
    CHECK(table->rowCount() == 0);
    CHECK(table->getSelectedRow() == nullptr);
    scene.frame();
}

TEST_CASE("DataTable: a control in a cell takes the mouse as its own flag says")
{
    Scene scene(2, 4);
    controls::DataTable *table=scene.table;
    controls::DataTableRow *withButton=table->getRow(1);
    controls::DataTableRow *withLabel=table->getRow(2);
    Counter presses;

    controls::Button *button=scene.canvas->newChild<controls::Button>();
    button->setBounds(4, 2, 40, 12);
    withButton->setCellContents(0, button);
    presses.listen(button->onPressCaller);

    controls::Label *label=scene.canvas->newChild<controls::Label>();
    label->setText("x");
    label->setBounds(4, 2, 40, 12);
    withLabel->setCellContents(1, label);
    scene.frame();
    scene.frame();

    REQUIRE(button->getParent() == withButton->getCell(0));
    REQUIRE(label->getParent() == withLabel->getCell(1));

    // the Button takes its click: pressed, and the row is not selected
    scene.click(button);
    CHECK(presses.calls == 1);
    CHECK(table->getSelectedRow() == nullptr);

    // elsewhere on that cell, the row's
    {
        const Rect cell=Scene::canvasBounds(withButton->getCell(0));

        scene.clock+=1.0f;
        scene.moveTo(Point(cell.x+cell.w-8, cell.y+cell.h/2));
        scene.canvas->inputMouseButton(0, true);
        scene.canvas->inputMouseButton(0, false);
        scene.frame();
    }
    CHECK(presses.calls == 1);
    CHECK(table->getSelectedRow() == withButton);

    // a Label takes no mouse: a click on it selects its row
    scene.click(label);
    CHECK(table->getSelectedRow() == withLabel);
}

TEST_CASE("DataTable: the header stays put while the body scrolls down, and follows it sideways")
{
    Scene scene(4, 40);
    controls::DataTable *table=scene.table;

    // 4 x 150 = 600 units of columns in a 398-unit wide body: it scrolls both ways
    for(int c=0; c < 4; ++c)
        table->setColumnWidth(c, 150);
    scene.frame();
    scene.frame();

    controls::DataTableRow *first=table->getRow(0);
    const Rect header=Scene::canvasBounds(table->getHeader());
    const Rect cell0=Scene::canvasBounds(scene.cell(0));
    const Rect cell3=Scene::canvasBounds(scene.cell(3));
    const Rect row0=Scene::canvasBounds(first);

    CHECK((Scene::canvasBounds(first->getCell(0)).x == cell0.x));
    CHECK(row0.y == header.y+header.h);

    table->getBody()->scrollToBottom();
    scene.frame();

    CHECK((Scene::canvasBounds(table->getHeader()) == header));
    CHECK((Scene::canvasBounds(scene.cell(0)) == cell0));
    CHECK(Scene::canvasBounds(first).y < row0.y);

    table->getBody()->scrollToRight();
    scene.frame();

    const Rect scrolled0=Scene::canvasBounds(scene.cell(0));
    const Rect scrolled3=Scene::canvasBounds(scene.cell(3));

    CHECK((Scene::canvasBounds(table->getHeader()) == header));
    CHECK(scrolled0.x < cell0.x);
    CHECK(scrolled3.x-cell3.x == scrolled0.x-cell0.x);
    CHECK(scrolled0.y == cell0.y);
    // the header's cells are where the body's are
    CHECK((Scene::canvasBounds(first->getCell(0)).x == scrolled0.x));
    CHECK((Scene::canvasBounds(first->getCell(3)).x == scrolled3.x));
    // the last column's right edge at the viewport's
    const Rect viewport=table->getBody()->getViewport();
    const Point body=table->getBody()->localPosToCanvas();
    CHECK(scrolled3.x+scrolled3.w == body.x+viewport.x+viewport.w);

    // the header's cells are clipped to the viewport, not drawn over the bar
    const Rect *clip=table->getHeader()->getChildClipRect(scene.cell(0));
    REQUIRE((clip != nullptr));
    CHECK(clip->x == viewport.x);
    CHECK(clip->w == viewport.w);

    // and back
    table->getBody()->scrollToLeft();
    scene.frame();
    CHECK((Scene::canvasBounds(scene.cell(0)) == cell0));
}

TEST_CASE("DataTable: the textured renderers stripe, sort, rule, and draw only what changed")
{
    Scene scene(3, 6, true);
    controls::DataTable *table=scene.table;

    table->setBorders(controls::DataTableBorders::All);
    scene.frame();

    auto drewInto=[&scene](size_t id) -> const DrawRecorder::Draw *
        {
            for(const DrawRecorder::Draw &draw : scene.renderer.draws)
            {
                if(draw.id == id)
                    return &draw;
            }
            return nullptr;
        };
    auto hid=[&scene](size_t id)
        {
            return std::find(scene.renderer.hidden.begin(), scene.renderer.hidden.end(), id)
                != scene.renderer.hidden.end();
        };
    auto slots=[](controls::Base *control) -> const std::vector<size_t> &
        {
            return control->getSkinControl()->getPrimitives();
        };

    controls::DataTableRow *even=table->getRow(0);
    controls::DataTableRow *odd=table->getRow(1);

    // [0] the fill, [1] the bottom rule, [2], [3] the rules after columns 0, 1
    REQUIRE(slots(even).size() == 4);
    REQUIRE(slots(odd).size() == 4);

    // redraw every row with its borders: the odd one filled, the even one not
    table->setBorders(controls::DataTableBorders::None);
    scene.frame();
    table->setBorders(controls::DataTableBorders::All);
    scene.frame();

    const DrawRecorder::Draw *stripe=drewInto(slots(odd)[0]);
    REQUIRE(stripe != nullptr);
    CHECK((stripe->color == skin::textured::tokyonight::DataTableStripe.top));
    CHECK(hid(slots(even)[0]));

    for(size_t i=1; i < 4; ++i)
    {
        const DrawRecorder::Draw *rule=drewInto(slots(odd)[i]);

        REQUIRE(rule != nullptr);
        CHECK(rule->kind == 6);
        CHECK(rule->direction == (i == 1?0:1));
        CHECK(rule->zIndex == odd->getZIndex()+1);
    }

    // a column rule at its cell's right edge, in the row's coordinates' parent
    const DrawRecorder::Draw *columnRule=drewInto(slots(odd)[2]);
    const Rect cell0=odd->getCell(0)->getBounds();
    CHECK(columnRule->rect.x+1 == odd->getRenderBounds().x+cell0.x+cell0.w-1);

    // no column rules, no slots for them
    table->setBorders(controls::DataTableBorders::Rows);
    scene.frame();
    CHECK(slots(odd).size() == 2);

    // a settled frame draws nothing
    scene.frame();
    scene.frame();
    CHECK(scene.renderer.draws.empty());
    CHECK(scene.renderer.hidden.empty());

    // a hover draws into the hovered row's slots only (its fill), and its
    // cells' text
    scene.moveTo(Scene::centre(even));
    scene.frame();
    REQUIRE(!scene.renderer.draws.empty());
    for(const DrawRecorder::Draw &draw : scene.renderer.draws)
    {
        CHECK(std::find(slots(even).begin(), slots(even).end(), draw.id) != slots(even).end());
    }
    const DrawRecorder::Draw *hover=drewInto(slots(even)[0]);
    REQUIRE(hover != nullptr);
    CHECK((hover->color == skin::textured::tokyonight::SelectableHover.top));

    // the sorted column's chevron, up and then down, at the cell's z+1
    controls::internal::DataTableHeaderCell *header=scene.cell(1);

    table->setSortColumn(1, controls::SortDirection::Ascending);
    scene.frame();
    const DrawRecorder::Draw *up=drewInto(slots(header)[1]);
    REQUIRE(up != nullptr);
    CHECK(up->kind == 1);
    CHECK(up->direction == 1);
    CHECK(up->zIndex == header->getZIndex()+1);
    // the other cells were not touched
    CHECK(drewInto(slots(scene.cell(0))[1]) == nullptr);
    CHECK(drewInto(slots(scene.cell(2))[1]) == nullptr);

    table->setSortColumn(1, controls::SortDirection::Descending);
    scene.frame();
    const DrawRecorder::Draw *down=drewInto(slots(header)[1]);
    REQUIRE(down != nullptr);
    CHECK(down->direction == 0);

    // moved to another column: this one's chevron hidden
    table->setSortColumn(2, controls::SortDirection::Ascending);
    scene.frame();
    CHECK(hid(slots(header)[1]));
    CHECK(drewInto(slots(scene.cell(2))[1]) != nullptr);
}
