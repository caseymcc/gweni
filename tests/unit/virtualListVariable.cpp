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
 *  scrollToItem below, above and inside the view (judged from the measures,
 *  not the estimates, of the items down to it), and at the end (the last
 *  item's bottom at the viewport's). A bar moved by the user before an edit
 *  (an insert, a removal or a new height above, the idle re-measure, an
 *  append) is the scroll the edit keeps; two scrollByRows before a layout
 *  move two items; refresh() measures every item again with the top kept;
 *  and leaving variable mode keeps the item at the top, no more than a row
 *  into it, a scroll not laid out yet included. The pool is ceil(viewport/smallest
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
#include <gweni/anim.h>
#include <gweni/controls/text.h>
#include <gweni/controls/virtualList.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <climits>
#include <cmath>
#include <map>
#include <random>
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

//! What a narrower list adds to every item, so a width change changes the
//! heights.
int widthBonus(int width)
{
    return width < 150?7:0;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::VirtualList *list=nullptr;

    //! The items, as the app holds them: each one's height and an id that
    //! moves with it through inserts and removals.
    std::vector<int> heights;
    std::vector<size_t> ids;
    size_t nextId=0;
    //! The id each row was last bound to.
    std::map<controls::Base *, size_t> boundTo;
    size_t bindCount=0;
    //! the widths the measurer was given
    std::set<int> widths;
    size_t measures=0;

    //! A list of count items at the given viewport height; frame() lays it out
    //! unless built is false.
    explicit Scene(size_t count, int height=200, bool layOut=true, bool variable=true)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 600);

        list=canvas->newChild<controls::VirtualList>();
        list->setPos(0, 0);
        list->setSize(200, height);
        list->setRowHeight(RowHeight);
        list->setRowBinder([this](controls::Base *row, size_t index)
            {
                boundTo[row]=ids.at(index);
                ++bindCount;
            });
        if(variable)
            list->setItemMeasurer([this](size_t index, int width)
                {
                    widths.insert(width);
                    ++measures;
                    return heights.at(index)+widthBonus(width);
                });
        // the cases below that pin offsets turn it on themselves
        list->setRemeasureBudget(0);
        insert(0, count);
        if(layOut)
            frame();
    }

    //! Items added and removed, in the app's data and in the list.
    void insert(size_t at, size_t count)
    {
        for(size_t i=0; i < count; ++i)
        {
            heights.insert(heights.begin()+at+i, itemHeight(nextId));
            ids.insert(ids.begin()+at+i, nextId);
            ++nextId;
        }
        if(at == list->getItemCount())
            list->setItemCount(heights.size());
        else
            list->insertItems(at, count);
    }

    void remove(size_t at, size_t count)
    {
        heights.erase(heights.begin()+at, heights.begin()+at+count);
        ids.erase(ids.begin()+at, ids.begin()+at+count);
        list->removeItems(at, count);
    }

    void change(size_t index, int height)
    {
        heights.at(index)=height;
        list->itemChanged(index);
    }

    //! The height the measurer gives the item at the list's width now.
    int measured(size_t index) const
    {
        return heights.at(index)+widthBonus(list->getMeasureWidth());
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
        CHECK(s.boundTo[row] == s.ids[item]);
        CHECK(index.height(item) == s.measured(item));
        CHECK(index.generation(item) == list->getMeasureGeneration());
        CHECK(row->getY() == static_cast<int>(index.offsetOf(item)-offset));
        CHECK(row->getHeight() == s.measured(item));
        CHECK(row->getWidth() == list->getInnerPanel()->getWidth());
        expected.insert(row);
    }

    CHECK(s.shownRows().size() == expected.size());
    for(controls::Base *row : s.shownRows())
        CHECK(expected.count(row) == 1);
}

