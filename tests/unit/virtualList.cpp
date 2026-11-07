/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  VirtualList on a real Canvas with the StubSkin: the pool is the viewport's
 *  rows plus one, whatever the item count, so 1 000 and 100 000 items take
 *  the same controls and ids; scrolling by a row moves the rows and binds the
 *  one that came into view, a page or a jump binds the rows whose item
 *  changed, a resize resizes the pool, and every shown row sits at its
 *  item's place in the viewport (so nothing is far off screen). A count that
 *  shrinks while scrolled to the end keeps the view full and valid, one that
 *  grows binds nothing that still shows its item, and scrollToItem puts the
 *  item fully in view. Also ScrollControl's virtual content under it: the
 *  vertical bar ranged from the virtual height, the offset clamped to it, and
 *  a bar the user moves setting the offset.
 */
#include "headlessFixture.h"

#include <gweni/controls/verticalScrollBar.h>
#include <gweni/controls/virtualList.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <map>
#include <set>
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
    controls::VirtualList *list=nullptr;

    //! Every bind, in call order.
    std::vector<std::pair<controls::Base *, size_t>> binds;
    //! The item each row was last bound to.
    std::map<controls::Base *, size_t> boundTo;

    explicit Scene(size_t count, int height=100)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 600);

        list=canvas->newChild<controls::VirtualList>();
        list->setPos(0, 0);
        list->setSize(200, height);
        list->setRowHeight(RowHeight);
        list->setRowBinder([this](controls::Base *row, size_t index)
            {
                binds.push_back({row, index});
                boundTo[row]=index;
            });
        list->setItemCount(count);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    //! Over every frame: layout requests recorded by the library (none), the
    //! most update-phase passes one took (one) and render-phase layouts (none).
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

    //! Every frame so far laid out in one pass, with no request.
    void checkOnePass() const
    {
        CHECK(requests == 0);
        CHECK(maxPasses <= 1);
        CHECK(!renderLayout);
    }

    //! Binds made by the frames since the last call.
    std::vector<std::pair<controls::Base *, size_t>> takeBinds()
    {
        std::vector<std::pair<controls::Base *, size_t>> taken;

        taken.swap(binds);
        return taken;
    }

    std::vector<controls::Base *> shownRows() const
    {
        std::vector<controls::Base *> shown;

        for(controls::Base *row : list->getRows())
        {
            if(!row->hidden())
                shown.push_back(row);
        }
        return shown;
    }

    controls::VerticalScrollBar *bar() const
    {
        for(controls::Base *child : list->getActualChildren())
        {
            if(auto *bar=gweni_cast<controls::VerticalScrollBar>(child))
                return bar;
        }
        return nullptr;
    }
};

//! Every item from the one at the top of the viewport to the last one in it
//! (or the last item) is shown by exactly one row, bound to that item and
//! placed at its place; no other row is shown.
void checkView(Scene &s)
{
    controls::VirtualList *list=s.list;
    const int offset=list->getVerticalScrollOffset();
    const int viewport=list->getViewportHeight();
    const size_t count=list->getItemCount();

    REQUIRE(viewport > 0);
    CHECK(offset >= 0);
    CHECK(offset <= list->getMaxVerticalScrollOffset());

    const size_t first=offset/RowHeight;
    const size_t last=std::min<size_t>(count, (offset+viewport+RowHeight-1)/RowHeight);
    std::set<controls::Base *> expected;

    for(size_t item=first; item < last; ++item)
    {
        controls::Base *row=list->rowForItem(item);

        INFO("item ", item, " offset ", offset);
        REQUIRE(row != nullptr);
        CHECK(!row->hidden());
        CHECK(list->itemForRow(row) == item);
        CHECK(s.boundTo[row] == item);
        CHECK(row->getY() == static_cast<int>(item*RowHeight)-offset);
        CHECK(row->getHeight() == RowHeight);
        CHECK(row->getWidth() == list->getInnerPanel()->getWidth());
        expected.insert(row);
    }

    for(controls::Base *row : s.shownRows())
    {
        INFO("shown row for item ", list->itemForRow(row));
        CHECK(list->itemForRow(row) < count);
        // on screen, however far down the list is
        CHECK(row->getY() > -RowHeight);
        CHECK(row->getY() < viewport);
    }

    // a row whose item is not in view (the spare one past the end) is hidden
    for(controls::Base *row : s.shownRows())
    {
        const size_t item=list->itemForRow(row);

        if(item < first || item >= last)
            FAIL("row of item ", item, " shown outside ", first, "..", last);
    }
    CHECK(s.shownRows().size() == expected.size());
}

}//namespace

