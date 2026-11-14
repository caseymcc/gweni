/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::VirtualListReorder on a real VirtualList of 1 000 items, 20 units
 *  a row, 200 high (10 rows on screen), each item keyed by the app's own
 *  vector of keys (key(index), indexOf(key), counting their calls), driven
 *  through the input handler with the clock stepped 1/60 s a frame.
 *
 *  A press on a pooled row moved 5 units drags the item it shows, keyed by
 *  the app's key, taken at the press: items inserted above it between the
 *  press and the move leave it the one dragged, and the pressed item removed
 *  before the move starts no drag. The gap is the half-height rule over index*rowHeight less
 *  the offset, its bar at that boundary, wholly inside the viewport where the
 *  boundary is its top or bottom edge. Moves across gaps and scroll frames
 *  call neither key function. The veil is over the row that shows the dragged
 *  item, follows it as the pool's rows are bound to other items, hides while
 *  it is out of view and comes back with it. The cursor resting in the bottom
 *  band auto-scrolls the list, binding rows, the drag keeping its item, and
 *  an auto-scroll frame costs a plain scroll of the same distance plus the
 *  looks' two controls, and no row looks hovered (the drag took the hover from
 *  the pressed row, which shows other items as it scrolls). A drop reports the item's key and indexes. Inserts and
 *  removals during the drag keep the gap before its item, a refresh() after
 *  the app reordered its items looks the dragged one up again, and the item
 *  removed cancels the drag. An insert or removal that moves the gap asks
 *  canDrop again: onto a refused index the bar goes, the cursor is No and a
 *  release reports nothing; off one it comes back and the drop is reported.
 *  Wheel input at a resting cursor during the drag scrolls and rebinds rows
 *  without hover or key lookups; the drag keeps its key after its row is
 *  recycled, and the drop uses the gap under the cursor after scrolling.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/anim.h>
#include <gweni/controls/reorderable.h>
#include <gweni/controls/virtualList.h>
#include <gweni/dragAndDrop.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace gweni;
using controls::ItemDrop;
using controls::Reorderable;
using controls::VirtualList;
using controls::VirtualListReorder;

