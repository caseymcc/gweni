/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  VirtualList's variable mode (an item measurer) on a real Canvas with the
 *  StubSkin, the items 10 to 50 units high. After every layout the rows on
 *  screen sit one below the other from the anchor (the item at the viewport's
 *  top and the offset into it), each at its item's offset in the height index
 *  less the scroll offset and as high as the measurer says, measured at this
 *  generation at the rows' width; no other row shows; the content height is
 *  the index's sum and the anchor is the item at the offset. That holds at
 *  the top, after an offset set by the app, a bar moved by the user, the
 *  wheel, scrollByRows (the anchor moved by whole items, its offset kept),
 *  scrollToItem below, above and inside the view, and at the end (the last
 *  item's bottom at the viewport's). The pool is ceil(viewport/smallest
 *  measured height)+1, and 1 000 and 100 000 items take the same controls,
 *  ids and binds. An item not measured counts at the row height before any
 *  was measured, then at the last measured height, or at the estimator's
 *  height for one added after it was set, and the measurer is given the inner
 *  panel's width. The vertical bar shows (disabled) even when everything
 *  fits, and a list given back its fixed mode hides it again and places
 *  every row at index*rowHeight. Every frame is one layout pass.
 */
#include "headlessFixture.h"

#include <gweni/controls/verticalScrollBar.h>
#include <gweni/controls/virtualList.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <climits>
#include <map>
#include <set>
#include <vector>

using namespace gweni;

namespace
{

constexpr int RowHeight=20;

int itemHeight(size_t index)
{
    return 10+static_cast<int>((index*37)%41);
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::VirtualList *list=nullptr;

    std::map<controls::Base *, size_t> boundTo;
    size_t bindCount=0;
    //! the widths the measurer was given
    std::set<int> widths;
    size_t measures=0;

    //! A list of count items at the given viewport height; frame() lays it out
    //! unless built is false.
    explicit Scene(size_t count, int height=200, bool layOut=true)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 600);

        list=canvas->newChild<controls::VirtualList>();
        list->setPos(0, 0);
        list->setSize(200, height);
        list->setRowHeight(RowHeight);
        list->setRowBinder([this](controls::Base *row, size_t index)
            {
                boundTo[row]=index;
                ++bindCount;
            });
        list->setItemMeasurer([this](size_t index, int width)
            {
                widths.insert(width);
                ++measures;
                return itemHeight(index);
            });
        list->setItemCount(count);
        if(layOut)
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

//! The rows on screen are the items from the anchor down, each at its index
//! offset less the scroll offset and its measured height; nothing else shows.
void checkView(Scene &s)
{
    controls::VirtualList *list=s.list;
    const controls::HeightIndex &index=list->getHeightIndex();
    const int offset=list->getVerticalScrollOffset();
    const int viewport=list->getViewportHeight();
    const size_t count=list->getItemCount();

    REQUIRE(viewport > 0);
    REQUIRE(index.size() == count);
    CHECK(index.checkInvariants());
    CHECK(offset >= 0);
    CHECK(offset <= list->getMaxVerticalScrollOffset());
    CHECK(list->getVirtualContentHeight() == static_cast<int>(std::min<int64_t>(index.totalHeight(), INT_MAX)));

    if(count == 0)
    {
        CHECK(s.shownRows().empty());
        return;
    }

    int64_t top=0;
    const size_t first=index.itemAt(offset, &top);

    CHECK(list->getAnchorItem() == first);
    CHECK(list->getAnchorOffset() == offset-top);
    CHECK(list->firstVisibleItem() == first);

    std::set<controls::Base *> expected;

    for(size_t item=first; item < count && index.offsetOf(item)-offset < viewport; ++item)
    {
        controls::Base *row=list->rowForItem(item);

        INFO("item ", item, " offset ", offset);
        REQUIRE(row != nullptr);
        CHECK(!row->hidden());
        CHECK(list->itemForRow(row) == item);
        CHECK(s.boundTo[row] == item);
        CHECK(index.height(item) == itemHeight(item));
        CHECK(index.generation(item) == list->getMeasureGeneration());
        CHECK(row->getY() == static_cast<int>(index.offsetOf(item)-offset));
        CHECK(row->getHeight() == itemHeight(item));
        CHECK(row->getWidth() == list->getInnerPanel()->getWidth());
        expected.insert(row);
    }

    CHECK(s.shownRows().size() == expected.size());
    for(controls::Base *row : s.shownRows())
        CHECK(expected.count(row) == 1);
}

//! The smallest height the index holds measured at this generation.
int smallestMeasured(const controls::VirtualList *list)
{
    const controls::HeightIndex &index=list->getHeightIndex();
    int smallest=INT_MAX;

    for(size_t i=0; i < index.size(); ++i)
    {
        if(index.generation(i) != 0)
            smallest=std::min(smallest, index.height(i));
    }
    return smallest;
}

}//namespace

