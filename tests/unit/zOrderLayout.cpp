/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Docked children are laid out in the order their holder holds them, so a
 *  z-order call on one (sendToBack, bringToFront, bringNextToControl) moves
 *  the rows docked with it. The call re-lays the holder, the control whose
 *  child list it changed, which for a child of a control with an inner panel
 *  (a ScrollControl) is the panel, not the logical parent (#486). Rows docked
 *  Top in a ScrollControl, each call followed by one frame: every row is
 *  where its new place in the order puts it, in one pass, and layout
 *  verification's full pass finds nothing to change. A row's setPlacement
 *  re-lays the panel the same way: owner-placed, the rows after it close up,
 *  and given back it is docked in its place again.
 *
 *  On a panel holding its rows itself the calls cost what they always did:
 *  one pass, entering what an invalidate() of the panel enters and each row
 *  given a new place once more.
 */
#include "headlessFixture.h"

#include <gweni/controls/scrollControl.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

constexpr int RowCount=5;
constexpr int RowHeight=20;

//! A row that asks for a fixed size.
class Row: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Row, controls::Base)
    {}

    void calculateSize(skin::Base * /*skin*/, Dim dim) override
    {
        if(dim == Dim::X)
            m_preferredSize.width=60;
        else
            m_preferredSize.height=RowHeight;
    }
};

//! RowCount rows docked Top, in a ScrollControl's inner panel or in a plain
//! panel holding them itself.
struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    //! The control the rows are added to.
    controls::Base *owner;
    //! The control holding them.
    controls::Base *holder;
    std::vector<Row *> rows;
    //! The settled y of the first row.
    int top=0;

    explicit Scene(bool innerPanel)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        if(innerPanel)
        {
            controls::ScrollControl *scroll=canvas->newChild<controls::ScrollControl>();

            scroll->setDock(DockPosition::Center);
            owner=scroll;
        }
        else
        {
            owner=canvas->newChild<controls::Base>();
            owner->setDock(DockPosition::Center);
        }

        for(int i=0; i<RowCount; ++i)
        {
            Row *row=owner->newChild<Row>();

            row->setDock(DockPosition::Top);
            rows.push_back(row);
        }
        holder=rows.front()->getActualParent();
        REQUIRE(holder != nullptr);
        CHECK((holder != owner) == innerPanel);

        settle();
        top=rows.front()->getBounds().y;
        checkOrder(rows);
    }

    ~Scene()
    {
        delete canvas;
    }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i<10 && frame().passes != 0; ++i)
            ;
    }

    //! Each row of `order` a row below the one before it, from the top.
    void checkOrder(const std::vector<Row *> &order) const
    {
        for(size_t i=0; i<order.size(); ++i)
        {
            const size_t index=static_cast<size_t>(
                std::find(rows.begin(), rows.end(), order[i])-rows.begin());

            INFO("row " << index << " at place " << i);
            CHECK(order[i]->getBounds().y == top+static_cast<int>(i)*RowHeight);
        }
    }

    //! Runs `change` and one frame, and returns that frame's stats. The frame
    //! lays out in one pass and, with `verify`, layout verification's full
    //! pass changes nothing. That pass leaves its own bounds, so a case
    //! checking where the pass put the rows runs without it.
    template<typename Change>
    controls::LayoutStats changedFrame(Change change, bool verify)
    {
        const size_t failures=controls::layoutVerifyFailures();
        const bool wasVerifying=controls::layoutVerifyEnabled();

        controls::setLayoutVerify(verify);
        change();
        const controls::LayoutStats stats=frame();
        controls::setLayoutVerify(wasVerifying);

        INFO("re-dirtied by " << std::string(stats.redirtiedType?stats.redirtiedType:"none") <<
            "#" << stats.redirtiedId);
        CHECK(controls::layoutVerifyFailures() == failures);
        CHECK(stats.passes == 1);
        CHECK(stats.redirtiedType == nullptr);
        CHECK_FALSE(stats.renderLayout);
        return stats;
    }
};

//! The z-order changes the cases make, each with the row order it leaves.
struct Reorder
{
    const char *name;
    void (*apply)(const std::vector<Row *> &rows);
    std::vector<int> order;
};