namespace
{

constexpr int RowHeight=20;
constexpr size_t Items=1000;

std::string str(const gweni::Rect &r)
{
    return std::to_string(r.x)+","+std::to_string(r.y)+" "+std::to_string(r.w)+"x"+std::to_string(r.h);
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    VirtualList *list=nullptr;
    VirtualListReorder *reorder=nullptr;
    float clock=10.0f;
    std::vector<uint64_t> keys;
    int keyCalls=0;
    int indexCalls=0;
    std::vector<ItemDrop> drops;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        for(size_t i=0; i < Items; ++i)
            keys.push_back(5000+i);

        list=canvas->newChild<VirtualList>();
        list->setPos(0, 0);
        list->setSize(200, 200);
        list->setRowHeight(RowHeight);
        list->setItemCount(keys.size());
        reorder=VirtualListReorder::attach(list,
            [this](size_t index) { ++keyCalls; return keys[index]; },
            [this](uint64_t key) {
                ++indexCalls;
                auto found=std::find(keys.begin(), keys.end(), key);
                return found == keys.end() ? VirtualList::NoItem : static_cast<size_t>(found-keys.begin());
            });
        reorder->setOnItemDropped([this](const ItemDrop &drop) { drops.push_back(drop); });
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        draganddrop::cancel();
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void step()
    {
        clock+=1.0f/60.0f;
        frame();
    }

    void move(Point to)
    {
        canvas->inputMouseMoved(to.x, to.y, 0, 0);
        frame();
    }

    void scrollTo(int offset)
    {
        list->setVerticalScrollOffset(offset);
        frame();
    }

    //! A canvas point at the viewport's y \p y, in the middle across
    Point at(int y) const
    {
        const Rect &viewport=list->getViewport();
        return list->localPosToCanvas(Point(viewport.x+viewport.w/2, viewport.y+y));
    }

    //! The viewport y of item \p index's top
    int top(size_t index) const
    {
        return static_cast<int>(index)*RowHeight-list->getVerticalScrollOffset();
    }

    //! Presses item \p index a quarter down and moves 6 units right
    void startDrag(size_t index)
    {
        clock+=1.0f;
        const Point p=at(top(index)+RowHeight/4);
        move(p);
        input::onMouseClicked(canvas, 0, true);
        frame();
        move(Point(p.x+6, p.y));
    }

    void release()
    {
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    //! A frame for what was done since the last one, and its counts (see
    //! cardReorder.cpp)
    controls::CanvasFrameStats inputFrame()
    {
        frame();
        frame();
        return canvas->previousFrameStats();
    }
};

}//namespace

TEST_CASE("VirtualListReorder: a press on a row drags its item, and the gap is index*rowHeight less the offset")
{
    Scene scene;
    scene.scrollTo(100);
    REQUIRE(scene.list->getVerticalScrollOffset() == 100);

    scene.startDrag(7);
    REQUIRE(draganddrop::isDragging());
    CHECK(Reorderable::dragSource() == scene.reorder);
    CHECK(scene.reorder->dragKey() == scene.keys[7]);
    CHECK(scene.reorder->dragIndex() == 7);

    // item 12's upper half: before it; its lower half: after it
    scene.move(scene.at(scene.top(12)+RowHeight/2-1));
    CHECK(scene.reorder->gap() == 12);
    controls::internal::DropIndicator *indicator=scene.reorder->getIndicator();
    REQUIRE(indicator != nullptr);
    CHECK(scene.reorder->indicatorShown());
    CHECK(indicator->getY() == scene.top(12)-Reorderable::IndicatorThickness/2);
    scene.move(scene.at(scene.top(12)+RowHeight/2+1));
    CHECK(scene.reorder->gap() == 13);

    // the veil over the row that shows item 7
    controls::Base *row=scene.list->rowForItem(7);
    REQUIRE(row != nullptr);
    CHECK(str(scene.reorder->getSourceVeil()->getBounds()) == str(row->getBounds()));

    scene.release();
    REQUIRE(scene.drops.size() == 1);
    CHECK(scene.drops[0].key == scene.keys[7]);
    CHECK(scene.drops[0].sourceIndex == 7);
    CHECK(scene.drops[0].targetIndex == 12);
}

TEST_CASE("VirtualListReorder: the item pressed is the one dragged, whatever the list did before the move")
{
    SUBCASE("items inserted above between the press and the move")
    {
        Scene scene;
        const uint64_t key=scene.keys[4];
        const Point p=scene.at(scene.top(4)+RowHeight/4);

        scene.clock+=1.0f;
        scene.move(p);
        input::onMouseClicked(scene.canvas, 0, true);
        scene.frame();
        scene.keys.insert(scene.keys.begin(), {1, 2});
        scene.list->insertItems(0, 2);
        scene.list->setVerticalScrollOffset(0);
        scene.frame();
        REQUIRE(scene.list->getVerticalScrollOffset() == 0);

        scene.move(Point(p.x+6, p.y));
        REQUIRE(draganddrop::isDragging());
        CHECK(scene.reorder->dragKey() == key);
        CHECK(scene.reorder->dragIndex() == 6);
        scene.release();
    }

    SUBCASE("the pressed item removed before the move starts no drag")
    {
        Scene scene;
        const Point p=scene.at(scene.top(4)+RowHeight/4);

        scene.clock+=1.0f;
        scene.move(p);
        input::onMouseClicked(scene.canvas, 0, true);
        scene.frame();
        scene.keys.erase(scene.keys.begin()+4);
        scene.list->removeItems(4, 1);
        scene.frame();

        scene.move(Point(p.x+6, p.y));
        CHECK_FALSE(draganddrop::isDragging());
        scene.release();
        CHECK(scene.drops.empty());
    }
}

TEST_CASE("VirtualListReorder: the bar of a gap at the viewport's edge lies wholly inside it")
{
    SUBCASE("before the first item, unscrolled")
    {
        Scene scene;
        scene.startDrag(3);
        scene.move(scene.at(scene.top(0)+3));
        REQUIRE(scene.reorder->gap() == 0);
        REQUIRE(scene.reorder->indicatorShown());
        const controls::Base *indicator=scene.reorder->getIndicator();
        CHECK(indicator->getY() == 0);
        CHECK(indicator->getHeight() == Reorderable::IndicatorThickness);
        scene.release();
    }

    SUBCASE("after the last item, scrolled to the end")
    {
        Scene scene;
        scene.scrollTo(scene.list->getMaxVerticalScrollOffset());
        const int viewport=scene.list->getViewportHeight();
        REQUIRE(scene.top(Items-1)+RowHeight == viewport);

        scene.startDrag(Items-3);
        scene.move(scene.at(scene.top(Items-1)+RowHeight-3));
        REQUIRE(scene.reorder->gap() == Items);
        REQUIRE(scene.reorder->indicatorShown());
        const controls::Base *indicator=scene.reorder->getIndicator();
        CHECK(indicator->getY()+indicator->getHeight() == viewport);
        scene.release();
    }
}

TEST_CASE("VirtualListReorder: moves across gaps and scroll frames call neither key function")
{
    Scene scene;
    scene.startDrag(3);
    REQUIRE(draganddrop::isDragging());
    const int keys=scene.keyCalls;
    const int indexes=scene.indexCalls;

    for(int y=10; y < 180; y+=7)
        scene.move(scene.at(y));
    scene.scrollTo(400);
    scene.scrollTo(4000);
    scene.move(scene.at(50));
    scene.scrollTo(0);

    CHECK(scene.keyCalls == keys);
    CHECK(scene.indexCalls == indexes);
    CHECK(draganddrop::isDragging());
    scene.release();
}

TEST_CASE("VirtualListReorder: the veil follows the dragged item as rows rebind, hidden while out of view")
{
    Scene scene;
    scene.startDrag(2);
    controls::internal::DragSourceVeil *veil=scene.reorder->getSourceVeil();
    REQUIRE(veil != nullptr);
    REQUIRE(scene.reorder->veilShown());

    // scrolled so far that item 2 is gone and its row shows another item
    controls::Base *row=scene.list->rowForItem(2);
    scene.scrollTo(300);
    CHECK(scene.list->rowForItem(2) == nullptr);
    CHECK(scene.list->itemForRow(row) != 2);
    CHECK_FALSE(scene.reorder->veilShown());

    // part way back: item 2 on screen, on whichever row shows it now
    scene.scrollTo(30);
    controls::Base *now=scene.list->rowForItem(2);
    REQUIRE(now != nullptr);
    CHECK(scene.reorder->veilShown());
    CHECK(str(veil->getBounds()) == str(now->getBounds()));
    scene.release();
    CHECK_FALSE(veil->visible());
}

TEST_CASE("VirtualListReorder: wheel input during a drag rebinds rows and keeps the key without hover")
{
    Scene scene;
    controls::Base *pressed=scene.list->rowForItem(2);
    REQUIRE(pressed != nullptr);
    scene.startDrag(2);
    REQUIRE(draganddrop::isDragging());
    scene.move(scene.at(100));
    const uint64_t key=scene.reorder->dragKey();
    const int keys=scene.keyCalls;
    const int indexes=scene.indexCalls;
    const size_t before=scene.reorder->gap();

    for(int notch=0; notch < 10; ++notch)
    {
        CHECK(scene.canvas->inputMouseWheel(-120));
        const controls::CanvasFrameStats stats=scene.inputFrame();
        CHECK(stats.layout.passes == 1);
        CHECK_FALSE(stats.layout.renderLayout);
    }
    CHECK(scene.list->getVerticalScrollOffset() > 200);
    CHECK(scene.list->rowForItem(2) == nullptr);
    CHECK(scene.list->itemForRow(pressed) != 2);
    CHECK_FALSE(scene.reorder->veilShown());
    CHECK(scene.reorder->gap() > before);
    CHECK(scene.reorder->gap() == static_cast<size_t>(
        (100+scene.list->getVerticalScrollOffset()+RowHeight/2)/RowHeight));
    CHECK(scene.keyCalls == keys);
    CHECK(scene.indexCalls == indexes);
    CHECK(draganddrop::isDragging());
    CHECK(scene.reorder->dragKey() == key);
    CHECK(scene.reorder->dragIndex() == 2);
    CHECK(HoveredControl == nullptr);
    for(controls::Base *row : scene.list->getRows())
        CHECK_FALSE(row->isHovered());

    const size_t gap=scene.reorder->gap();
    scene.release();
    REQUIRE(scene.drops.size() == 1);
    CHECK(scene.drops[0].key == key);
    CHECK(scene.drops[0].sourceIndex == 2);
    CHECK(scene.drops[0].targetIndex == gap-1);
}

TEST_CASE("VirtualListReorder: the cursor in the bottom band auto-scrolls the list as a plain scroll costs")
{
    Scene scene;

    // a plain scroll of 10 units, with no drag, for the cost to compare
    scene.list->setVerticalScrollOffset(10);
    const controls::CanvasFrameStats plain=scene.inputFrame();
    scene.scrollTo(0);

    scene.startDrag(1);
    scene.reorder->setAutoScrollSpeed(600.0f);
    // the band's outer edge: the whole speed, 10 units a frame
    scene.move(scene.at(scene.list->getViewport().h-1));
    REQUIRE(scene.reorder->autoScrollDirection() == 1);
    scene.step();
    scene.step();

    const int before=scene.list->getVerticalScrollOffset();
    scene.clock+=1.0f/60.0f;
    const controls::CanvasFrameStats stepped=scene.inputFrame();
    const int moved=scene.list->getVerticalScrollOffset()-before;
    CHECK(moved >= 9);
    CHECK(moved <= 10);
    CHECK(stepped.render.controls <= plain.render.controls+2);
    CHECK(stepped.layout.visited <= plain.layout.visited+8);
    CHECK(stepped.layout.passes == 1);

    for(int frame=0; frame < 30; ++frame)
        scene.step();
    CHECK(scene.list->getVerticalScrollOffset() > 300);
    // rows were bound to the items scrolled to, and the drag kept its item
    CHECK(scene.list->rowForItem(scene.list->firstVisibleItem()+3) != nullptr);
    CHECK(draganddrop::isDragging());
    CHECK(scene.reorder->dragKey() == scene.keys[1]);
    CHECK_FALSE(scene.reorder->veilShown());
    // the pressed row, bound to other items as it scrolled, looks hovered by
    // none of them: the drag took the hover away
    for(controls::Base *row : scene.list->getRows())
        CHECK_FALSE(row->isHovered());

    // dropped at the gap under the cursor
    const size_t gap=scene.reorder->gap();
    scene.release();
    REQUIRE(scene.drops.size() == 1);
    CHECK(scene.drops[0].sourceIndex == 1);
    CHECK(scene.drops[0].targetIndex == gap-1);
}

TEST_CASE("VirtualListReorder: edits during the drag keep it, and the item removed cancels it")
{
    SUBCASE("inserted above, then a refresh after the app moved the item")
    {
        Scene scene;
        scene.startDrag(4);
        scene.move(scene.at(scene.top(8)+3));
        REQUIRE(scene.reorder->gap() == 8);
        const int keysBefore=scene.keyCalls;

        // three items at the top: the list keeps its rows on screen
        scene.keys.insert(scene.keys.begin(), {1, 2, 3});
        scene.list->insertItems(0, 3);
        scene.frame();
        CHECK(scene.reorder->dragIndex() == 7);
        CHECK(scene.reorder->gap() == 11);
        CHECK(scene.keyCalls == keysBefore);

        // the app moves the dragged item to the end and says so
        const uint64_t key=scene.keys[7];
        scene.keys.erase(scene.keys.begin()+7);
        scene.keys.push_back(key);
        scene.list->refresh();
        scene.frame();
        CHECK(draganddrop::isDragging());
        CHECK(scene.reorder->dragIndex() == scene.keys.size()-1);

        scene.release();
        REQUIRE(scene.drops.size() == 1);
        CHECK(scene.drops[0].key == key);
        CHECK(scene.drops[0].sourceIndex == scene.keys.size()-1);
    }

    SUBCASE("the item removed")
    {
        Scene scene;
        scene.startDrag(4);
        scene.move(scene.at(scene.top(8)+3));
        scene.keys.erase(scene.keys.begin()+2, scene.keys.begin()+6);
        scene.list->removeItems(2, 4);
        scene.frame();
        CHECK_FALSE(draganddrop::isDragging());
        CHECK_FALSE(scene.reorder->getSourceVeil()->visible());
        scene.release();
        CHECK(scene.drops.empty());
    }

    SUBCASE("a new count with the item gone")
    {
        Scene scene;
        scene.startDrag(9);
        scene.keys.resize(5);
        scene.list->setItemCount(5);
        scene.frame();
        CHECK_FALSE(draganddrop::isDragging());
        scene.release();
        CHECK(scene.drops.empty());
    }
}

TEST_CASE("VirtualListReorder: an insert or removal that moves the gap asks canDrop again")
{
    SUBCASE("a removal above moves the gap onto a refused index")
    {
        Scene scene;
        int asked=0;
        // nothing above the pinned first item
        scene.reorder->setCanDrop([&asked](Reorderable *, uint64_t, Reorderable *, size_t index) {
            ++asked;
            return index > 0;
        });
        scene.startDrag(3);
        scene.move(scene.at(scene.top(1)+3));
        REQUIRE(scene.reorder->gap() == 1);
        REQUIRE(scene.reorder->showsGap());
        const int askedBefore=asked;

        scene.keys.erase(scene.keys.begin());
        scene.list->removeItems(0, 1);
        scene.frame();
        CHECK(scene.reorder->gap() == 0);
        CHECK(asked > askedBefore);
        CHECK_FALSE(scene.reorder->showsGap());
        CHECK_FALSE(scene.reorder->indicatorShown());
        CHECK(draganddrop::currentCursor() == cursor::No);
        scene.release();
        CHECK(scene.drops.empty());
    }

    SUBCASE("an insert above moves the gap off a refused index")
    {
        Scene scene;
        scene.reorder->setCanDrop([](Reorderable *, uint64_t, Reorderable *, size_t index) { return index > 0; });
        scene.startDrag(3);
        scene.move(scene.at(scene.top(0)+3));
        REQUIRE(scene.reorder->gap() == 0);
        REQUIRE_FALSE(scene.reorder->showsGap());

        scene.keys.insert(scene.keys.begin(), 1);
        scene.list->insertItems(0, 1);
        scene.frame();
        CHECK(scene.reorder->gap() == 1);
        CHECK(scene.reorder->showsGap());
        CHECK(draganddrop::currentCursor() == cursor::Normal);
        scene.release();
        REQUIRE(scene.drops.size() == 1);
        CHECK(scene.drops[0].targetIndex == 1);
    }
}