TEST_CASE("VirtualList variable: rows sit one below the other at their measured heights")
{
    Scene s(1000);

    CHECK(s.list->isVariableHeight());
    CHECK(s.list->getVerticalScrollOffset() == 0);
    checkView(s);

    SUBCASE("an offset the app sets")
    {
        s.list->setVerticalScrollOffset(1234);
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == 1234);
        checkView(s);
    }

    SUBCASE("the bar moved by the user")
    {
        s.bar()->setScrolledAmount(0.5f, true);
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() > 0);
        checkView(s);
    }

    SUBCASE("the wheel")
    {
        s.list->onMouseWheeled(-60);
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == 3*RowHeight);
        checkView(s);
        s.list->onMouseWheeled(60);
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == 0);
        checkView(s);
    }

    SUBCASE("scrollByRows moves the anchor by items and keeps its offset")
    {
        s.list->setVerticalScrollOffset(static_cast<int>(s.list->getHeightIndex().offsetOf(20))+3);
        s.frame();

        const size_t anchor=s.list->getAnchorItem();
        const int into=s.list->getAnchorOffset();

        REQUIRE(into > 0);
        s.list->scrollByRows(5);
        s.frame();
        CHECK(s.list->getAnchorItem() == anchor+5);
        CHECK(s.list->getAnchorOffset() == std::min(into, itemHeight(anchor+5)-1));
        checkView(s);
        s.list->scrollByRows(-7);
        s.frame();
        CHECK(s.list->getAnchorItem() == anchor-2);
        checkView(s);
    }

    SUBCASE("scrollToItem below, above and inside the view")
    {
        const controls::HeightIndex &index=s.list->getHeightIndex();
        const int viewport=s.list->getViewportHeight();

        s.list->scrollToItem(500);
        s.frame();
        checkView(s);
        // at the viewport's bottom
        CHECK(index.offsetOf(501)-s.list->getVerticalScrollOffset() == viewport);

        s.list->scrollToItem(490);
        s.frame();
        checkView(s);
        // at its top
        CHECK(index.offsetOf(490) == s.list->getVerticalScrollOffset());

        const int before=s.list->getVerticalScrollOffset();

        s.list->scrollToItem(491);
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == before);
        checkView(s);

        s.list->scrollToItem(0);
        s.frame();
        CHECK(s.list->getVerticalScrollOffset() == 0);
        checkView(s);
    }

    SUBCASE("the end")
    {
        s.list->setVerticalScrollOffset(INT_MAX);
        s.frame();
        checkView(s);

        const controls::HeightIndex &index=s.list->getHeightIndex();

        // the last item's bottom at the viewport's, every item there measured
        CHECK(index.totalHeight()-s.list->getVerticalScrollOffset() == s.list->getViewportHeight());
        CHECK(s.list->getVerticalScrollOffset() == s.list->getMaxVerticalScrollOffset());
    }

    s.checkOnePass();
}

TEST_CASE("VirtualList variable: the pool is the viewport at the smallest measured height, at any count")
{
    struct Taken
    {
        size_t rows;
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
        const int smallest=smallestMeasured(s.list);

        checkView(s);
        CHECK(s.list->getRows().size() == static_cast<size_t>((200+smallest-1)/smallest+1));
        s.checkOnePass();
        return Taken{s.list->getRows().size(), s.skin.liveControls, after.livePrimitives()-before.livePrimitives(),
            after.liveTexts()-before.liveTexts(), s.bindCount};
    };

    const Taken small=take(1000);
    const Taken large=take(100000);

    CHECK(small.rows == large.rows);
    CHECK(small.controls == large.controls);
    CHECK(small.primitives == large.primitives);
    CHECK(small.texts == large.texts);
    CHECK(small.binds == large.binds);
}

TEST_CASE("VirtualList variable: an item not measured counts at the default height")
{
    Scene s(100, 200, false);
    const controls::HeightIndex &index=s.list->getHeightIndex();

    // nothing measured: the row height
    CHECK(s.list->getVirtualContentHeight() == 100*RowHeight);
    CHECK(s.measures == 0);

    s.frame();
    checkView(s);
    // the measurer had the rows' width
    CHECK(s.widths == std::set<int>{s.list->getInnerPanel()->getWidth()});

    size_t measured=0;
    int64_t sum=0;
    size_t last=0;

    for(size_t i=0; i < index.size(); ++i)
    {
        if(index.generation(i) != 0)
        {
            ++measured;
            sum+=itemHeight(i);
            last=i;
        }
    }
    CHECK(measured == s.measures);
    CHECK(index.totalHeight() == sum+static_cast<int64_t>(100-measured)*RowHeight);

    // added now: the last measured height
    s.list->setItemCount(150);
    CHECK(index.height(149) == itemHeight(last));
    CHECK(index.generation(149) == 0);

    // with an estimator, its height for the items added after it
    s.list->setItemEstimator([](size_t index, int) { return index%2?33:7; });
    s.list->setItemCount(160);
    CHECK(index.height(150) == 7);
    CHECK(index.height(151) == 33);
    CHECK(index.height(149) == itemHeight(last));
    CHECK(s.list->getVirtualContentHeight() == static_cast<int>(index.totalHeight()));

    // a truncation drops their heights
    s.list->setItemCount(120);
    CHECK(index.size() == 120);
    s.frame();
    checkView(s);
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: the bar shows while everything fits, and fixed mode comes back")
{
    Scene s(3, 200);

    checkView(s);
    REQUIRE(s.bar() != nullptr);
    CHECK(!s.bar()->hidden());
    CHECK(s.bar()->isDisabled());

    s.list->setItemCount(1000);
    s.frame();
    CHECK(!s.bar()->isDisabled());
    s.list->setVerticalScrollOffset(400);
    s.frame();
    checkView(s);

    // fixed mode: no index, every row at index*rowHeight from the item the
    // variable list showed at the top
    const size_t top=s.list->firstVisibleItem();

    s.list->setItemMeasurer(nullptr);
    s.frame();
    CHECK(!s.list->isVariableHeight());
    CHECK(s.list->getHeightIndex().empty());
    CHECK(s.list->getVirtualContentHeight() == 1000*RowHeight);
    CHECK(s.list->firstVisibleItem() == top);

    for(controls::Base *row : s.shownRows())
    {
        const size_t item=s.list->itemForRow(row);

        CHECK(row->getY() == static_cast<int>(item*RowHeight)-s.list->getVerticalScrollOffset());
        CHECK(row->getHeight() == RowHeight);
    }

    s.list->setItemCount(3);
    s.frame();
    CHECK(s.bar()->hidden());
    s.checkOnePass();
}
