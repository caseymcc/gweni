/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A ScrollControl keeps its scroll position as a whole-unit offset per axis
 *  (issue #512), not as the bars' fraction: scrolled to an offset, content
 *  that grows keeps it, vertically and horizontally, with and without virtual
 *  content, so the inner panel and every row stay where they are on the canvas
 *  and the bar's amount follows the new content size (offset / (content -
 *  viewport)). Content that shrinks keeps the offset while it still fits and
 *  clamps it to the new end otherwise; content that fits again takes the bar
 *  back to 0. A bar the user moves (the thumb dragged, an arrow button, a
 *  wheel notch) sets the offset at the content size it was moved against,
 *  even when the content changes in the same frame, and the offset it set is
 *  kept when the content grows after. A ListBox scrolled part-way keeps its
 *  rows where they are, and marks none of them, when one is appended, and an
 *  addItem followed by scrollToBottom before any layout (Demo::printText)
 *  ends on the new last row; scrollToRight likewise lands on the new right edge. The offsets are
 *  public (get/set*ScrollOffset, getMax*ScrollOffset) for non-virtual content
 *  too: one set before any layout is clamped by the first pass, a set after
 *  a scrollTo*() cancels it (one that keeps the offset too), and an axis that
 *  cannot scroll has none. On content that overflows by a few units, arrow
 *  nudges and wheel notches of under half a unit each add up rather than
 *  being put back, both ways, to both ends. A VirtualList's scrollToBottom
 *  followed by a new item count lands on the new end, and a count that
 *  shrinks under the offset clamps it.
 */
#include "headlessFixture.h"

#include <gweni/controls/scrollControl.h>
#include <gweni/controls/listBox.h>
#include <gweni/controls/scrollBarBar.h>
#include <gweni/controls/virtualList.h>

#include <doctest/doctest.h>

#include <iterator>
#include <string>

using namespace gweni;

namespace
{

//! A row that asks for a fixed size.
class Block: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Block, controls::Base)
    {}

    Size want;

    void calculateSize(skin::Base * /*skin*/, Dim dim) override
    {
        if(dim == Dim::X)
            m_preferredSize.width=want.width;
        else
            m_preferredSize.height=want.height;
    }
};

//! A control's bounds in canvas coordinates.
Rect onCanvas(controls::Base *control)
{
    const Point origin=control->localPosToCanvas();

    return Rect(origin.x, origin.y, control->getWidth(), control->getHeight());
}

//! A 400x300 canvas holding a ScrollControl that fills it: its viewport is
//! 390x290 with no bar shown (the inner panel's 5 unit margin).
struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::ScrollControl *scroll;
    controls::Base *inner;
    controls::BaseScrollBar *verticalBar;
    controls::BaseScrollBar *horizontalBar;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        scroll=canvas->newChild<controls::ScrollControl>("scroll");
        scroll->setDock(DockPosition::Center);
        parts();
    }

    //! Takes the ScrollControl's parts: the inner panel (sent to back), then
    //! the two bars.
    void parts()
    {
        const controls::Base::List &children=scroll->getActualChildren();

        REQUIRE(children.size() >= 3);
        auto part=children.begin();
        inner=*part++;
        verticalBar=gweni_cast<controls::BaseScrollBar>(*part++);
        horizontalBar=gweni_cast<controls::BaseScrollBar>(*part);
        REQUIRE(verticalBar != nullptr);
        REQUIRE(horizontalBar != nullptr);
    }

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
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

    Block *addRow(int width, int height)
    {
        Block *row=scroll->newChild<Block>();

        row->want=Size(width, height);
        row->setDock(DockPosition::Top);
        return row;
    }

    //! How far the content is scrolled, from where the inner panel is.
    int offsetX() const { return scroll->getViewport().x-inner->getX(); }
    int offsetY() const { return scroll->getViewport().y-inner->getY(); }
};

}//namespace