const std::vector<Reorder> &reorders()
{
    static const std::vector<Reorder> list={
        {"bringToFront of the first row",
            [](const std::vector<Row *> &rows) { rows[0]->bringToFront(); }, {1, 2, 3, 4, 0}},
        {"sendToBack of the last row",
            [](const std::vector<Row *> &rows) { rows[4]->sendToBack(); }, {4, 0, 1, 2, 3}},
        {"bringNextToControl in front of a row",
            [](const std::vector<Row *> &rows) { rows[0]->bringNextToControl(rows[3], false); },
            {1, 2, 0, 3, 4}},
        {"bringNextToControl behind a row",
            [](const std::vector<Row *> &rows) { rows[0]->bringNextToControl(rows[3], true); },
            {1, 2, 3, 0, 4}},
        {"bringNextToControl behind a row, from the back",
            [](const std::vector<Row *> &rows) { rows[4]->bringNextToControl(rows[1], true); },
            {0, 1, 4, 2, 3}},
    };

    return list;
}

std::vector<Row *> inOrder(const Scene &scene, const std::vector<int> &order)
{
    std::vector<Row *> out;

    for(int index : order)
        out.push_back(scene.rows[static_cast<size_t>(index)]);
    return out;
}

} // namespace

TEST_CASE("z order: a reorder re-lays the rows of a ScrollControl's inner panel")
{
    for(const Reorder &reorder : reorders())
    for(bool verify : {false, true})
    {
        const std::string name=reorder.name;

        CAPTURE(name);
        CAPTURE(verify);

        Scene scene(true);

        scene.changedFrame([&] { reorder.apply(scene.rows); }, verify);
        scene.checkOrder(inOrder(scene, reorder.order));

        // the holder's list is the new order
        const controls::Base::List &held=scene.holder->getChildren();
        const std::vector<Row *> expected=inOrder(scene, reorder.order);
        REQUIRE(held.size() == expected.size());
        CHECK(std::equal(held.begin(), held.end(), expected.begin()));

        // and the next frame has nothing left to lay out
        CHECK(scene.frame().passes == 0);
    }
}

TEST_CASE("z order: a reorder on a panel holding its rows costs a panel invalidate")
{
    for(const Reorder &reorder : reorders())
    for(bool verify : {false, true})
    {
        const std::string name=reorder.name;

        CAPTURE(name);
        CAPTURE(verify);

        Scene invalidated(false);
        const controls::LayoutStats expected=
            invalidated.changedFrame([&] { invalidated.holder->invalidate(); }, verify);

        Scene scene(false);
        const controls::LayoutStats stats=
            scene.changedFrame([&] { reorder.apply(scene.rows); }, verify);

        scene.checkOrder(inOrder(scene, reorder.order));

        // a row given a new place is entered once more, in its Y arrange
        size_t moved=0;

        for(size_t i=0; i<reorder.order.size(); ++i)
        {
            if(reorder.order[i] != static_cast<int>(i))
                ++moved;
        }
        CHECK(stats.visited == expected.visited+moved);
        CHECK(stats.childrenScanned == expected.childrenScanned);
        CHECK(stats.measureCalls == expected.measureCalls);
        CHECK(stats.requests == expected.requests);
        CHECK(scene.frame().passes == 0);
    }
}

TEST_CASE("z order: a placement change re-lays the rows of a ScrollControl's inner panel")
{
    for(bool verify : {false, true})
    {
        CAPTURE(verify);

        Scene scene(true);
        Row *moved=scene.rows[1];
        const Rect at=moved->getBounds();

        // owner-placed, the row keeps its bounds and the rows after it close up
        scene.changedFrame([&] { moved->setPlacement(controls::Placement::Owner); }, verify);
        CHECK((moved->getBounds() == at));
        scene.checkOrder({scene.rows[0], scene.rows[2], scene.rows[3], scene.rows[4]});
        CHECK(scene.frame().passes == 0);

        // given back, it is docked in its place again
        scene.changedFrame([&] { moved->setPlacement(controls::Placement::Parent); }, verify);
        scene.checkOrder(scene.rows);
        CHECK(scene.frame().passes == 0);
    }
}
