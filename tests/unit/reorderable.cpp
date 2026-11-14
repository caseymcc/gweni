/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Reorderable and ReorderGroup on a real Canvas, through the drag
 *  core and the input handler, with a list of 20-unit items over a vector of
 *  keys (the list itself is pressed: it hands out the item under the press).
 *
 *  The gap is the one nearer the cursor by the half-height rule, and a move
 *  that keeps it calls nothing: no key(), no indexOf(), no canDrop, no
 *  gapChanged. The target index is the item's index once it has left the
 *  source (SortableJS's newIndex); a drop within one list reports the key, the
 *  source index and that index, and a drop where the item was reports
 *  nothing. The accept rules: a list takes its own items while reorderWithin
 *  (and reorderWithinNow) allows, another member's of its ReorderGroup while
 *  acceptsFromGroup; a list that refuses is no target (cursor::No) and a
 *  release over it cancels. canDrop is asked once per gap change; a refused
 *  gap shows nothing, the cursor is No and a release there cancels.
 *  Inserts and removals told during a drag keep the gap before the item it
 *  was before and the drop reports the indexes as they are then; an edit the
 *  list cannot describe looks the dragged item up again; the dragged item
 *  removed (told either way) cancels with nothing reported. The drop handler's
 *  own removal of the item from the source is a plain edit, not a cancel.
 *  ReorderGroup and Reorderable may be destroyed in either order, and a
 *  Reorderable deleted as the drag's source ends it.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/reorderable.h>
#include <gweni/dragAndDrop.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <vector>

using namespace gweni;
using controls::Reorderable;
using controls::ReorderGroup;
using controls::ItemDrop;

namespace
{

constexpr int ItemHeight=20;

//! The adapter of a KeyList: its items are `keys`, each ItemHeight high from
//! the list's top. Counts the app calls a drag makes.
class KeyReorder: public Reorderable
{
public:
    using Reorderable::Reorderable;

    std::vector<uint64_t> keys;
    mutable int keyCalls=0;
    mutable int indexCalls=0;
    int gapChanges=0;
    int dragChanges=0;
    bool withinNow=true;

    size_t itemCount() const override { return keys.size(); }
    uint64_t keyAt(size_t index) const override
    {
        ++keyCalls;
        return keys[index];
    }
    size_t indexOf(uint64_t key) const override
    {
        ++indexCalls;
        auto found=std::find(keys.begin(), keys.end(), key);
        return (found == keys.end()) ? NoItem : static_cast<size_t>(found-keys.begin());
    }
    size_t gapAt(Point local) const override
    {
        const int gap=(local.y+ItemHeight/2)/ItemHeight;
        return static_cast<size_t>(std::clamp(gap, 0, static_cast<int>(keys.size())));
    }
    bool reorderWithinNow() const override { return withinNow; }
    void gapChanged() override { ++gapChanges; }
    void dragChanged() override { ++dragChanges; }

    //! The item under the canvas point \p y, as a row would hand it out
    size_t itemAtLocal(int y) const
    {
        return (y < 0) ? NoItem : static_cast<size_t>(y/ItemHeight);
    }
};

//! A list control forwarding its drag-and-drop hooks to its KeyReorder.
class KeyList: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(KeyList, controls::Base)
    {
        reorder=std::make_unique<KeyReorder>(this);
    }

    std::unique_ptr<KeyReorder> reorder;
    std::vector<ItemDrop> drops;

    // Null-safe: a Reorderable deleted as the drag's source cancels the drag,
    // which calls these back while its owner's pointer is already null.
    bool dragAndDrop_draggable() override { return reorder != nullptr; }
    draganddrop::Package *dragAndDrop_getPackage(int x, int y) override
    {
        return reorder ? reorder->beginDrag(reorder->itemAtLocal(canvasPosToLocal(Point(x, y)).y)) : nullptr;
    }
    void dragAndDrop_startDragging(draganddrop::Package *, int, int) override
    {
        if(reorder)
            reorder->startDragging();
    }
    void dragAndDrop_endDragging(bool success, int, int) override
    {
        if(reorder)
            reorder->endDragging(success);
    }
    bool dragAndDrop_canAcceptPackage(draganddrop::Package *p) override
    {
        return reorder && reorder->canAcceptPackage(p);
    }
    void dragAndDrop_hoverEnter(draganddrop::Package *p, int x, int y) override
    {
        if(reorder)
            reorder->hoverEnter(p, x, y);
    }
    void dragAndDrop_hover(draganddrop::Package *p, int x, int y) override
    {
        if(reorder)
            reorder->hover(p, x, y);
    }
    void dragAndDrop_hoverLeave(draganddrop::Package *p) override
    {
        if(reorder)
            reorder->hoverLeave(p);
    }
    bool dragAndDrop_handleDrop(draganddrop::Package *p, int x, int y) override
    {
        return reorder && reorder->handleDrop(p, x, y);
    }

    void setKeys(std::vector<uint64_t> keys)
    {
        reorder->keys=std::move(keys);
        reorder->setOnItemDropped([this](const ItemDrop &drop) { drops.push_back(drop); });
    }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;
    KeyList *a=nullptr;
    KeyList *b=nullptr;

    //! Two KeyLists 100 wide, docked left (a: keys 1-4) and right (b: 11-14)
    //! on a 400x300 canvas, with empty canvas between them.
    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        a=list(DockPosition::Left, {1, 2, 3, 4});
        b=list(DockPosition::Right, {11, 12, 13, 14});
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

    KeyList *list(DockPosition dock, std::vector<uint64_t> keys)
    {
        KeyList *list=canvas->newChild<KeyList>();
        list->setDock(dock);
        list->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
        list->setWidth(100);
        list->setKeys(std::move(keys));
        return list;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void move(Point to)
    {
        canvas->inputMouseMoved(to.x, to.y, 0, 0);
        frame();
    }

    //! A canvas point at \p y units down \p list, in its middle
    Point on(KeyList *list, int y) const
    {
        return list->localPosToCanvas(Point(list->getWidth()/2, y));
    }

    //! Presses item \p index of \p list in its upper half and moves 6 units
    //! right, which starts the drag (the gap is the one before the item)
    void press(KeyList *list, size_t index)
    {
        clock+=1.0f;
        const Point at=on(list, static_cast<int>(index)*ItemHeight+ItemHeight/4);
        move(at);
        input::onMouseClicked(canvas, 0, true);
        frame();
        move(Point(at.x+6, at.y));
    }

    void release()
    {
        input::onMouseClicked(canvas, 0, false);
        frame();
    }
};

}//namespace

TEST_CASE("Reorderable: the target index is the item's index once it has left the source")
{
    Scene scene;
    Reorderable *a=scene.a->reorder.get();
    Reorderable *b=scene.b->reorder.get();

    // within one list: a gap after the item is one lower
    CHECK(Reorderable::targetIndexFor(a, 1, a, 0) == 0);
    CHECK(Reorderable::targetIndexFor(a, 1, a, 1) == 1);
    CHECK(Reorderable::targetIndexFor(a, 1, a, 2) == 1);
    CHECK(Reorderable::targetIndexFor(a, 1, a, 3) == 2);
    CHECK(Reorderable::targetIndexFor(a, 1, a, 4) == 3);
    // into another list the gap is the index
    CHECK(Reorderable::targetIndexFor(a, 1, b, 3) == 3);
    CHECK(Reorderable::targetIndexFor(a, 1, b, 0) == 0);
    CHECK(Reorderable::targetIndexFor(a, 1, a, Reorderable::NoItem) == Reorderable::NoItem);
}

TEST_CASE("Reorderable: the gap is the nearer one, and a move that keeps it calls nothing")
{
    Scene scene;
    KeyReorder *a=scene.a->reorder.get();
    int asked=0;
    a->setCanDrop([&](Reorderable *, uint64_t, Reorderable *, size_t) {
        ++asked;
        return true;
    });

    scene.press(scene.a, 1);
    REQUIRE(Reorderable::dragSource() == a);
    CHECK(a->dragKey() == 2);
    CHECK(a->dragIndex() == 1);
    CHECK(a->isDragged(2));
    CHECK_FALSE(a->isDragged(3));
    CHECK(a->dragChanges == 1);
    // the press's own item, in its upper half: the gap before it
    CHECK(Reorderable::dropTarget() == a);
    CHECK(a->gap() == 1);
    CHECK(a->showsGap());

    const int keys=a->keyCalls;
    const int indexes=a->indexCalls;
    const int changes=a->gapChanges;
    const int askedBefore=asked;

    // item 2's upper half (y 40..49): gap 2; its lower half (50..59): gap 3
    scene.move(scene.on(scene.a, 41));
    CHECK(a->gap() == 2);
    scene.move(scene.on(scene.a, 45));
    scene.move(scene.on(scene.a, 49));
    CHECK(a->gap() == 2);
    CHECK(a->gapChanges == changes+1);
    CHECK(asked == askedBefore+1);
    scene.move(scene.on(scene.a, 50));
    CHECK(a->gap() == 3);
    CHECK(a->gapChanges == changes+2);
    CHECK(asked == askedBefore+2);
    // past the last item: the end
    scene.move(scene.on(scene.a, 200));
    CHECK(a->gap() == 4);

    CHECK(a->keyCalls == keys);
    CHECK(a->indexCalls == indexes);
    CHECK(draganddrop::currentCursor() == cursor::Normal);
}

TEST_CASE("Reorderable: a drop within one list reports the move once, and nothing for a drop where the item was")
{
    SUBCASE("moved down past two items")
    {
        Scene scene;
        scene.press(scene.a, 1);
        // between items 2 and 3 (keys 3 and 4): gap 3, index 2 once key 2 has left
        scene.move(scene.on(scene.a, 3*ItemHeight-2));
        scene.release();

        REQUIRE(scene.a->drops.size() == 1);
        const ItemDrop &drop=scene.a->drops[0];
        CHECK(drop.source == scene.a->reorder.get());
        CHECK(drop.target == scene.a->reorder.get());
        CHECK(drop.key == 2);
        CHECK(drop.sourceIndex == 1);
        CHECK(drop.targetIndex == 2);
        CHECK(Reorderable::dragSource() == nullptr);
        CHECK(Reorderable::dropTarget() == nullptr);
        CHECK_FALSE(scene.a->reorder->isDragged(2));
        CHECK(scene.a->reorder->dragChanges == 2);
        // gweni moved nothing
        CHECK(scene.a->reorder->keys == std::vector<uint64_t>{1, 2, 3, 4});
    }

    SUBCASE("moved to the top")
    {
        Scene scene;
        scene.press(scene.a, 2);
        scene.move(scene.on(scene.a, 3));
        scene.release();
        REQUIRE(scene.a->drops.size() == 1);
        CHECK(scene.a->drops[0].key == 3);
        CHECK(scene.a->drops[0].sourceIndex == 2);
        CHECK(scene.a->drops[0].targetIndex == 0);
    }

    SUBCASE("dropped in either gap beside it")
    {
        Scene scene;
        scene.press(scene.a, 1);
        // the gap after it, then the gap before it
        scene.move(scene.on(scene.a, 2*ItemHeight+2));
        REQUIRE(scene.a->reorder->gap() == 2);
        scene.release();
        CHECK(scene.a->drops.empty());

        scene.press(scene.a, 1);
        REQUIRE(scene.a->reorder->gap() == 1);
        scene.release();
        CHECK(scene.a->drops.empty());
        CHECK_FALSE(draganddrop::isDragging());
    }
}

TEST_CASE("Reorderable: a drop on another member of the group reports the gap as the index")
{
    Scene scene;
    ReorderGroup group;
    group.add(scene.a->reorder.get());
    group.add(scene.b->reorder.get());

    scene.press(scene.a, 3);
    scene.move(scene.on(scene.b, 2*ItemHeight-3));
    CHECK(Reorderable::dropTarget() == scene.b->reorder.get());
    CHECK(scene.b->reorder->gap() == 2);
    CHECK(scene.a->reorder->gap() == Reorderable::NoItem);
    scene.release();

    CHECK(scene.a->drops.empty());
    REQUIRE(scene.b->drops.size() == 1);
    const ItemDrop &drop=scene.b->drops[0];
    CHECK(drop.source == scene.a->reorder.get());
    CHECK(drop.target == scene.b->reorder.get());
    CHECK(drop.key == 4);
    CHECK(drop.sourceIndex == 3);
    CHECK(drop.targetIndex == 2);
}

TEST_CASE("Reorderable: a list that refuses the item is no target, the cursor is No and a release cancels")
{
    SUBCASE("lists in no group take only their own items")
    {
        Scene scene;
        scene.press(scene.a, 0);
        scene.move(scene.on(scene.b, 30));
        CHECK(draganddrop::HoveredControl == nullptr);
        CHECK(Reorderable::dropTarget() == nullptr);
        CHECK(draganddrop::currentCursor() == cursor::No);
        scene.release();
        CHECK(scene.b->drops.empty());
        CHECK_FALSE(draganddrop::isDragging());
    }

    SUBCASE("a member not accepting from its group")
    {
        Scene scene;
        ReorderGroup group;
        group.add(scene.a->reorder.get());
        group.add(scene.b->reorder.get());
        scene.b->reorder->setAcceptsFromGroup(false);

        scene.press(scene.a, 0);
        scene.move(scene.on(scene.b, 30));
        CHECK(Reorderable::dropTarget() == nullptr);
        CHECK(draganddrop::currentCursor() == cursor::No);
        scene.release();
        CHECK(scene.b->drops.empty());

        // and the other way it takes the item
        scene.press(scene.b, 0);
        scene.move(scene.on(scene.a, 30));
        CHECK(Reorderable::dropTarget() == scene.a->reorder.get());
        scene.release();
        CHECK(scene.a->drops.size() == 1);
    }

    SUBCASE("members of two groups")
    {
        Scene scene;
        ReorderGroup one;
        ReorderGroup two;
        one.add(scene.a->reorder.get());
        two.add(scene.b->reorder.get());

        scene.press(scene.a, 0);
        scene.move(scene.on(scene.b, 30));
        CHECK(Reorderable::dropTarget() == nullptr);
        scene.release();
        CHECK(scene.b->drops.empty());
    }

    SUBCASE("not reorderable within itself, but taking from its group")
    {
        Scene scene;
        ReorderGroup group;
        group.add(scene.a->reorder.get());
        group.add(scene.b->reorder.get());
        scene.a->reorder->setReorderWithin(false);

        scene.press(scene.a, 0);
        CHECK(Reorderable::dropTarget() == nullptr);
        scene.move(scene.on(scene.a, 50));
        CHECK(Reorderable::dropTarget() == nullptr);
        CHECK(draganddrop::currentCursor() == cursor::No);
        // the item still goes to the other list
        scene.move(scene.on(scene.b, 50));
        CHECK(Reorderable::dropTarget() == scene.b->reorder.get());
        scene.release();
        CHECK(scene.a->drops.empty());
        CHECK(scene.b->drops.size() == 1);
    }

    SUBCASE("refusing its own items for now (a sorted table)")
    {
        Scene scene;
        scene.a->reorder->withinNow=false;

        scene.press(scene.a, 0);
        scene.move(scene.on(scene.a, 50));
        CHECK(Reorderable::dropTarget() == nullptr);
        CHECK(draganddrop::currentCursor() == cursor::No);
        scene.release();
        CHECK(scene.a->drops.empty());
    }
}

TEST_CASE("Reorderable: canDrop is asked once per gap change, and a refused gap shows nothing and cancels")
{
    Scene scene;
    KeyReorder *a=scene.a->reorder.get();
    std::vector<size_t> asked;

    // nothing above the pinned first item
    a->setCanDrop([&](Reorderable *source, uint64_t key, Reorderable *target, size_t index) {
        CHECK(source == a);
        CHECK(target == a);
        CHECK(key == 3);
        asked.push_back(index);
        return index > 0;
    });

    scene.press(scene.a, 2);
    // the press's gap, before key 3: index 2
    CHECK(asked == std::vector<size_t>{2});
    CHECK(a->showsGap());

    scene.move(scene.on(scene.a, 25));
    scene.move(scene.on(scene.a, 27));
    // gap 1, index 1
    CHECK(asked == std::vector<size_t>{2, 1});

    // gap 0: refused
    scene.move(scene.on(scene.a, 4));
    scene.move(scene.on(scene.a, 2));
    CHECK(asked == std::vector<size_t>{2, 1, 0});
    CHECK(a->gap() == 0);
    CHECK_FALSE(a->gapAllowed());
    CHECK_FALSE(a->showsGap());
    CHECK(draganddrop::isDropRefused());
    CHECK(draganddrop::currentCursor() == cursor::No);

    // back to an allowed gap the cursor is Normal again
    scene.move(scene.on(scene.a, 25));
    CHECK(a->showsGap());
    CHECK(draganddrop::currentCursor() == cursor::Normal);

    scene.move(scene.on(scene.a, 2));
    scene.release();
    CHECK(scene.a->drops.empty());
    CHECK_FALSE(draganddrop::isDragging());
    CHECK(draganddrop::currentCursor() == cursor::Normal);
}

TEST_CASE("Reorderable: inserts and removals during a drag keep the gap before its item, and the drop reports the indexes then")
{
    Scene scene;
    ReorderGroup group;
    KeyReorder *a=scene.a->reorder.get();
    KeyReorder *b=scene.b->reorder.get();
    group.add(a);
    group.add(b);

    // key 3 of a, over b before key 13 (gap 2)
    scene.press(scene.a, 2);
    scene.move(scene.on(scene.b, 2*ItemHeight+3));
    REQUIRE(b->gap() == 2);

    const int keys=a->keyCalls+b->keyCalls;
    const int indexes=a->indexCalls+b->indexCalls;

    // two items inserted at a's top, one at b's top
    a->keys.insert(a->keys.begin(), {100, 101});
    a->itemsInserted(0, 2);
    b->keys.insert(b->keys.begin(), 200);
    b->itemsInserted(0, 1);
    CHECK(a->dragIndex() == 4);
    CHECK(b->gap() == 3);

    // one removed below the gap, one above the item: the item and the gap
    // stay where they were among the others
    b->keys.erase(b->keys.begin()+4);
    b->itemsRemoved(4, 1);
    CHECK(b->gap() == 3);
    a->keys.erase(a->keys.begin()+1);
    a->itemsRemoved(1, 1);
    CHECK(a->dragIndex() == 3);
    // none of that asked the app anything
    CHECK(a->keyCalls+b->keyCalls == keys);
    CHECK(a->indexCalls+b->indexCalls == indexes);
    CHECK(draganddrop::isDragging());

    scene.release();
    REQUIRE(scene.b->drops.size() == 1);
    CHECK(scene.b->drops[0].key == 3);
    CHECK(scene.b->drops[0].sourceIndex == 3);
    CHECK(scene.b->drops[0].targetIndex == 3);
}

TEST_CASE("Reorderable: an edit the list cannot describe looks the dragged item up and finds the gap again")
{
    Scene scene;
    KeyReorder *a=scene.a->reorder.get();

    scene.press(scene.a, 1);
    scene.move(scene.on(scene.a, 3*ItemHeight+2));
    REQUIRE(a->gap() == 3);

    // reordered by the app: key 2 now last
    a->keys={4, 1, 3, 2};
    a->itemsChanged();
    CHECK(draganddrop::isDragging());
    CHECK(a->dragIndex() == 3);
    // the gap from the cursor where it was
    CHECK(a->gap() == 3);

    // fewer items than the gap: the gap found again is the end
    a->keys={4, 2};
    a->itemsChanged();
    CHECK(a->dragIndex() == 1);
    CHECK(a->gap() == 2);

    scene.move(scene.on(scene.a, 3));
    scene.release();
    REQUIRE(scene.a->drops.size() == 1);
    CHECK(scene.a->drops[0].key == 2);
    CHECK(scene.a->drops[0].sourceIndex == 1);
    CHECK(scene.a->drops[0].targetIndex == 0);
}

TEST_CASE("Reorderable: the dragged item removed cancels the drag with nothing reported")
{
    SUBCASE("told as a removal")
    {
        Scene scene;
        KeyReorder *a=scene.a->reorder.get();
        scene.press(scene.a, 1);
        scene.move(scene.on(scene.a, 70));

        a->keys.erase(a->keys.begin(), a->keys.begin()+2);
        a->itemsRemoved(0, 2);
        CHECK_FALSE(draganddrop::isDragging());
        CHECK(Reorderable::dragSource() == nullptr);
        CHECK(Reorderable::dropTarget() == nullptr);
        CHECK_FALSE(a->isDragged(2));
        CHECK(a->dragChanges == 2);
        scene.release();
        CHECK(scene.a->drops.empty());
    }

    SUBCASE("told as an edit it cannot describe")
    {
        Scene scene;
        KeyReorder *a=scene.a->reorder.get();
        scene.press(scene.a, 1);
        scene.move(scene.on(scene.a, 70));

        a->keys={1, 3, 4};
        a->itemsChanged();
        CHECK_FALSE(draganddrop::isDragging());
        scene.release();
        CHECK(scene.a->drops.empty());
    }

    SUBCASE("the drop handler removing it is a plain edit")
    {
        Scene scene;
        KeyReorder *a=scene.a->reorder.get();
        int cancelledEnds=0;

        // the app applies the drop at once, telling the list as it goes
        a->setOnItemDropped([&](const ItemDrop &drop) {
            scene.a->drops.push_back(drop);
            a->keys.erase(a->keys.begin()+static_cast<long>(drop.sourceIndex));
            a->itemsRemoved(drop.sourceIndex, 1);
            a->keys.insert(a->keys.begin()+static_cast<long>(drop.targetIndex), drop.key);
            a->itemsInserted(drop.targetIndex, 1);
            cancelledEnds+=draganddrop::isDragging() ? 0 : 1;
        });

        scene.press(scene.a, 0);
        scene.move(scene.on(scene.a, 50));
        scene.release();
        REQUIRE(scene.a->drops.size() == 1);
        CHECK(scene.a->drops[0].targetIndex == 2);
        CHECK(a->keys == std::vector<uint64_t>{2, 3, 1, 4});
        // the drag stayed the core's until the drop returned
        CHECK(cancelledEnds == 0);
        CHECK_FALSE(draganddrop::isDragging());
    }
}

TEST_CASE("Reorderable: group and member may go in either order, and a source deleted ends the drag")
{
    Scene scene;

    {
        ReorderGroup group;
        group.add(scene.a->reorder.get());
        CHECK(scene.a->reorder->getGroup() == &group);
    }
    CHECK(scene.a->reorder->getGroup() == nullptr);

    ReorderGroup group;
    group.add(scene.a->reorder.get());
    group.add(scene.b->reorder.get());
    scene.b->reorder.reset();
    CHECK(group.getMembers() == std::vector<Reorderable *>{scene.a->reorder.get()});

    // a's adapter replaced in the middle of its drag
    scene.press(scene.a, 0);
    REQUIRE(draganddrop::isDragging());
    scene.a->reorder.reset();
    CHECK_FALSE(draganddrop::isDragging());
    CHECK(Reorderable::dragSource() == nullptr);
    CHECK(group.getMembers().empty());
}