TEST_CASE("ScrollControl offset: content that grows keeps the vertical offset")
{
    Scene scene;
    scene.scroll->setScroll(false, true);
    Block *first=scene.addRow(50, 400);
    scene.settle();
    REQUIRE_FALSE(scene.verticalBar->hidden());
    REQUIRE(scene.scroll->getViewport().h == 290);

    // half-way down 400 units in a 290 viewport: 55 units
    scene.verticalBar->setScrolledAmount(0.5f, true);
    scene.settle();
    REQUIRE(scene.offsetY() == 55);

    const Rect panel=onCanvas(scene.inner);
    const Rect row=onCanvas(first);

    SUBCASE("a row appended")
    {
        scene.addRow(50, 100);
        scene.settle();
    }

    SUBCASE("the row taller")
    {
        first->want.height=500;
        first->invalidate();
        scene.settle();
    }

    CHECK(scene.offsetY() == 55);
    CHECK(onCanvas(scene.inner).y == panel.y);
    CHECK(onCanvas(first).y == row.y);
    CHECK(onCanvas(first).x == row.x);
    CHECK(scene.inner->getHeight() == 500);
    // the bar shows the offset against the new content: 55 of 210
    CHECK(scene.verticalBar->getScrolledAmount() == doctest::Approx(55.0/210.0));
}

TEST_CASE("ScrollControl offset: content that grows keeps the horizontal offset")
{
    Scene scene;
    scene.scroll->setScroll(true, false);
    Block *first=scene.addRow(600, 20);
    scene.settle();
    REQUIRE_FALSE(scene.horizontalBar->hidden());
    REQUIRE(scene.scroll->getViewport().w == 390);

    // half-way across 600 units in a 390 viewport: 105 units
    scene.horizontalBar->setScrolledAmount(0.5f, true);
    scene.settle();
    REQUIRE(scene.offsetX() == 105);

    const Rect panel=onCanvas(scene.inner);
    const Rect row=onCanvas(first);

    first->want.width=800;
    first->invalidate();
    scene.settle();

    CHECK(scene.offsetX() == 105);
    CHECK(onCanvas(scene.inner).x == panel.x);
    CHECK(onCanvas(first).x == row.x);
    CHECK(onCanvas(first).w == 800);
    // 105 of 410
    CHECK(scene.horizontalBar->getScrolledAmount() == doctest::Approx(105.0/410.0));
}

TEST_CASE("ScrollControl offset: content that grows while scrolled to the end keeps the offset")
{
    Scene scene;
    Block *first=scene.addRow(600, 400);
    scene.settle();
    REQUIRE_FALSE(scene.verticalBar->hidden());
    REQUIRE_FALSE(scene.horizontalBar->hidden());

    scene.scroll->scrollToBottom();
    scene.scroll->scrollToRight();
    scene.settle();

    const Rect &viewport=scene.scroll->getViewport();
    const int endX=600-viewport.w;
    const int endY=400-viewport.h;

    REQUIRE(scene.offsetX() == endX);
    REQUIRE(scene.offsetY() == endY);

    first->want=Size(700, 450);
    first->invalidate();
    scene.settle();

    // no sticking to the end: the offsets stay, the new content's end is past
    // the viewport's
    CHECK(scene.offsetX() == endX);
    CHECK(scene.offsetY() == endY);
    CHECK(scene.horizontalBar->getScrolledAmount() == doctest::Approx(double(endX)/(700-viewport.w)));
    CHECK(scene.verticalBar->getScrolledAmount() == doctest::Approx(double(endY)/(450-viewport.h)));
}

