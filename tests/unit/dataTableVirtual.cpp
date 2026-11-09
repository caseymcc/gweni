/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  DataTable in source mode (setItemCount, setRowBinder) on a real Canvas
 *  with the StubSkin, driven through the input handler: its body is a
 *  VirtualList of pooled DataTableRows, the viewport's rows plus one, so 1 000
 *  and 100 000 items take the same controls and ids. Across one-row scrolls
 *  (each binding exactly one row), a page, a jump to the middle and to the
 *  end, and a resize taller and shorter, every item in view is shown by one
 *  pooled row bound to it, at its place, with the binder's text in each cell
 *  under its header cell. The stripes are the odd items' and the selection an
 *  item: a click selects the item its row shows (once, onSelectionChange with
 *  the item), the selection is shown on no row while the item is scrolled out
 *  and on the row that shows it once it is back, and a stripe never moves with
 *  a pooled row. A column drag reaches every pooled row (shown and spare) and
 *  changes only that column's cells and the following columns'; with the
 *  columns overflowing the body (scrolled to x=0) the rows grow at the same
 *  origin and nothing in the cells left of the column is marked; a sort click
 *  with the app's rebind() binds only the rows on screen, once each, in one
 *  update-phase pass. A count that shrinks while scrolled to the end keeps the
 *  view full and valid and drops a selection past the new end (with its
 *  event); one that grows binds nothing still showing its item. The header
 *  follows the body sideways, the source mode drops the rows addRow made (and
 *  their selection) and leaves the row API inert, and every frame lays out in
 *  one pass with no layout request.
 */
#include "headlessFixture.h"

#include <gweni/controls/dataTable.h>
#include <gweni/controls/virtualList.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