TEST_CASE("VirtualList: the pool is the viewport's rows plus one")
{
    const struct
    {
        int height;
        size_t pool;
    } cases[]={{100, 6}, {110, 7}, {20, 2}, {19, 2}, {21, 3}, {300, 16}};

    for(const auto &c : cases)
    {
        Scene s(1000, c.height);

        INFO("viewport ", c.height);
        CHECK(s.list->getViewportHeight() == c.height);
        CHECK(s.list->getRows().size() == c.pool);
        checkView(s);
        s.checkOnePass();
    }
}

TEST_CASE("VirtualList: 1 000 and 100 000 items take the same controls and ids")
{
    struct Taken
    {
        size_t controls;
        size_t primitives;
        size_t texts;
        size_t binds;
    };

    auto take=[](size_t count)
    {
        test::IdBankCounts before=test::IdBankCounts::now();
        Scene s(count, 200);
        test::IdBankCounts after=test::IdBankCounts::now();

        checkView(s);
        return Taken{s.skin.liveControls, after.livePrimitives()-before.livePrimitives(),
            after.liveTexts()-before.liveTexts(), s.takeBinds().size()};
    };

    const Taken small=take(1000);
    const Taken large=take(100000);

    CHECK(small.controls == large.controls);
    CHECK(small.primitives == large.primitives);
    CHECK(small.texts == large.texts);
    // the rows on screen, bound once each
    CHECK(small.binds == 10);
    CHECK(large.binds == 10);
}

TEST_CASE("VirtualList: scrolling binds only the rows whose item changed")
{
    Scene s(100000);
    s.takeBinds();
    struct OnePass
    {
        Scene &s;
        ~OnePass() { s.checkOnePass(); }
    } onePass{s};

    SUBCASE("one row down and back up binds one row each")
    {
        controls::Base *top=s.list->rowForItem(0);

        s.list->scrollByRows(1);
        s.frame();

        auto binds=s.takeBinds();
        REQUIRE(binds.size() == 1);
        // the row that went out at the top comes in at the bottom
        CHECK(binds[0].first == top);
        CHECK(binds[0].second == 5);
        CHECK(s.list->firstVisibleItem() == 1);
        checkView(s);

        s.list->scrollByRows(-1);
        s.frame();
        binds=s.takeBinds();
        REQUIRE(binds.size() == 1);
        CHECK(binds[0].second == 0);
        checkView(s);
    }

    SUBCASE("a page binds the rows that came into view")
    {
        s.list->scrollByRows(5);
        s.frame();
        // items 0..5 were bound (6 rows), 5 stays: 5..10 needs 6..10
        CHECK(s.takeBinds().size() == 5);
        checkView(s);
    }

    SUBCASE("a part-row offset shows one more row, bound once")
    {
        s.list->setVerticalScrollOffset(10);
        s.frame();
        CHECK(s.takeBinds().size() == 1);
        CHECK(s.shownRows().size() == 6);
        checkView(s);

        // a move that shows the same items binds nothing
        s.list->setVerticalScrollOffset(15);
        s.frame();
        CHECK(s.takeBinds().empty());
        checkView(s);
    }

    SUBCASE("a jump to the middle and to the end bind every row")
    {
        s.list->scrollToItem(50000);
        s.frame();
        CHECK(s.takeBinds().size() == 5);
        checkView(s);
        CHECK(s.list->rowForItem(50000) != nullptr);

        s.list->setVerticalScrollOffset(INT_MAX);
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == 100000*RowHeight-100);
        CHECK(s.list->getMaxVerticalScrollOffset() == 100000*RowHeight-100);
        CHECK(s.takeBinds().size() == 5);
        CHECK(s.list->rowForItem(99999) != nullptr);
        CHECK(s.list->rowForItem(99999)->getY() == 80);
        checkView(s);
    }

    SUBCASE("the wheel scrolls three rows a notch, and passes on what it cannot use")
    {
        CHECK(s.list->onMouseWheeled(-60));
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == 3*RowHeight);
        CHECK(s.takeBinds().size() == 3);
        checkView(s);

        CHECK(s.list->onMouseWheeled(60));
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == 0);
        // at the top already: nothing to use it for
        CHECK(!s.list->onMouseWheeled(60));
    }

    SUBCASE("a settled frame after a scroll lays out and binds nothing")
    {
        s.list->scrollByRows(1);
        s.frame();
        s.takeBinds();
        controls::resetChangeStats();
        s.frame();
        CHECK(s.takeBinds().empty());
        CHECK(s.canvas->getLayoutStats().passes == 0);
        CHECK(s.canvas->getUpdateStats().zVisited == 0);
        CHECK(controls::getChangeStats().controls == 0);
    }

    SUBCASE("a one-row scroll lays out in one pass and renumbers nothing")
    {
        s.list->scrollByRows(1);
        controls::resetChangeStats();
        s.frame();
        CHECK(s.canvas->getLayoutStats().passes == 1);
        CHECK(s.canvas->getUpdateStats().zVisited == 0);
        // the shown rows moved, and the bar's thumb; nothing else
        CHECK(controls::getChangeStats().controls <= s.list->getRows().size()+4);
    }
}