TEST_CASE("ScrollControl offset: content that shrinks keeps the offset while it fits and clamps it otherwise")
{
    Scene scene;
    scene.scroll->setScroll(false, true);
    Block *first=scene.addRow(50, 400);
    scene.settle();

    SUBCASE("still in range")
    {
        // 55 of 110, then 400 -> 350 tall: the end is 60, so 55 stays
        scene.verticalBar->setScrolledAmount(0.5f, true);
        scene.settle();
        REQUIRE(scene.offsetY() == 55);

        first->want.height=350;
        first->invalidate();
        scene.settle();
        CHECK(scene.offsetY() == 55);
        CHECK(scene.verticalBar->getScrolledAmount() == doctest::Approx(55.0/60.0));
    }

    SUBCASE("past the new end")
    {
        // 100 of 110, then 350 tall: clamped to 60, the content's end at the
        // viewport's
        scene.verticalBar->setScrolledAmount(100.f/110.f, true);
        scene.settle();
        REQUIRE(scene.offsetY() == 100);

        first->want.height=350;
        first->invalidate();
        scene.settle();
        CHECK(scene.offsetY() == 60);
        CHECK(scene.inner->getY()+scene.inner->getHeight() == 5+290);
        CHECK(scene.verticalBar->getScrolledAmount() == doctest::Approx(1.0));
    }

    SUBCASE("fitting again")
    {
        scene.verticalBar->setScrolledAmount(0.5f, true);
        scene.settle();
        REQUIRE(scene.offsetY() == 55);

        first->want.height=200;
        first->invalidate();
        scene.settle();
        CHECK(scene.verticalBar->hidden());
        CHECK(scene.verticalBar->getScrolledAmount() == 0);
        CHECK(scene.offsetY() == 0);

        // and the offset does not come back with the bar
        first->want.height=400;
        first->invalidate();
        scene.settle();
        CHECK_FALSE(scene.verticalBar->hidden());
        CHECK(scene.offsetY() == 0);
    }
}

TEST_CASE("ScrollControl offset: virtual content keeps the offset as it grows and clamps it as it shrinks")
{
    Scene scene;
    scene.scroll->setScroll(false, true);
    scene.scroll->setVirtualContentHeight(1000);
    scene.settle();
    REQUIRE_FALSE(scene.verticalBar->hidden());
    REQUIRE(scene.scroll->getViewportHeight() == 290);

    scene.scroll->setVerticalScrollOffset(300);
    scene.settle();
    REQUIRE(scene.scroll->getVerticalScrollOffset() == 300);

    scene.scroll->setVirtualContentHeight(2000);
    scene.settle();
    CHECK(scene.scroll->getVerticalScrollOffset() == 300);
    CHECK(scene.verticalBar->getScrolledAmount() == doctest::Approx(300.0/1710.0));
    // the inner panel is not scrolled with virtual content
    CHECK(scene.offsetY() == 0);

    scene.scroll->setVirtualContentHeight(400);
    scene.settle();
    CHECK(scene.scroll->getVerticalScrollOffset() == 110);
    CHECK(scene.verticalBar->getScrolledAmount() == doctest::Approx(1.0));

    scene.scroll->setVirtualContentHeight(200);
    scene.settle();
    CHECK(scene.verticalBar->hidden());
    CHECK(scene.scroll->getVerticalScrollOffset() == 0);
}

TEST_CASE("ScrollControl offset: virtual content keeps its horizontal offset as the content widens")
{
    Scene scene;
    scene.scroll->setVirtualContentHeight(1000);
    Block *first=scene.addRow(600, 20);
    scene.settle();
    REQUIRE_FALSE(scene.horizontalBar->hidden());
    REQUIRE_FALSE(scene.verticalBar->hidden());

    const int viewportWidth=scene.scroll->getViewport().w;

    scene.horizontalBar->setScrolledAmount(0.5f, true);
    scene.scroll->setVerticalScrollOffset(300);
    scene.settle();

    const int offsetX=scene.offsetX();

    REQUIRE(offsetX > 0);
    REQUIRE(scene.scroll->getVerticalScrollOffset() == 300);

    first->want.width=900;
    first->invalidate();
    scene.scroll->setVirtualContentHeight(1500);
    scene.settle();

    CHECK(scene.offsetX() == offsetX);
    CHECK(scene.scroll->getVerticalScrollOffset() == 300);
    CHECK(scene.horizontalBar->getScrolledAmount() == doctest::Approx(double(offsetX)/(900-viewportWidth)));
}

