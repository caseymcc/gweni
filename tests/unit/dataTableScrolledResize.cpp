/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A DataTable whose columns overflow its body, scrolled sideways part-way
 *  and all the way right, given a wider column (setColumnWidth, and a drag of
 *  its header grip), in row mode and in source mode (issue #512): the body
 *  keeps its horizontal scroll offset, so the dragged grip moves on the canvas
 *  by exactly the width change, the cells left of the column (header and
 *  body) keep their canvas bounds, the inner panel and the header keep their
 *  x, and every header cell stays over its body cells. Scrolled all the way
 *  right, that leaves the new right end past the viewport (no sticking to
 *  the end); the bar's amount follows the wider content.
 */
#include "headlessFixture.h"

#include <gweni/controls/dataTable.h>
#include <gweni/controls/horizontalScrollBar.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/virtualList.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <map>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

constexpr int Columns=4;
constexpr int ColumnWidth=150;
constexpr int RowHeight=20;

enum class Mode { Rows, Source };
enum class Scrolled { Middle, Right };

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::DataTable *table=nullptr;
    float clock=10.0f;

    explicit Scene(Mode mode)
    {
        // a second apart per press, so two are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(600, 400);

        table=canvas->newChild<controls::DataTable>();
        table->setBounds(10, 10, 400, 200);
        table->setColumnCount(Columns);
        for(int c=0; c < Columns; ++c)
            table->setColumnCaption(c, "Column "+std::to_string(c));

        if(mode == Mode::Rows)
        {
            for(int r=0; r < 40; ++r)
            {
                controls::DataTableRow *row=table->addRow();

                for(int c=0; c < Columns; ++c)
                    row->setCellText(c, "r"+std::to_string(r)+"c"+std::to_string(c));
            }
        }
        else
        {
            table->setRowHeight(RowHeight);
            table->setRowBinder([](controls::DataTableRow *row, size_t item)
                {
                    for(int c=0; c < row->getCellCount(); ++c)
                        row->setCellText(c, "r"+std::to_string(item)+"c"+std::to_string(c));
                });
            table->setItemCount(1000);
        }

        // 4 x 150 = 600 units of columns in a 400-unit wide table: the body
        // scrolls both ways
        for(int c=0; c < Columns; ++c)
            table->setColumnWidth(c, ColumnWidth);
        settle();
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

    int frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats().passes;
    }

    void settle()
    {
        for(int i=0; i < 10 && frame() != 0; ++i)
            ;
    }

    controls::ScrollControl *body() const { return table->getBody(); }

    controls::HorizontalScrollBar *horizontalBar() const
    {
        for(controls::Base *child : body()->getActualChildren())
        {
            if(auto *bar=gweni_cast<controls::HorizontalScrollBar>(child))
                return bar;
        }
        return nullptr;
    }

    controls::internal::DataTableHeaderCell *cell(int column) const
    {
        return table->getHeader()->getCells().at(column);
    }

    //! The body rows on screen: every row in row mode, the shown pooled ones
    //! in source mode.
    std::vector<controls::DataTableRow *> rows() const
    {
        if(!table->isSourceMode())
            return table->getRows();

        std::vector<controls::DataTableRow *> shown;

        for(controls::Base *control : table->getVirtualBody()->getRows())
        {
            controls::DataTableRow *row=gweni_cast<controls::DataTableRow>(control);

            REQUIRE(row != nullptr);
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

    //! How far the body is scrolled sideways, from where its inner panel is.
    int offsetX() const
    {
        return body()->getViewport().x-body()->getInnerPanel()->getX();
    }
};

void checkWidened(Mode mode, Scrolled scrolled, int column, bool drag)
{
    CAPTURE(int(mode));
    CAPTURE(int(scrolled));
    CAPTURE(column);
    CAPTURE(drag);

    Scene scene(mode);
    controls::DataTable *table=scene.table;
    controls::ScrollControl *body=scene.body();
    controls::HorizontalScrollBar *bar=scene.horizontalBar();

    REQUIRE(bar != nullptr);
    REQUIRE_FALSE(bar->hidden());
    REQUIRE(table->contentWidth() == Columns*ColumnWidth);

    const int viewportWidth=body->getViewport().w;
    const int end=table->contentWidth()-viewportWidth;

    REQUIRE(end > 0);

    if(scrolled == Scrolled::Middle)
        bar->setScrolledAmount(0.5f, true);
    else
        body->scrollToRight();
    scene.settle();

    const int offset=scene.offsetX();

    if(scrolled == Scrolled::Right)
        REQUIRE(offset == end);
    else
        REQUIRE((offset > 0 && offset < end));

    const std::vector<controls::DataTableRow *> rows=scene.rows();

    REQUIRE_FALSE(rows.empty());

    const int grip=Scene::canvasBounds(scene.cell(column)->getGrip()).x;
    const int panelX=body->getInnerPanel()->getX();
    const Rect header=Scene::canvasBounds(table->getHeader());
    std::map<controls::Base *, Rect> kept;

    for(int c=0; c < column; ++c)
    {
        kept[scene.cell(c)]=Scene::canvasBounds(scene.cell(c));
        for(controls::DataTableRow *row : rows)
            kept[row->getCell(c)]=Scene::canvasBounds(row->getCell(c));
    }

    const int width=table->getColumnLaidOutWidth(column);

    if(drag)
        scene.dragGrip(column, 20);
    else
        table->setColumnWidth(column, width+20);
    scene.settle();

    const int change=table->getColumnLaidOutWidth(column)-width;

    CHECK(change == 20);
    CHECK(scene.offsetX() == offset);
    CHECK(body->getInnerPanel()->getX() == panelX);
    CHECK(Scene::canvasBounds(scene.cell(column)->getGrip()).x == grip+change);
    CHECK((Scene::canvasBounds(table->getHeader()) == header));
    CHECK(bar->getScrolledAmount() == doctest::Approx(double(offset)/(end+change)));

    // the cells left of the column that moved or changed size, and the body
    // cells not under their header cell
    int moved=0;
    int misaligned=0;

    for(const auto &entry : kept)
    {
        if(!(Scene::canvasBounds(entry.first) == entry.second))
            ++moved;
    }

    for(controls::DataTableRow *row : scene.rows())
    {
        for(int c=0; c < Columns; ++c)
        {
            const Rect at=Scene::canvasBounds(row->getCell(c));
            const Rect under=Scene::canvasBounds(scene.cell(c));

            if(at.x != under.x || at.w != under.w)
                ++misaligned;
        }
    }

    CHECK(moved == 0);
    CHECK(misaligned == 0);
}

}//namespace

TEST_CASE("DataTable scrolled sideways: a wider column moves its grip by the change and leaves the columns left of it")
{
    for(Mode mode : {Mode::Rows, Mode::Source})
    {
        for(Scrolled scrolled : {Scrolled::Middle, Scrolled::Right})
        {
            for(int column : {1, 2})
            {
                for(bool drag : {false, true})
                    checkWidened(mode, scrolled, column, drag);
            }
        }
    }
}