//! A stuck list's last item ends at the viewport's bottom, or, when they all
//! fit, the first starts at its top.
void checkAtEnd(Scene &s)
{
    const controls::HeightIndex &index=s.list->getHeightIndex();
    const int viewport=s.list->getViewportHeight();
    const int offset=s.list->getVerticalScrollOffset();

    checkView(s);
    if(index.totalHeight() <= viewport)
    {
        CHECK(offset == 0);
        return;
    }
    CHECK(index.totalHeight()-offset == viewport);

    controls::Base *last=s.list->rowForItem(s.list->getItemCount()-1);

    REQUIRE(last != nullptr);
    CHECK(last->bottom() == viewport);
}

//! The shown rows by the id of the item they show: the row, its place and
//! the bind it shows.
struct Shown
{
    controls::Base *row;
    int y;
    int height;

    bool operator==(const Shown &other) const
    {
        return row == other.row && y == other.y && height == other.height;
    }
};

std::map<size_t, Shown> shownById(Scene &s)
{
    std::map<size_t, Shown> shown;

    for(controls::Base *row : s.shownRows())
    {
        const size_t item=s.list->itemForRow(row);

        REQUIRE(item < s.ids.size());
        shown[s.ids[item]]=Shown{row, row->getY(), row->getHeight()};
    }
    return shown;
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
    s.insert(100, 50);
    CHECK(index.height(149) == itemHeight(last));
    CHECK(index.generation(149) == 0);

    // with an estimator, its height for the items added after it
    s.list->setItemEstimator([](size_t index, int) { return index%2?33:7; });
    s.insert(150, 10);
    CHECK(index.height(150) == 7);
    CHECK(index.height(151) == 33);
    CHECK(index.height(149) == itemHeight(last));
    CHECK(s.list->getVirtualContentHeight() == static_cast<int>(index.totalHeight()));

    // a truncation drops their heights
    s.remove(120, 40);
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

    s.insert(3, 997);
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

    s.remove(3, 997);
    s.frame();
    CHECK(s.bar()->hidden());
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: an edit above the top moves nothing on screen")
{
    Scene s(1000);
    const controls::HeightIndex &index=s.list->getHeightIndex();

    s.list->setVerticalScrollOffset(static_cast<int>(index.offsetOf(300))+7);
    s.frame();
    checkView(s);

    const std::map<size_t, Shown> before=shownById(s);
    const size_t binds=s.bindCount;
    const size_t anchorId=s.ids[s.list->getAnchorItem()];
    const int anchorOffset=s.list->getAnchorOffset();
    const int offset=s.list->getVerticalScrollOffset();

    REQUIRE(anchorOffset == 7);
    int64_t moved=0;

    SUBCASE("an insert")
    {
        s.insert(100, 25);
        moved=index.offsetOf(125)-index.offsetOf(100);
    }

    SUBCASE("an insert right before the item at the top")
    {
        const size_t at=s.list->getAnchorItem();

        s.insert(at, 3);
        moved=index.offsetOf(at+3)-index.offsetOf(at);
    }

    SUBCASE("a removal")
    {
        moved=-(index.offsetOf(150)-index.offsetOf(100));
        s.remove(100, 50);
    }

    SUBCASE("a removal of the first items, as a log's cap")
    {
        moved=-index.offsetOf(40);
        s.remove(0, 40);
    }

    SUBCASE("a new height, of an estimated item and of a measured one")
    {
        const int estimated=index.height(250);

        s.change(250, 44);
        moved=44-estimated;
        CHECK(index.height(250) == 44);
        s.frame();
        s.change(250, 12);
        moved+=12-44;
    }

    SUBCASE("several in one frame")
    {
        s.insert(5, 7);
        moved=index.offsetOf(12)-index.offsetOf(5);
        moved-=index.offsetOf(25)-index.offsetOf(20);
        s.remove(20, 5);
        const int estimated=index.height(30);
        s.change(30, 49);
        moved+=49-estimated;
    }

    s.frame();
    checkView(s);
    CHECK(shownById(s) == before);
    CHECK(s.bindCount == binds);
    CHECK(s.ids[s.list->getAnchorItem()] == anchorId);
    CHECK(s.list->getAnchorOffset() == anchorOffset);
    CHECK(s.list->getVerticalScrollOffset() == offset+moved);
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: an edit in view moves only the rows below it and binds only new items")
{
    Scene s(1000);
    const controls::HeightIndex &index=s.list->getHeightIndex();

    s.list->setVerticalScrollOffset(static_cast<int>(index.offsetOf(300)));
    s.frame();

    const std::map<size_t, Shown> before=shownById(s);
    const size_t binds=s.bindCount;

    SUBCASE("an insert")
    {
        s.insert(302, 2);
    }

    SUBCASE("a removal")
    {
        s.remove(301, 2);
    }

    SUBCASE("a new height")
    {
        s.change(301, 50);
    }

    s.frame();
    checkView(s);

    const std::map<size_t, Shown> after=shownById(s);
    size_t fresh=0;

    for(const auto &[id, shown] : after)
    {
        auto it=before.find(id);

        if(it == before.end())
        {
            ++fresh;
            continue;
        }
        // the same row, bound once, at its new place
        CHECK(it->second.row == shown.row);
        CHECK(s.boundTo[shown.row] == id);
    }
    // the items above the edit did not move
    CHECK(after.at(s.ids[300]) == before.at(s.ids[300]));
    // only the rows given new items were bound, and the changed item's
    CHECK(s.bindCount-binds <= fresh+1);
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: removing the item at the top puts the next one there")
{
    Scene s(1000);
    const controls::HeightIndex &index=s.list->getHeightIndex();

    s.list->setVerticalScrollOffset(static_cast<int>(index.offsetOf(300))+5);
    s.frame();

    const size_t next=s.ids[303];

    s.remove(298, 5);
    s.frame();
    checkView(s);
    CHECK(s.ids[s.list->getAnchorItem()] == next);
    CHECK(s.list->getAnchorOffset() == 0);
    CHECK(s.list->rowForItem(s.list->getAnchorItem())->getY() == 0);
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: a list stuck to its end keeps its last item at the bottom")
{
    Scene s(500);

    s.list->setStickToEnd(true);
    CHECK(s.list->isStuckToEnd());
    s.frame();
    checkAtEnd(s);

    // each in the frame it is made in
    s.insert(500, 7);
    s.frame();
    checkAtEnd(s);
    s.remove(0, 50);
    s.frame();
    checkAtEnd(s);
    s.change(s.heights.size()-1, 50);
    s.frame();
    checkAtEnd(s);
    s.change(s.heights.size()-2, 11);
    s.frame();
    checkAtEnd(s);
    s.insert(200, 4);
    s.remove(s.heights.size()-3, 3);
    s.frame();
    checkAtEnd(s);

    // a scroll does not unstick it
    s.list->setVerticalScrollOffset(0);
    s.frame();
    checkAtEnd(s);
    s.list->scrollToItem(10);
    s.frame();
    checkAtEnd(s);

    // the app does: from then on it stays where it is
    s.list->setStickToEnd(false);
    s.frame();

    const std::map<size_t, Shown> before=shownById(s);
    const int offset=s.list->getVerticalScrollOffset();

    s.insert(s.heights.size(), 10);
    s.frame();
    checkView(s);
    CHECK(shownById(s) == before);
    CHECK(s.list->getVerticalScrollOffset() == offset);

    // stuck again, at the new end
    s.list->setStickToEnd(true);
    s.frame();
    checkAtEnd(s);

    // emptied and filled again
    s.remove(0, s.heights.size());
    s.frame();
    checkView(s);
    s.insert(0, 3);
    s.frame();
    checkAtEnd(s);
    s.insert(3, 30);
    s.frame();
    checkAtEnd(s);
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: random edits, scrolls and sticks keep the view valid")
{
    Scene s(300, 150);
    std::mt19937 rng(7);

    // the idle re-measure on, and now and then a new width
    s.list->setRemeasureBudget(16);

    for(int step=0; step < 400; ++step)
    {
        CAPTURE(step);

        const size_t count=s.heights.size();
        const int op=rng()%8;

        if(op == 0 || count == 0)
            s.insert(rng()%(count+1), 1+rng()%20);
        else if(op == 1)
        {
            const size_t at=rng()%count;

            s.remove(at, std::min<size_t>(1+rng()%20, count-at));
        }
        else if(op == 2)
            s.change(rng()%count, 10+rng()%41);
        else if(op == 3)
            s.list->setVerticalScrollOffset(static_cast<int>(rng()%(s.list->getMaxVerticalScrollOffset()+1)));
        else if(op == 4)
            s.list->scrollByRows(static_cast<int>(rng()%11)-5);
        else if(op == 5)
            s.list->scrollToItem(rng()%count);
        else if(op == 6)
        {
            s.list->setStickToEnd(rng()%3 == 0);
            if(rng()%2)
                s.list->setWidth(120+static_cast<int>(rng()%81));
        }
        else
            s.list->onMouseWheeled((rng()%2)?60:-60);

        // now and then two edits in one frame
        if(rng()%3)
        {
            s.frame();
            if(s.list->isStuckToEnd())
                checkAtEnd(s);
            else
                checkView(s);
        }
    }
    s.frame();
    checkView(s);
    s.checkOnePass();
}

TEST_CASE("VirtualList fixed: inserts and removals above the top keep the rows on screen")
{
    Scene s(1000, 200, true, false);

    REQUIRE(!s.list->isVariableHeight());
    s.list->setVerticalScrollOffset(300*RowHeight+7);
    s.frame();

    const std::map<size_t, Shown> before=shownById(s);
    const size_t binds=s.bindCount;

    s.insert(100, 25);
    s.frame();
    CHECK(s.list->getVerticalScrollOffset() == 325*RowHeight+7);
    CHECK(shownById(s) == before);
    s.remove(10, 40);
    s.frame();
    CHECK(s.list->getVerticalScrollOffset() == 285*RowHeight+7);
    CHECK(shownById(s) == before);
    CHECK(s.bindCount == binds);

    for(controls::Base *row : s.shownRows())
    {
        const size_t item=s.list->itemForRow(row);

        CHECK(s.boundTo[row] == s.ids[item]);
        CHECK(row->getY() == static_cast<int>(item*RowHeight)-s.list->getVerticalScrollOffset());
    }

    // the item at the top removed: the one after the removed ones there
    const size_t next=s.ids[290];

    s.remove(283, 7);
    s.frame();
    CHECK(s.list->getVerticalScrollOffset() == 283*RowHeight);
    CHECK(s.ids[s.list->firstVisibleItem()] == next);

    // stuck to the end
    s.list->setStickToEnd(true);
    s.frame();
    CHECK(s.list->getVerticalScrollOffset() == s.list->getMaxVerticalScrollOffset());
    s.insert(s.heights.size(), 5);
    s.frame();
    CHECK(s.list->getVerticalScrollOffset() == s.list->getMaxVerticalScrollOffset());
    CHECK(s.list->rowForItem(s.heights.size()-1)->bottom() == s.list->getViewportHeight());
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: a new width measures the rows on screen at once and keeps the top in place")
{
    Scene s(1000);
    const controls::HeightIndex &index=s.list->getHeightIndex();

    s.list->setVerticalScrollOffset(static_cast<int>(index.offsetOf(300))+5);
    s.frame();

    const uint32_t generation=s.list->getMeasureGeneration();
    const size_t anchorId=s.ids[s.list->getAnchorItem()];

    REQUIRE(s.list->rowForItem(s.list->getAnchorItem())->getY() == -5);

    // narrower: the bonus is added to every item measured at it
    s.list->setWidth(140);
    s.frame();
    REQUIRE(widthBonus(s.list->getMeasureWidth()) > 0);
    CHECK(s.list->getMeasureGeneration() == generation+1);
    CHECK(s.list->getMeasureWidth() == s.list->getInnerPanel()->getWidth());
    // every row on screen measured at the new width (checkView), the top kept
    checkView(s);
    CHECK(s.ids[s.list->getAnchorItem()] == anchorId);
    CHECK(s.list->rowForItem(s.list->getAnchorItem())->getY() == -5);
    // the items off screen were not measured: one counter bumped, no walk
    CHECK(index.generation(0) < s.list->getMeasureGeneration());
    CHECK(index.minGeneration() < s.list->getMeasureGeneration());

    // and back
    s.list->setWidth(200);
    s.frame();
    checkView(s);
    CHECK(s.list->getMeasureGeneration() == generation+2);
    CHECK(s.list->rowForItem(s.list->getAnchorItem())->getY() == -5);
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: the idle re-measure measures a budget a frame, moves nothing on screen and stops")
{
    const bool stuck=GENERATE(false, true);
    Scene s(1000);
    const controls::HeightIndex &index=s.list->getHeightIndex();

    CAPTURE(stuck);
    s.list->setStickToEnd(stuck);
    if(!stuck)
        s.list->setVerticalScrollOffset(static_cast<int>(index.offsetOf(300))+5);
    s.frame();
    CHECK(!s.list->isRemeasuring());

    // all but the items on screen have never been measured
    auto runToEnd=[&](size_t budget)
    {
        const std::map<size_t, Shown> shown=shownById(s);
        size_t stale=0;

        for(size_t i=0; i < index.size(); ++i)
            stale+=index.generation(i) != s.list->getMeasureGeneration();
        REQUIRE(stale > 0);

        s.list->setRemeasureBudget(budget);
        CHECK(s.list->isRemeasuring());

        size_t frames=0;

        while(s.list->isRemeasuring())
        {
            const size_t before=s.measures;

            s.frame();
            ++frames;
            REQUIRE(frames < 100);
            CHECK(s.measures-before <= budget);
            // nothing on screen moves, the offset and the thumb do
            CHECK(shownById(s) == shown);
            if(stuck)
                checkAtEnd(s);
            else
                checkView(s);
        }
        CHECK(frames == (stale+budget-1)/budget);

        // every item measured at this width: the content is their sum
        CHECK(index.minGeneration() == s.list->getMeasureGeneration());

        int64_t sum=0;

        for(size_t i=0; i < index.size(); ++i)
        {
            CHECK(index.height(i) == s.measured(i));
            sum+=s.measured(i);
        }
        CHECK(index.totalHeight() == sum);
        CHECK(s.list->getVirtualContentHeight() == sum);

        // then it stops: a settled frame runs, measures and lays out nothing
        const size_t before=s.measures;

        s.frame();
        CHECK(s.measures == before);
        CHECK(anim::activeCount(s.list) == 0);
        CHECK(s.canvas->getLayoutStats().passes == 0);
        CHECK(s.canvas->getUpdateStats().animsRun == 0);
    };

    runToEnd(64);

    // a new width: the rows on screen at once, the rest by the re-measure
    s.list->setWidth(140);
    s.frame();
    REQUIRE(widthBonus(s.list->getMeasureWidth()) > 0);
    runToEnd(100);
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: with no budget only what shows is measured")
{
    Scene s(1000);

    s.frame();
    s.frame();
    CHECK(!s.list->isRemeasuring());
    CHECK(s.list->getHeightIndex().minGeneration() == 0);

    // and given one, it starts
    s.list->setRemeasureBudget(64);
    CHECK(s.list->isRemeasuring());
    s.list->setRemeasureBudget(0);
    CHECK(!s.list->isRemeasuring());
}

TEST_CASE("VirtualList variable: layout verification reports a row its measurer disagrees with")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);
    std::map<controls::Base *, controls::internal::Text *> texts;
    std::vector<String> strings;

    for(size_t i=0; i < 200; ++i)
        strings.push_back(std::string(5+(i*13)%90, 'x')+" some words to wrap here "+std::to_string(i));

    canvas->setSize(400, 600);

    auto *list=canvas->newChild<controls::VirtualList>();

    list->setBounds(0, 0, 200, 300);
    list->setRemeasureBudget(0);
    list->setRowFactory([&texts](controls::Base *parent)
        {
            controls::Base *row=parent->newChild<controls::Base>();
            auto *text=row->newChild<controls::internal::Text>();

            text->setWrap(true);
            texts[row]=text;
            return row;
        });
    list->setRowBinder([&](controls::Base *row, size_t index) { texts.at(row)->setText(strings.at(index)); });

    int lie=0;

    list->setItemMeasurer([&](size_t index, int width)
        {
            return list->wrappedTextHeight(skin.getDefaultFont(), strings.at(index), width)+lie;
        });
    list->setItemCount(strings.size());

    const bool verify=controls::layoutVerifyEnabled();

    controls::setLayoutVerify(true);

    const size_t failures=controls::layoutVerifyFailures();

    // the measurer is the wrap the rows' Texts lay out at
    for(int i=0; i < 3; ++i)
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
    list->setVerticalScrollOffset(777);
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(controls::layoutVerifyFailures() == failures);

    for(controls::Base *row : list->getRows())
    {
        if(!row->hidden())
            CHECK(row->getPreferredSize().height == list->getHeightIndex().height(list->itemForRow(row)));
    }

    // one that is not: every shown row is reported
    lie=3;
    list->setItemMeasurer([&](size_t index, int width)
        {
            return list->wrappedTextHeight(skin.getDefaultFont(), strings.at(index), width)+lie;
        });
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(controls::layoutVerifyFailures() > failures);

    controls::setLayoutVerify(verify);
    delete canvas;
}

TEST_CASE("VirtualList variable: scrollToItem judges the view from measures, not estimates")
{
    // laid out first by the frame that resolves the scroll: every item counts
    // at the row height until then, below most of the measured heights
    Scene s(1000, 200, false);
    const int viewport=200;

    int64_t top=0;
    size_t item=0;

    // the first item whose measured bottom is past the viewport while its
    // estimated one is not
    for(; item < 1000; ++item)
    {
        // measured before any layout gave a width: the default width's
        if(top+s.heights.at(item) > viewport && static_cast<int64_t>(item+1)*RowHeight <= viewport)
            break;
        top+=s.heights.at(item);
    }
    REQUIRE(item < 1000);

    s.list->scrollToItem(item);
    s.frame();
    checkView(s);

    controls::Base *row=s.list->rowForItem(item);

    REQUIRE(row != nullptr);
    CHECK(row->getY() >= 0);
    CHECK(row->bottom() == s.list->getViewportHeight());

    SUBCASE("from a scrolled anchor, further down")
    {
        const controls::HeightIndex &index=s.list->getHeightIndex();
        const size_t anchor=s.list->getAnchorItem();
        size_t target=anchor;
        int64_t covered=0;

        // the first item not measured yet, once the view is full
        while(covered < viewport || index.generation(target) == s.list->getMeasureGeneration())
            covered+=s.measured(target++);

        s.list->scrollToItem(target);
        s.frame();
        checkView(s);
        REQUIRE(s.list->rowForItem(target) != nullptr);
        CHECK(s.list->rowForItem(target)->bottom() == s.list->getViewportHeight());
    }
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: a bar moved before an edit is the scroll the edit keeps")
{
    Scene s(1000);
    const controls::HeightIndex &index=s.list->getHeightIndex();
    const int range=s.list->getMaxVerticalScrollOffset();

    REQUIRE(range > 1000);
    s.bar()->setScrolledAmount(0.5f, true);

    // the item the user scrolled to, before anything moved it
    const int scrolled=static_cast<int>(std::lround(0.5*range));
    int64_t top=0;
    const size_t at=index.itemAt(scrolled, &top);
    const size_t id=s.ids.at(at);

    SUBCASE("an insert above") { s.insert(0, 1); }
    SUBCASE("a removal above") { s.remove(0, 1); }
    SUBCASE("a new height above") { s.change(0, 49); }
    SUBCASE("the idle re-measure") { s.list->setRemeasureBudget(64); }
    SUBCASE("setItemCount") { s.insert(s.heights.size(), 10); }

    s.frame();
    checkView(s);
    CHECK(s.ids.at(s.list->getAnchorItem()) == id);
    CHECK(s.list->getVerticalScrollOffset() > range/4);
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: leaving variable mode keeps the item at the top")
{
    Scene s(1000);
    const controls::HeightIndex &index=s.list->getHeightIndex();
    size_t tall=100;

    // an item into which the anchor can be more than a row height
    while(itemHeight(tall) < RowHeight+10)
        ++tall;

    const int into=itemHeight(tall)-2;

    // measured, so the offsets below are its
    s.list->scrollToItem(tall);
    s.frame();

    SUBCASE("after a layout")
    {
        s.list->setVerticalScrollOffset(static_cast<int>(index.offsetOf(tall))+into);
        s.frame();
        REQUIRE(s.list->getAnchorItem() == tall);
        REQUIRE(s.list->getAnchorOffset() == into);
    }

    SUBCASE("with a scroll not laid out yet")
    {
        s.list->setVerticalScrollOffset(static_cast<int>(index.offsetOf(tall))+into);
    }

    s.list->setItemMeasurer(nullptr);
    s.frame();
    CHECK(s.list->firstVisibleItem() == tall);
    CHECK(s.list->getVerticalScrollOffset() == static_cast<int>(tall)*RowHeight+RowHeight-1);
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: refresh measures every item again")
{
    Scene s(1000);
    const controls::HeightIndex &index=s.list->getHeightIndex();

    s.list->setVerticalScrollOffset(static_cast<int>(index.offsetOf(300))+5);
    s.list->setRemeasureBudget(1000);
    s.frame();
    s.frame();
    REQUIRE(!s.list->isRemeasuring());
    REQUIRE(index.minGeneration() == s.list->getMeasureGeneration());

    const uint32_t generation=s.list->getMeasureGeneration();
    const size_t anchorId=s.ids[s.list->getAnchorItem()];

    // the app's items all change, with no itemChanged()
    for(int &height : s.heights)
        height=height%2?height+3:height;
    s.list->refresh();
    s.frame();
    CHECK(s.list->getMeasureGeneration() == generation+1);
    // the rows on screen at their new heights (checkView), the top kept
    checkView(s);
    CHECK(s.ids[s.list->getAnchorItem()] == anchorId);
    CHECK(s.list->rowForItem(s.list->getAnchorItem())->getY() == -5);

    // and the rest by the idle re-measure
    while(s.list->isRemeasuring())
        s.frame();
    for(size_t i=0; i < index.size(); ++i)
        CHECK(index.height(i) == s.measured(i));
    s.checkOnePass();
}

TEST_CASE("VirtualList variable: two scrollByRows before a layout move two items")
{
    Scene s(1000);
    const controls::HeightIndex &index=s.list->getHeightIndex();
    size_t tall=100;

    // an item followed by one shorter than the offset into it
    while(itemHeight(tall+1) >= itemHeight(tall)-2)
        ++tall;

    s.list->scrollToItem(tall+1);
    s.frame();
    s.list->setVerticalScrollOffset(static_cast<int>(index.offsetOf(tall))+itemHeight(tall)-2);
    s.frame();
    REQUIRE(s.list->getAnchorItem() == tall);

    // the offset shows the new item until the next layout
    s.list->scrollByRows(1);
    CHECK(s.list->firstVisibleItem() == tall+1);
    s.list->scrollByRows(1);
    CHECK(s.list->firstVisibleItem() == tall+2);
    s.frame();
    CHECK(s.list->getAnchorItem() == tall+2);
    checkView(s);
    s.checkOnePass();
}