TEST_CASE("ScrollControl offset: a bar the user moves sets the offset at the content it was moved against")
{
    Scene scene;
    scene.scroll->setScroll(false, true);
    Block *first=scene.addRow(50, 400);
    scene.settle();
    REQUIRE(scene.offsetY() == 0);

    SUBCASE("moved in the frame the content grows")
    {
        // half-way down the 400 units it was moved over: 55, not half of the
        // 500 units the frame lays out
        scene.verticalBar->setScrolledAmount(0.5f, true);
        first->want.height=500;
        first->invalidate();
        scene.settle();
        CHECK(scene.offsetY() == 55);
        CHECK(scene.verticalBar->getScrolledAmount() == doctest::Approx(55.0/210.0));
    }

    SUBCASE("the thumb dragged, then the content grows")
    {
        auto *thumb=gweni_cast<controls::internal::ScrollBarBar>(
            [&]() -> controls::Base *
            {
                for(controls::Base *child : scene.verticalBar->getActualChildren())
                {
                    if(gweni_cast<controls::internal::ScrollBarBar>(child))
                        return child;
                }
                return nullptr;
            }());

        REQUIRE(thumb != nullptr);
        const Rect at=onCanvas(thumb);
        const Point grab(at.x+at.w/2, at.y+at.h/2);

        scene.canvas->inputMouseMoved(grab.x, grab.y, 0, 0);
        scene.canvas->inputMouseButton(0, true);
        scene.canvas->inputMouseMoved(grab.x, grab.y+40, 0, 40);
        scene.canvas->inputMouseButton(0, false);
        scene.settle();

        const int dragged=scene.offsetY();

        REQUIRE(dragged > 0);
        REQUIRE(dragged < 110);

        first->want.height=500;
        first->invalidate();
        scene.settle();
        CHECK(scene.offsetY() == dragged);
    }

    SUBCASE("an arrow button, then the content grows")
    {
        auto *vertical=gweni_cast<controls::VerticalScrollBar>(scene.verticalBar);

        REQUIRE(vertical != nullptr);
        vertical->nudgeDown(vertical);
        scene.settle();

        const int nudged=scene.offsetY();

        REQUIRE(nudged > 0);

        first->want.height=500;
        first->invalidate();
        scene.settle();
        CHECK(scene.offsetY() == nudged);
    }

    SUBCASE("a wheel notch, then the content grows")
    {
        REQUIRE(first->onMouseWheeled(-60));
        scene.settle();

        const int wheeled=scene.offsetY();

        REQUIRE(wheeled > 0);

        first->want.height=500;
        first->invalidate();
        scene.settle();
        CHECK(scene.offsetY() == wheeled);

        // and the next notch goes on from there
        REQUIRE(first->onMouseWheeled(-60));
        scene.settle();
        CHECK(scene.offsetY() > wheeled);
    }
}

TEST_CASE("ScrollControl offset: moves under half a unit add up on content that overflows by a few units")
{
    // 294 tall in the 290 viewport: a range of 4, so a nudge (30 of 294) or
    // a wheel notch moves the bar by under half a unit of it
    Scene scene;
    scene.scroll->setScroll(false, true);
    Block *row=scene.addRow(10, 294);
    scene.settle();
    REQUIRE(scene.scroll->getMaxVerticalScrollOffset() == 4);
    REQUIRE(scene.offsetY() == 0);

    SUBCASE("the arrow buttons")
    {
        auto *vertical=gweni_cast<controls::VerticalScrollBar>(scene.verticalBar);

        REQUIRE(vertical != nullptr);

        int last=0;
        bool moved=false;

        for(int i=0; i < 20 && scene.offsetY() < 4; ++i)
        {
            vertical->nudgeDown(vertical);
            scene.settle();
            CHECK(scene.offsetY() >= last);
            moved=moved || scene.offsetY() > last;
            last=scene.offsetY();
            CHECK(scene.scroll->getVerticalScrollOffset() == last);
        }
        CHECK(moved);
        CHECK(scene.offsetY() == 4);

        for(int i=0; i < 20 && scene.offsetY() > 0; ++i)
        {
            vertical->nudgeUp(vertical);
            scene.settle();
            CHECK(scene.offsetY() <= last);
            last=scene.offsetY();
        }
        CHECK(scene.offsetY() == 0);
    }

    SUBCASE("the wheel")
    {
        for(int i=0; i < 20 && scene.offsetY() < 4; ++i)
        {
            REQUIRE(row->onMouseWheeled(-60));
            scene.settle();
        }
        CHECK(scene.offsetY() == 4);

        for(int i=0; i < 20 && scene.offsetY() > 0; ++i)
        {
            REQUIRE(row->onMouseWheeled(60));
            scene.settle();
        }
        CHECK(scene.offsetY() == 0);
    }

    SUBCASE("a bar left between two units still follows the content")
    {
        auto *vertical=gweni_cast<controls::VerticalScrollBar>(scene.verticalBar);

        REQUIRE(vertical != nullptr);
        vertical->nudgeDown(vertical);
        scene.settle();

        const int nudged=scene.offsetY();

        // the content grows: the offset is kept, the bar shows it exactly
        row->want.height=590;
        row->invalidate();
        scene.settle();
        CHECK(scene.offsetY() == nudged);
        CHECK(scene.verticalBar->getScrolledAmount() == doctest::Approx(nudged/300.0));
    }
}