TEST_CASE("VirtualList: a resize resizes the pool and keeps the offset")
{
    Scene s(1000);

    s.list->scrollByRows(10);
    s.frame();
    s.takeBinds();

    s.list->setSize(200, 200);
    s.frame();
    CHECK(s.list->getRows().size() == 11);
    CHECK(s.list->firstVisibleItem() == 10);
    // the five rows that stayed keep their items; the five new ones are bound
    CHECK(s.takeBinds().size() == 5);
    checkView(s);

    s.list->setSize(200, 60);
    s.frame();
    CHECK(s.list->getRows().size() == 4);
    CHECK(s.list->firstVisibleItem() == 10);
    CHECK(s.takeBinds().empty());
    checkView(s);

    // taller than the rest of the list: the offset comes up to show a full view
    s.list->setVerticalScrollOffset(s.list->getMaxVerticalScrollOffset());
    s.frame();
    s.takeBinds();
    s.list->setSize(200, 300);
    s.frame();
    CHECK(s.list->getVerticalScrollOffset() == 1000*RowHeight-300);
    checkView(s);
    s.checkOnePass();
}

TEST_CASE("VirtualList: the item count changing keeps the view valid")
{
    Scene s(1000);
    struct OnePass
    {
        Scene &s;
        ~OnePass() { s.checkOnePass(); }
    } onePass{s};

    SUBCASE("shrinking while scrolled to the end")
    {
        s.list->setVerticalScrollOffset(s.list->getMaxVerticalScrollOffset());
        s.frame();
        s.takeBinds();

        s.list->setItemCount(500);
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == 500*RowHeight-100);
        CHECK(s.list->rowForItem(499) != nullptr);
        CHECK(s.list->rowForItem(999) == nullptr);
        checkView(s);

        // to fewer than fit: every item shows, from the top
        s.list->setItemCount(3);
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == 0);
        CHECK(s.list->getMaxVerticalScrollOffset() == 0);
        CHECK(s.shownRows().size() == 3);
        checkView(s);

        s.list->setItemCount(0);
        s.frame();
        CHECK(s.shownRows().empty());
        CHECK(s.list->firstVisibleItem() == 0);
    }

    SUBCASE("growing binds nothing that still shows its item")
    {
        s.list->scrollByRows(3);
        s.frame();
        s.takeBinds();

        s.list->setItemCount(2000);
        s.frame();
        CHECK(s.takeBinds().empty());
        checkView(s);
    }

    SUBCASE("growing from a short list binds only the new items")
    {
        s.list->setItemCount(2);
        s.frame();
        s.takeBinds();

        s.list->setItemCount(4);
        s.frame();
        auto binds=s.takeBinds();
        REQUIRE(binds.size() == 2);
        std::set<size_t> items{binds[0].second, binds[1].second};
        CHECK(items == std::set<size_t>{2, 3});
        checkView(s);
    }

    SUBCASE("refresh binds the rows on screen again, refreshItem one of them")
    {
        s.takeBinds();
        s.list->refresh();
        s.frame();
        CHECK(s.takeBinds().size() == 5);

        s.list->refreshItem(2);
        s.frame();
        auto binds=s.takeBinds();
        REQUIRE(binds.size() == 1);
        CHECK(binds[0].second == 2);

        // not on screen: nothing to bind
        s.list->refreshItem(500);
        s.frame();
        CHECK(s.takeBinds().empty());
    }
}