constexpr int RowHeight=20;

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::DataTable *table=nullptr;
    float clock=10.0f;

    //! Every bind, in call order, and the item each row was last bound to.
    std::vector<std::pair<controls::DataTableRow *, size_t>> binds;
    std::map<controls::DataTableRow *, size_t> boundTo;
    //! The binder shows item i as record order(i): the identity, or the
    //! reverse once the app "sorted" its records descending.
    bool reversed=false;

    explicit Scene(size_t count, int columns=3, int height=200)
    {
        // a second apart per click, so two clicks are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 500);

        table=canvas->newChild<controls::DataTable>();
        table->setBounds(10, 10, 400, height);
        table->setColumnCount(columns);
        for(int c=0; c < columns; ++c)
            table->setColumnCaption(c, "Column "+std::to_string(c));
        table->setRowHeight(RowHeight);
        table->setRowBinder([this](controls::DataTableRow *row, size_t item)
            {
                binds.push_back({row, item});
                boundTo[row]=item;
                for(int c=0; c < row->getCellCount(); ++c)
                    row->setCellText(c, text(item, c));
            });
        table->setItemCount(count);
        frame();
        frame();
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

    size_t count() const { return table->getItemCount(); }

    //! What the binder writes in a cell for an item.
    String text(size_t item, int column) const
    {
        const size_t record=reversed?count()-1-item:item;

        return "r"+std::to_string(record)+"c"+std::to_string(column);
    }

    controls::VirtualList *list() const { return table->getVirtualBody(); }

    //! Over every frame: layout requests (none), the most update-phase
    //! passes one took (one) and render-phase layouts (none).
    int requests=0;
    int maxPasses=0;
    bool renderLayout=false;

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();

        const controls::LayoutStats &layout=canvas->getLayoutStats();

        requests+=layout.requests;
        maxPasses=std::max(maxPasses, layout.passes);
        renderLayout=renderLayout || layout.renderLayout;
    }

    void checkOnePass() const
    {
        CHECK(requests == 0);
        CHECK(maxPasses <= 1);
        CHECK(!renderLayout);
    }

    std::vector<std::pair<controls::DataTableRow *, size_t>> takeBinds()
    {
        std::vector<std::pair<controls::DataTableRow *, size_t>> taken;

        taken.swap(binds);
        return taken;
    }

    std::vector<controls::DataTableRow *> pooledRows() const
    {
        std::vector<controls::DataTableRow *> rows;

        for(controls::Base *control : list()->getRows())
        {
            controls::DataTableRow *row=gweni_cast<controls::DataTableRow>(control);

            REQUIRE(row != nullptr);
            rows.push_back(row);
        }
        return rows;
    }

    std::vector<controls::DataTableRow *> shownRows() const
    {
        std::vector<controls::DataTableRow *> shown;

        for(controls::DataTableRow *row : pooledRows())
        {
            if(!row->hidden())
                shown.push_back(row);
        }
        return shown;
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

    controls::internal::DataTableHeaderCell *cell(int column) const
    {
        return table->getHeader()->getCells().at(column);
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

    static Rect canvasBounds(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        return Rect(at.x, at.y, control->getWidth(), control->getHeight());
    }
};

//! Every item in the viewport is shown by exactly one pooled row, bound to it
//! (its cells hold the binder's text) and placed at its place, its cells under
//! the header cells; no other row is shown. The pool is the viewport's rows
//! plus one.
void checkView(Scene &s)
{
    controls::VirtualList *list=s.list();
    const int offset=list->getVerticalScrollOffset();
    const int viewport=list->getViewportHeight();
    const size_t count=s.count();

    REQUIRE(viewport > 0);
    CHECK(offset >= 0);
    CHECK(offset <= list->getMaxVerticalScrollOffset());
    CHECK(list->getRows().size() == size_t((viewport+RowHeight-1)/RowHeight)+1);

    const size_t first=offset/RowHeight;
    const size_t last=std::min<size_t>(count, (offset+viewport+RowHeight-1)/RowHeight);
    std::set<controls::DataTableRow *> expected;

    for(size_t item=first; item < last; ++item)
    {
        controls::DataTableRow *row=s.table->rowForItem(item);

        INFO("item ", item, " offset ", offset);
        REQUIRE(row != nullptr);
        CHECK(!row->hidden());
        CHECK(s.table->itemForRow(row) == item);
        CHECK(s.boundTo[row] == item);
        CHECK(row->getY() == static_cast<int>(item*RowHeight)-offset);
        CHECK(row->getHeight() == RowHeight);
        CHECK(row->getCellCount() == s.table->getColumnCount());

        for(int c=0; c < row->getCellCount(); ++c)
        {
            CHECK(row->getCellText(c) == s.text(item, c));
            CHECK(Scene::canvasBounds(row->getCell(c)).x == Scene::canvasBounds(s.cell(c)).x);
            CHECK(row->getCell(c)->getWidth() == s.cell(c)->getWidth());
        }
        expected.insert(row);
    }

    for(controls::DataTableRow *row : s.shownRows())
        CHECK(expected.count(row) == 1);
    CHECK(s.shownRows().size() == expected.size());
}

//! Counts a Caller's calls and keeps the last info.
class Counter: public event::Handler
{
public:
    void count(event::Info info)
    {
        ++calls;
        last=info.control;
        integer=info.integer;
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
    controls::Base *last=nullptr;
    size_t integer=0;
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

TEST_CASE("DataTable source mode: the pooled rows show the items in view across scrolls, jumps and resizes")
{
    Scene s(1000);
    controls::VirtualList *list=s.list();

    REQUIRE(s.table->isSourceMode());
    REQUIRE((s.table->getBody() == list));
    CHECK(s.table->getItemCount() == 1000);
    checkView(s);

    // the rows on screen bound once each
    const size_t onScreen=s.shownRows().size();

    CHECK(s.takeBinds().size() == onScreen);

    // one row down: the row that left takes the one that came into view, and
    // only it is bound
    for(int step=0; step < 3; ++step)
    {
        list->scrollByRows(1);
        s.frame();
        checkView(s);

        const auto binds=s.takeBinds();

        REQUIRE(binds.size() == 1);
        CHECK(binds[0].second == list->firstVisibleItem()+onScreen-1);
    }

    // and back up one
    list->scrollByRows(-1);
    s.frame();
    checkView(s);
    CHECK(s.takeBinds().size() == 1);

    // a page, a part-row offset, the middle, the end
    for(int offset : {200, 207, 10000, list->getMaxVerticalScrollOffset()})
    {
        CAPTURE(offset);
        list->setVerticalScrollOffset(offset);
        s.frame();
        CHECK(list->getVerticalScrollOffset() == offset);
        checkView(s);
        s.takeBinds();
    }
    CHECK(s.table->rowForItem(999) != nullptr);

    // taller: the pool follows the viewport, the offset clamped to the new
    // end; shorter: the pool follows, the offset kept
    s.table->setSize(400, 320);
    s.frame();
    checkView(s);
    CHECK(list->getVerticalScrollOffset() == list->getMaxVerticalScrollOffset());
    s.takeBinds();

    const int kept=list->getVerticalScrollOffset();

    for(int height : {120, 200})
    {
        CAPTURE(height);
        s.table->setSize(400, height);
        s.frame();
        checkView(s);
        CHECK(list->getVerticalScrollOffset() == kept);
        s.takeBinds();
    }

    s.table->scrollToItem(0);
    s.frame();
    CHECK(list->getVerticalScrollOffset() == 0);
    checkView(s);

    s.checkOnePass();
}

TEST_CASE("DataTable source mode: 1 000 and 100 000 items take the same controls and ids")
{
    struct Taken
    {
        size_t controls;
        size_t primitives;
        size_t texts;
        size_t binds;
        size_t pool;
    };

    auto take=[](size_t count)
    {
        test::IdBankCounts before=test::IdBankCounts::now();
        Scene s(count);
        test::IdBankCounts after=test::IdBankCounts::now();
        std::vector<controls::Base *> all;

        checkView(s);
        collect(s.table, all);
        return Taken{all.size(), after.livePrimitives()-before.livePrimitives(),
            after.liveTexts()-before.liveTexts(), s.takeBinds().size(), s.list()->getRows().size()};
    };

    const Taken small=take(1000);
    const Taken large=take(100000);

    CHECK(small.controls == large.controls);
    CHECK(small.primitives == large.primitives);
    CHECK(small.texts == large.texts);
    CHECK(small.binds == large.binds);
    CHECK(small.pool == large.pool);
    // the pool is a handful of rows, not the items
    CHECK(small.pool < 20);
}

TEST_CASE("DataTable source mode: stripes and the selection go by item, across scrolling")
{
    Scene s(1000);
    controls::DataTable *table=s.table;
    controls::VirtualList *list=s.list();
    Counter selections;

    selections.listen(table->onSelectionChange);

    auto checkStripes=[&s]()
    {
        for(controls::DataTableRow *row : s.shownRows())
        {
            const size_t item=s.table->itemForRow(row);

            INFO("item ", item);
            CHECK(row->isStriped() == ((item%2) == 1));
            CHECK(row->isSelected() == (item == s.table->getSelectedItem()));
        }
    };

    checkStripes();

    // a click on the row showing item 3 selects item 3
    controls::DataTableRow *row3=table->rowForItem(3);

    REQUIRE(row3 != nullptr);
    s.click(row3);
    CHECK(table->getSelectedItem() == 3);
    CHECK((table->getSelectedRow() == row3));
    CHECK(row3->isSelected());
    CHECK(selections.calls == 1);
    CHECK(selections.integer == 3);
    CHECK((selections.last == row3));
    checkStripes();

    // the same row again: nothing changes, no event
    s.click(row3);
    CHECK(table->getSelectedItem() == 3);
    CHECK(selections.calls == 1);

    // a row at a time down past it: the stripes stay on the odd items while
    // the pooled rows rotate, and once item 3 has left no row shows a selection
    for(int step=0; step < 15; ++step)
    {
        list->scrollByRows(1);
        s.frame();
        checkStripes();
    }
    list->scrollByRows(40);
    s.frame();
    CHECK(table->rowForItem(3) == nullptr);
    CHECK(table->getSelectedRow() == nullptr);
    CHECK(table->getSelectedItem() == 3);
    for(controls::DataTableRow *row : s.pooledRows())
        CHECK((!row->isSelected() || row->hidden()));
    checkStripes();

    // and back: shown again on whichever row holds item 3 now
    table->scrollToItem(3);
    s.frame();
    REQUIRE(table->rowForItem(3) != nullptr);
    CHECK(table->rowForItem(3)->isSelected());
    checkStripes();
    CHECK(selections.calls == 1);

    // setSelectedItem from outside, onto an item scrolled out, then none
    table->setSelectedItem(500);
    s.frame();
    CHECK(selections.calls == 2);
    CHECK(selections.integer == 500);
    CHECK(selections.last == nullptr);
    checkStripes();
    table->setSelectedItem(500);
    CHECK(selections.calls == 2);
    table->setSelectedRow(nullptr);
    CHECK(table->getSelectedItem() == controls::DataTable::NoItem);
    CHECK(selections.calls == 3);
    CHECK(selections.integer == controls::DataTable::NoItem);
    s.frame();
    checkStripes();

    // no stripes at all, then back
    table->setStriped(false);
    s.frame();
    for(controls::DataTableRow *row : s.shownRows())
        CHECK(!row->isStriped());
    table->setStriped(true);
    s.frame();
    checkStripes();

    s.checkOnePass();
}

TEST_CASE("DataTable source mode: a column drag reaches every pooled row and moves only the following columns")
{
    Scene s(1000, 4);
    controls::DataTable *table=s.table;

    for(int c=0; c < 4; ++c)
        table->setColumnWidth(c, 80);
    // a viewport half a row past whole rows and a part-row offset: every
    // pooled row is shown, the spare one included
    s.frame();
    const int viewport=s.list()->getViewportHeight();

    table->setSize(400, table->getHeight()+RowHeight/2-viewport%RowHeight);
    s.list()->setVerticalScrollOffset(RowHeight*10+RowHeight-1);
    s.frame();
    s.frame();
    REQUIRE(s.list()->getViewportHeight()%RowHeight == RowHeight/2);
    REQUIRE(s.shownRows().size() == s.pooledRows().size());

    std::vector<controls::Base *> all;
    collect(table, all);

    std::map<controls::Base *, Rect> before;
    for(controls::Base *control : all)
        before[control]=Scene::canvasBounds(control);

    std::vector<controls::Base *> movable;
    for(int c=1; c < 4; ++c)
    {
        movable.push_back(s.cell(c));
        for(controls::DataTableRow *row : s.pooledRows())
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

    s.binds.clear();
    s.dragGrip(1, 20);
    s.canvas->updateCanvas();

    CHECK(table->getColumnWidth(1) == 100);
    CHECK(s.canvas->getLayoutStats().passes == 1);

    std::vector<std::string> marked;
    for(controls::Base *control : all)
    {
        if((control->getStateChange() & controls::StateChange_Geometry) && !mayChange(control))
            marked.push_back(control->getTypeName());
    }
    CHECK(marked.empty());

    s.canvas->renderCanvas();

    std::vector<std::string> changed;
    for(controls::Base *control : all)
    {
        if(!(Scene::canvasBounds(control) == before[control]) && !mayChange(control))
            changed.push_back(control->getTypeName());
    }
    CHECK(changed.empty());
    // a drag binds nothing
    CHECK(s.binds.empty());

    for(controls::DataTableRow *row : s.pooledRows())
    {
        CHECK((Scene::canvasBounds(row->getCell(0)) == before[row->getCell(0)]));
        CHECK(row->getCell(1)->getWidth() == before[row->getCell(1)].w+20);
        CHECK(Scene::canvasBounds(row->getCell(2)).x == before[row->getCell(2)].x+20);
        CHECK(row->getCell(3)->getWidth() == before[row->getCell(3)].w-20);
    }
    checkView(s);

    // a row scrolled into view after the drag has the new columns too
    s.list()->setVerticalScrollOffset(RowHeight*200);
    s.frame();
    checkView(s);

    // a column more: every pooled row gets its cell, and the rows on screen
    // are bound again to fill it
    s.binds.clear();
    table->setColumnCount(5);
    s.frame();
    CHECK(s.binds.size() == s.shownRows().size());
    for(controls::DataTableRow *row : s.pooledRows())
        CHECK(row->getCellCount() == 5);
    checkView(s);

    s.checkOnePass();
}

// The columns overflow the body (4 x 150 in a 400-wide table, scrolled to x=0):
// a column's new width gives every pooled row and the inner panel new bounds
// at the same origin, the region each clips its children to (its bounds
// within the viewport) staying, so what is marked is the rows, the column's
// cells and the following columns' (with their subtrees once moved), header
// and body, and nothing in the cells left of the column: a drag frame does not
// re-stamp the whole pool.
TEST_CASE("DataTable source mode: an overflowing column drag marks nothing left of the column")
{
    for(int column : {1, 2})
    {
        CAPTURE(column);
        Scene s(1000, 4);
        controls::DataTable *table=s.table;

        for(int c=0; c < 4; ++c)
            table->setColumnWidth(c, 150);
        s.frame();
        s.frame();

        controls::VirtualList *list=s.list();
        REQUIRE(list->getInnerPanel()->getX() == 0);
        REQUIRE(table->contentWidth() > list->getViewport().w);
        REQUIRE(!s.shownRows().empty());

        std::vector<controls::Base *> all;
        collect(table, all);

        std::map<controls::Base *, Rect> before;
        for(controls::Base *control : all)
            before[control]=Scene::canvasBounds(control);

        // the spare row, hidden, is laid out once it shows
        const std::vector<controls::DataTableRow *> rows=s.shownRows();
        std::vector<controls::Base *> kept, moved, resized;
        for(int c=0; c < 4; ++c)
        {
            std::vector<controls::Base *> &cells=(c < column)?kept:(c == column)?resized:moved;

            cells.push_back(s.cell(c));
            for(controls::DataTableRow *row : s.pooledRows())
            {
                if((c < column) || !row->hidden())
                    cells.push_back(row->getCell(c));
            }
        }

        s.binds.clear();
        table->setColumnWidth(column, 170);
        s.canvas->updateCanvas();

        CHECK(table->getColumnWidth(column) == 170);
        CHECK(list->getInnerPanel()->getX() == 0);

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

            const bool isRow=std::find(rows.begin(), rows.end(), control) != rows.end();
            const bool isResized=std::find(resized.begin(), resized.end(), control) != resized.end();

            if(inKept && geometry)
                wrong.push_back(std::string("marked left of the column: ")+control->getTypeName());
            if((inMoved || isRow || isResized) && !geometry)
                wrong.push_back(std::string("not marked: ")+control->getTypeName());
        }
        MESSAGE("column ", column, ": ", marked, " of ", all.size(), " controls marked");
        CHECK(wrong.empty());
        for(const std::string &line : wrong)
            MESSAGE(line);

        s.canvas->renderCanvas();
        // a drag binds nothing
        CHECK(s.binds.empty());

        for(controls::DataTableRow *row : rows)
        {
            CHECK((Scene::canvasBounds(row->getCell(0)) == before[row->getCell(0)]));
            CHECK(row->getCell(column)->getWidth() == 170);
            CHECK(Scene::canvasBounds(row->getCell(3)).x == before[row->getCell(3)].x+20);
        }
        checkView(s);
    }
}

TEST_CASE("DataTable source mode: a sort click and rebind() bind only the rows on screen, in one pass")
{
    Scene s(1000);
    controls::DataTable *table=s.table;

    // the app "sorts" its records: descending is the reverse order
    struct Sorter: public event::Handler
    {
        Scene *scene=nullptr;

        void sort(event::Info)
        {
            scene->reversed=(scene->table->getSortDirection() == controls::SortDirection::Descending);
            scene->table->rebind();
        }
    } sorter;

    sorter.scene=&s;
    table->onSortChanged.add(&sorter, &Sorter::sort);
    table->setSortColumn(0, controls::SortDirection::Ascending);
    s.list()->setVerticalScrollOffset(RowHeight*100);
    s.frame();
    s.frame();
    checkView(s);
    s.takeBinds();

    const size_t onScreen=s.shownRows().size();

    // the click makes it descending: every row on screen shows another record
    s.clock+=1.0f;
    s.moveTo(Scene::centre(s.cell(0)));
    s.frame();
    s.canvas->inputMouseButton(0, true);
    s.canvas->inputMouseButton(0, false);
    s.canvas->updateCanvas();

    const controls::LayoutStats stats=s.canvas->getLayoutStats();

    CHECK(table->getSortDirection() == controls::SortDirection::Descending);
    CHECK(s.reversed);
    CHECK(stats.passes == 1);
    CHECK(stats.redirtiedType == nullptr);
    s.canvas->renderCanvas();
    CHECK(!s.canvas->getLayoutStats().renderLayout);

    // the rows on screen, bound once each, and no other
    const auto binds=s.takeBinds();
    std::set<controls::DataTableRow *> bound;

    for(const auto &bind : binds)
        bound.insert(bind.first);
    CHECK(binds.size() == onScreen);
    CHECK(bound.size() == onScreen);
    for(controls::DataTableRow *row : bound)
        CHECK(!row->hidden());
    checkView(s);

    // rebindItem binds the row of that item alone
    const size_t item=s.list()->firstVisibleItem()+2;

    table->rebindItem(item);
    s.frame();
    {
        const auto again=s.takeBinds();

        REQUIRE(again.size() == 1);
        CHECK(again[0].second == item);
    }

    // settled
    s.frame();
    CHECK(s.canvas->getLayoutStats().passes == 0);
    CHECK(s.takeBinds().empty());

    s.checkOnePass();
}

TEST_CASE("DataTable source mode: the item count changing keeps the view valid")
{
    Scene s(1000);
    controls::DataTable *table=s.table;
    controls::VirtualList *list=s.list();
    Counter selections;

    selections.listen(table->onSelectionChange);

    table->scrollToItem(999);
    s.frame();
    table->setSelectedItem(990);
    s.frame();
    REQUIRE(table->rowForItem(990) != nullptr);
    checkView(s);

    // shrunk while scrolled to the end: the end moves up into the viewport,
    // and the selection past it goes, with its event
    table->setItemCount(500);
    CHECK(table->getSelectedItem() == controls::DataTable::NoItem);
    CHECK(selections.calls == 2);
    CHECK(selections.integer == controls::DataTable::NoItem);
    s.frame();
    CHECK(list->getVerticalScrollOffset() == list->getMaxVerticalScrollOffset());
    CHECK(table->rowForItem(499) != nullptr);
    checkView(s);
    for(controls::DataTableRow *row : s.pooledRows())
        CHECK(!row->isSelected());
    s.takeBinds();

    // grown: every row still shows its item, and none is bound
    table->setItemCount(2000);
    s.frame();
    checkView(s);
    CHECK(s.takeBinds().empty());

    // shrunk to fewer than fit: every item from the top
    table->setItemCount(4);
    s.frame();
    CHECK(list->getVerticalScrollOffset() == 0);
    checkView(s);
    CHECK(s.shownRows().size() == 4);

    // none
    table->setItemCount(0);
    s.frame();
    CHECK(s.shownRows().empty());
    CHECK(table->rowForItem(0) == nullptr);

    s.checkOnePass();
}

TEST_CASE("DataTable source mode: the header follows the body sideways")
{
    Scene s(1000, 4);
    controls::DataTable *table=s.table;
    controls::ScrollControl *body=table->getBody();

    // 4 x 150 = 600 units of columns in a 398-unit wide body
    for(int c=0; c < 4; ++c)
        table->setColumnWidth(c, 150);
    s.frame();
    s.frame();

    const Rect header=Scene::canvasBounds(table->getHeader());
    const Rect cell0=Scene::canvasBounds(s.cell(0));
    controls::DataTableRow *first=table->rowForItem(0);

    REQUIRE(first != nullptr);
    CHECK(body->getInnerPanel()->getWidth() == 600);
    CHECK(Scene::canvasBounds(first->getCell(0)).x == cell0.x);
    CHECK(Scene::canvasBounds(first).y == header.y+header.h);

    body->scrollToRight();
    s.frame();

    const Rect scrolled0=Scene::canvasBounds(s.cell(0));
    const Rect scrolled3=Scene::canvasBounds(s.cell(3));
    const Rect &viewport=body->getViewport();
    const Point origin=body->localPosToCanvas();

    CHECK((Scene::canvasBounds(table->getHeader()) == header));
    CHECK(scrolled0.x < cell0.x);
    CHECK(Scene::canvasBounds(first->getCell(0)).x == scrolled0.x);
    CHECK(Scene::canvasBounds(first->getCell(3)).x == scrolled3.x);
    CHECK(scrolled3.x+scrolled3.w == origin.x+viewport.x+viewport.w);
    checkView(s);

    // scrolling down keeps the sideways offset
    s.list()->scrollByRows(5);
    s.frame();
    CHECK(Scene::canvasBounds(s.cell(0)).x == scrolled0.x);
    checkView(s);

    body->scrollToLeft();
    s.frame();
    CHECK((Scene::canvasBounds(s.cell(0)) == cell0));
    checkView(s);

    s.checkOnePass();
}

TEST_CASE("DataTable source mode: the rows addRow made go, and the row API does nothing")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);
    canvas->setSize(600, 400);

    controls::DataTable *table=canvas->newChild<controls::DataTable>();
    Counter selections;

    table->setBounds(10, 10, 400, 200);
    table->setColumnCount(2);
    controls::DataTableRow *row=table->addRow();
    table->addRow();
    table->setSelectedRow(row);
    selections.listen(table->onSelectionChange);
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(!table->isSourceMode());
    CHECK(table->getItemCount() == 2);

    table->setItemCount(50);
    CHECK(table->isSourceMode());
    CHECK(table->rowCount() == 0);
    CHECK(table->getRows().empty());
    // the selection the rows held went, with its event
    CHECK(selections.calls == 1);
    CHECK(selections.last == nullptr);
    CHECK(table->getSelectedItem() == controls::DataTable::NoItem);

    CHECK(table->addRow() == nullptr);
    CHECK(table->rowCount() == 0);

    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(table->getItemCount() == 50);
    CHECK(table->rowForItem(0) != nullptr);
    // the row-mode body and its rows are gone
    CHECK(table->getVirtualBody() != nullptr);
    for(controls::Base *child : table->getActualChildren())
        CHECK((child == table->getHeader() || child == table->getVirtualBody()));

    delete canvas;
}