TEST_CASE("ScrollControl offset: a ListBox scrolled part-way keeps its rows where they are when one is appended")
{
    Scene scene;
    controls::ListBox *list=scene.canvas->newChild<controls::ListBox>();

    list->setBounds(10, 10, 200, 150);
    for(int i=0; i < 30; ++i)
        list->addItem("Row "+std::to_string(i));
    scene.settle();

    scene.scroll=list;
    scene.parts();
    REQUIRE_FALSE(scene.verticalBar->hidden());

    scene.verticalBar->setScrolledAmount(0.5f, true);
    scene.settle();

    const controls::Base::List &rows=list->getTable()->getChildren();
    controls::Base *row=*std::next(rows.begin(), 10);
    const Rect before=onCanvas(row);
    const int offset=scene.offsetY();

    REQUIRE(offset > 0);

    list->addItem("Row 30");

    // the update phase of that frame: the rows already there are not marked
    // (the inner panel does not move, so its subtree is not re-stamped)
    scene.canvas->updateCanvas();

    int marked=0;

    for(controls::Base *each : rows)
    {
        if(each != rows.back() && (each->getStateChange() & controls::StateChange_Geometry))
            ++marked;
    }
    CHECK(marked == 0);
    scene.canvas->renderCanvas();
    scene.settle();

    CHECK(scene.offsetY() == offset);
    CHECK((onCanvas(row) == before));
}

TEST_CASE("ScrollControl offset: scrollToBottom right after addItem ends on the new last row")
{
    Scene scene;
    controls::ListBox *list=scene.canvas->newChild<controls::ListBox>();

    list->setBounds(10, 10, 200, 150);
    for(int i=0; i < 30; ++i)
        list->addItem("Row "+std::to_string(i));
    scene.settle();

    scene.scroll=list;
    scene.parts();
    REQUIRE_FALSE(scene.verticalBar->hidden());

    SUBCASE("from the top")
    {
    }

    SUBCASE("from part-way down")
    {
        scene.verticalBar->setScrolledAmount(0.5f, true);
        scene.settle();
    }

    SUBCASE("from the end")
    {
        list->scrollToBottom();
        scene.settle();
    }

    // as Demo::printText does: no layout between the two
    for(int i=30; i < 33; ++i)
    {
        list->addItem("Row "+std::to_string(i));
        list->scrollToBottom();
        scene.settle();

        const Rect &viewport=list->getViewport();
        const Point origin=list->localPosToCanvas();
        const Rect last=onCanvas(list->getTable()->getChildren().back());

        INFO("row ", i);
        // the inner panel's far edge at the viewport's, the new row inside it
        CHECK(scene.inner->getY()+scene.inner->getHeight() == viewport.y+viewport.h);
        CHECK(last.y+last.h <= origin.y+viewport.y+viewport.h);
        CHECK(last.y >= origin.y+viewport.y);
        CHECK(scene.verticalBar->getScrolledAmount() == doctest::Approx(1.0));
    }
}