TEST_CASE("VirtualList: scrollToItem puts the item fully in view")
{
    Scene s(1000);

    for(size_t item : {size_t(7), size_t(3), size_t(500), size_t(499), size_t(999), size_t(0)})
    {
        s.list->scrollToItem(item);
        s.frame();

        INFO("item ", item);
        controls::Base *row=s.list->rowForItem(item);
        REQUIRE(row != nullptr);
        CHECK(row->getY() >= 0);
        CHECK(row->getY()+RowHeight <= s.list->getViewportHeight());
        checkView(s);
    }

    // in view already: it stays where it is
    s.list->setVerticalScrollOffset(10);
    s.frame();
    s.list->scrollToItem(2);
    CHECK(s.list->getVerticalScrollOffset() == 10);
    // past the end: nothing
    s.list->scrollToItem(1000);
    CHECK(s.list->getVerticalScrollOffset() == 10);
}

TEST_CASE("VirtualList: the vertical bar is ranged from the virtual content")
{
    Scene s(100000);
    controls::VerticalScrollBar *bar=s.bar();

    REQUIRE(bar != nullptr);
    CHECK(!bar->hidden());
    CHECK(s.list->getVirtualContentHeight() == 100000*RowHeight);
    // the inner panel is the viewport, never the content's height
    CHECK(s.list->getInnerPanel()->getHeight() == 100);
    CHECK(s.list->getInnerPanel()->getY() == 0);

    SUBCASE("the offset shows on the bar")
    {
        s.list->setVerticalScrollOffset(s.list->getMaxVerticalScrollOffset()/2);
        s.frame();
        CHECK(bar->getScrolledAmount() == doctest::Approx(0.5f).epsilon(1e-6));
        CHECK(s.list->getInnerPanel()->getY() == 0);
    }

    SUBCASE("a bar the user moves sets the offset")
    {
        bar->setScrolledAmount(0.25f, true);
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == static_cast<int>(std::lround(
            0.25*s.list->getMaxVerticalScrollOffset())));
        checkView(s);
    }

    SUBCASE("content that fits hides the bar and scrolls back")
    {
        s.list->setVerticalScrollOffset(1000);
        s.frame();
        s.list->setItemCount(4);
        s.frame();
        CHECK(bar->hidden());
        CHECK(s.list->getVerticalScrollOffset() == 0);
        checkView(s);
    }

    SUBCASE("the offset is clamped to the content")
    {
        s.list->setVerticalScrollOffset(-50);
        CHECK(s.list->getVerticalScrollOffset() == 0);
        s.list->setVerticalScrollOffset(INT_MAX);
        CHECK(s.list->getVerticalScrollOffset() == s.list->getMaxVerticalScrollOffset());
    }
}

TEST_CASE("VirtualList: deleting it gives back every id and control")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);

    canvas->setSize(400, 400);
    canvas->updateCanvas();
    canvas->renderCanvas();

    const test::IdBankCounts before=test::IdBankCounts::now();
    const size_t controlsBefore=skin.liveControls;

    controls::VirtualList *list=canvas->newChild<controls::VirtualList>();

    list->setItemCount(100000);
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(list->getRows().size() == 9);    // the default 160 high

    delete list;
    canvas->updateCanvas();
    canvas->renderCanvas();

    const test::IdBankCounts after=test::IdBankCounts::now();

    CHECK(after.livePrimitives() == before.livePrimitives());
    CHECK(after.liveTexts() == before.liveTexts());
    CHECK(skin.liveControls == controlsBefore);
    CHECK(renderer.doubleAcquired == 0);
    CHECK(renderer.doubleReleased == 0);
    delete canvas;
}

TEST_CASE("VirtualList: a pressed default row fires onItemPressed with its item")
{
    struct Receiver: event::Handler
    {
        std::vector<size_t> items;
        void pressed(event::Info info) { items.push_back(info.integer); }
    } receiver;

    // far from any other test's click, so it is never a double click
    float clock=1000.0f;
    platform::setTimeSource([&clock]() { return clock; });

    Scene s(1000);

    s.list->onItemPressed.add(&receiver, &Receiver::pressed);
    s.list->scrollByRows(4);
    s.frame();

    auto *row=gweni_cast<controls::internal::VirtualListRow>(s.list->rowForItem(6));

    REQUIRE(row != nullptr);
    const Point at=row->localPosToCanvas();

    s.canvas->inputMouseMoved(at.x+10, at.y+10, 0, 0);
    s.canvas->inputMouseButton(0, true);
    s.canvas->inputMouseButton(0, false);
    s.frame();

    REQUIRE(receiver.items.size() == 1);
    CHECK(receiver.items[0] == 6);
    // a press never leaves the row selected: it shows another item later
    CHECK(!row->isSelected());

    input::onMouseClicked(s.canvas, 0, false);
    platform::setTimeSource(nullptr);
}