TEST_CASE("ScrollControl offset: scrollToRight in the frame the content widens lands on the new edge")
{
    Scene scene;
    scene.scroll->setScroll(true, false);
    Block *first=scene.addRow(600, 20);
    scene.settle();

    scene.horizontalBar->setScrolledAmount(0.5f, true);
    scene.settle();
    REQUIRE(scene.offsetX() == 105);

    first->want.width=800;
    first->invalidate();
    scene.scroll->scrollToRight();
    scene.settle();
    CHECK(scene.offsetX() == 800-390);
    CHECK(scene.inner->getX()+scene.inner->getWidth() == 5+390);

    first->want.width=900;
    first->invalidate();
    scene.scroll->scrollToLeft();
    scene.settle();
    CHECK(scene.offsetX() == 0);
}

TEST_CASE("ScrollControl offset: set*ScrollOffset place non-virtual content, clamped by the next pass")
{
    Scene scene;
    scene.addRow(600, 400);

    // before any layout: kept until the first pass, which clamps it
    scene.scroll->setHorizontalScrollOffset(1000);
    scene.scroll->setVerticalScrollOffset(40);
    CHECK(scene.scroll->getHorizontalScrollOffset() == 1000);
    scene.settle();

    const Rect &viewport=scene.scroll->getViewport();

    CHECK(scene.scroll->getMaxHorizontalScrollOffset() == 600-viewport.w);
    CHECK(scene.scroll->getMaxVerticalScrollOffset() == 400-viewport.h);
    CHECK(scene.scroll->getHorizontalScrollOffset() == 600-viewport.w);
    CHECK(scene.offsetX() == 600-viewport.w);
    CHECK(scene.scroll->getVerticalScrollOffset() == 40);
    CHECK(scene.offsetY() == 40);
    CHECK(scene.verticalBar->getScrolledAmount() == doctest::Approx(40.0/(400-viewport.h)));

    // the getters follow a bar the user moved once a pass has read it
    scene.horizontalBar->setScrolledAmount(0.f, true);
    scene.settle();
    CHECK(scene.scroll->getHorizontalScrollOffset() == 0);

    // an offset set after a scrollTo*() cancels it; one before is overridden
    scene.scroll->scrollToBottom();
    scene.scroll->setVerticalScrollOffset(10);
    scene.scroll->setHorizontalScrollOffset(20);
    scene.scroll->scrollToRight();
    scene.settle();
    CHECK(scene.scroll->getVerticalScrollOffset() == 10);
    CHECK(scene.scroll->getHorizontalScrollOffset() == 600-viewport.w);

    // even one that sets the offset it already has
    scene.scroll->scrollToBottom();
    scene.scroll->setVerticalScrollOffset(10);
    scene.scroll->scrollToLeft();
    scene.scroll->setHorizontalScrollOffset(600-viewport.w);
    scene.settle();
    CHECK(scene.scroll->getVerticalScrollOffset() == 10);
    CHECK(scene.scroll->getHorizontalScrollOffset() == 600-viewport.w);

    // an axis that cannot scroll has no offset
    scene.scroll->setScroll(false, true);
    scene.settle();
    CHECK(scene.scroll->getHorizontalScrollOffset() == 0);
    CHECK(scene.scroll->getMaxHorizontalScrollOffset() == 0);
    CHECK(scene.inner->getX() == viewport.x);
}

TEST_CASE("ScrollControl offset: a VirtualList's scrollToBottom before a count change lands on the new end")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);

    canvas->setSize(400, 400);

    controls::VirtualList *list=canvas->newChild<controls::VirtualList>();

    list->setPos(0, 0);
    list->setSize(200, 160);
    list->setRowHeight(20);
    list->setItemCount(100);
    canvas->updateCanvas();
    canvas->renderCanvas();
    REQUIRE(list->getVerticalScrollOffset() == 0);

    // a log view: scroll to the end, then append
    list->scrollToBottom();
    list->setItemCount(101);
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(list->getVerticalScrollOffset() == 101*20-160);

    // a count that shrinks under the offset clamps it at once
    list->setItemCount(50);
    CHECK(list->getVerticalScrollOffset() == 50*20-160);
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(list->getVerticalScrollOffset() == 50*20-160);
    delete canvas;
}
