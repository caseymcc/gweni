/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  LogView's following, on a real Canvas with the StubSkin, through the
 *  canvas's own input (the mouse, the wheel, keys): a following view keeps
 *  its last row's bottom at the viewport's bottom in the frame of an append,
 *  however many rows came. A wheel notch up, the thumb dragged up, the bar's
 *  up arrow, Up, PageUp, Home and scrollToRow() of a row above the last each
 *  detach it, telling onFollowingChanged once; detached, appends leave every
 *  row on screen where it was and the jump button shows over the bottom of
 *  the viewport, centred, with the count of rows appended since. A press on
 *  the button, End, scrollToRow() of the last row, a wheel scroll and Down
 *  reaching the end each follow it again, hiding the button. The up arrow
 *  detaches it too when the list takes the bar's move before the layout
 *  does: the idle re-measure after a resize, a block expanded between the
 *  press and the frame. A view that never detaches makes no jump button.
 *  Every frame is one layout pass.
 */
#include "headlessFixture.h"

#include <gweni/controls/logView.h>
#include <gweni/controls/scrollBarBar.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <map>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

using RowId=controls::LogView::RowId;

Rect onCanvas(controls::Base *control)
{
    const Point origin=control->localPosToCanvas();

    return Rect(origin.x, origin.y, control->getWidth(), control->getHeight());
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::LogView *log=nullptr;
    controls::BaseScrollBar *bar=nullptr;
    std::vector<int> followEvents;
    int appended=0;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(500, 400);

        log=canvas->newChild<controls::LogView>();
        log->setPos(10, 10);
        log->setSize(300, 200);
        log->setRemeasureBudget(0);
        log->onFollowingChanged.addCallback(canvas,
            [this](event::Handler &, event::Info info) { followEvents.push_back(static_cast<int>(info.integer)); });

        for(controls::Base *child : log->getActualChildren())
        {
            if(controls::BaseScrollBar *scrollBar=gweni_cast<controls::BaseScrollBar>(child))
            {
                if(!bar)
                    bar=scrollBar;
            }
        }
        REQUIRE(bar != nullptr);

        append(80);
        frame();
        // the keyboard goes to the view
        log->focus();
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

    void append(int count)
    {
        for(int i=0; i < count; ++i, ++appended)
        {
            String text="row "+std::to_string(appended);

            // every seventh row wraps onto a few lines
            if(appended%7 == 3)
                text+=" which is long enough to wrap onto more than one line at the view's width, twice over";
            log->append(controls::LogRow(text));
        }
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();

        const controls::LayoutStats &layout=canvas->getLayoutStats();

        CHECK(layout.requests == 0);
        CHECK(layout.passes <= 1);
        CHECK(!layout.renderLayout);
    }

    void key(int key)
    {
        canvas->inputModifierKey(key, true);
        canvas->inputModifierKey(key, false);
    }

    void wheel(int notches)
    {
        const Rect at=onCanvas(log);

        canvas->inputMouseMoved(at.x+at.w/3, at.y+at.h/2, 0, 0);
        canvas->inputMouseWheel(60*notches);
    }

    void click(const Point &at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
    }

    //! The rows on screen: each one's top in the viewport.
    std::map<RowId, int> shown() const
    {
        std::map<RowId, int> rows;

        for(size_t i=0; i < log->getRowCount(); ++i)
        {
            if(controls::Base *row=log->rowForItem(i))
                rows[log->rowIdAt(i)]=row->getBounds().y;
        }
        return rows;
    }

    //! The last row is on screen with its bottom at the viewport's.
    void checkAtEnd() const
    {
        controls::Base *last=log->rowControl(log->lastRowId());

        REQUIRE(last != nullptr);
        CHECK(last->getBounds().y+last->getBounds().h == log->getViewportHeight());
    }

    bool jumpShown() const
    {
        return log->getJumpButton() && !log->getJumpButton()->hidden();
    }

    void detachAndCheck(int events)
    {
        CHECK_FALSE(log->isFollowing());
        REQUIRE(followEvents.size() == static_cast<size_t>(events));
        CHECK(followEvents.back() == 0);
        frame();
        CHECK(jumpShown());

        // appends move nothing on screen, and the button counts them
        const auto before=shown();

        append(9);
        frame();
        CHECK(shown() == before);
        CHECK(log->getRowsSinceDetached() == 9);
        CHECK(log->getJumpButton()->getText() == controls::LogView::jumpText(9));
    }

    void follow()
    {
        log->setFollowing(true);
        frame();
        checkAtEnd();
    }
};

}//namespace

TEST_CASE("LogView follow: appends keep the last row at the bottom in their frame")
{
    Scene scene;

    CHECK(scene.log->isFollowing());
    scene.checkAtEnd();

    for(int count : {1, 3, 17, 40})
    {
        scene.append(count);
        scene.frame();
        CAPTURE(count);
        scene.checkAtEnd();
    }
    CHECK(scene.log->isFollowing());
    CHECK(scene.followEvents.empty());
    CHECK(scene.log->getJumpButton() == nullptr);
}

TEST_CASE("LogView follow: a wheel notch up detaches and the jump button follows again")
{
    Scene scene;

    scene.wheel(1);
    scene.detachAndCheck(1);

    // the button: over the bottom of the viewport, centred
    controls::internal::LogViewJumpButton *jump=scene.log->getJumpButton();
    const Rect &viewport=scene.log->getViewport();
    const Rect bounds=jump->getBounds();

    CHECK(bounds.y+bounds.h == viewport.y+viewport.h-controls::LogView::JumpGap);
    CHECK(std::abs((bounds.x+bounds.w/2)-(viewport.x+viewport.w/2)) <= 1);
    CHECK(bounds.w > 0);

    const Rect at=onCanvas(jump);

    scene.click(Point(at.x+at.w/2, at.y+at.h/2));
    CHECK(scene.log->isFollowing());
    REQUIRE(scene.followEvents.size() == 2);
    CHECK(scene.followEvents.back() == 1);
    scene.frame();
    CHECK_FALSE(scene.jumpShown());
    CHECK(scene.log->getRowsSinceDetached() == 0);
    scene.checkAtEnd();

    // a wheel notch down while following changes nothing
    scene.wheel(-1);
    CHECK(scene.log->isFollowing());
    CHECK(scene.followEvents.size() == 2);
}

TEST_CASE("LogView follow: Up PageUp Home and scrollToRow above the end detach")
{
    Scene scene;
    int events=0;

    for(int key : {static_cast<int>(keys::Up), static_cast<int>(keys::PageUp), static_cast<int>(keys::Home)})
    {
        CAPTURE(key);
        const int before=scene.log->getVerticalScrollOffset();

        scene.key(key);
        scene.detachAndCheck(++events);
        CHECK(scene.log->getVerticalScrollOffset() < before);
        if(key == keys::Home)
            CHECK(scene.log->getVerticalScrollOffset() == 0);
        scene.follow();
        ++events;
    }

    CHECK(scene.log->scrollToRow(scene.log->rowIdAt(10)));
    scene.detachAndCheck(++events);
    CHECK(scene.log->rowControl(scene.log->rowIdAt(10)) != nullptr);

    // the last row follows
    CHECK(scene.log->scrollToRow(scene.log->lastRowId()));
    CHECK(scene.log->isFollowing());
    scene.frame();
    scene.checkAtEnd();
    CHECK_FALSE(scene.log->scrollToRow(controls::LogView::NoRow));
}

TEST_CASE("LogView follow: End and a scroll back to the end follow again")
{
    Scene scene;

    scene.key(keys::PageUp);
    scene.detachAndCheck(1);
    scene.key(keys::End);
    CHECK(scene.log->isFollowing());
    scene.frame();
    scene.checkAtEnd();
    CHECK_FALSE(scene.jumpShown());

    // the wheel, down to the end
    scene.wheel(2);
    scene.detachAndCheck(3);
    for(int i=0; i < 50 && !scene.log->isFollowing(); ++i)
    {
        scene.wheel(-1);
        scene.frame();
    }
    CHECK(scene.log->isFollowing());
    CHECK(scene.followEvents.size() == 4);
    scene.frame();
    scene.checkAtEnd();
    CHECK_FALSE(scene.jumpShown());

    // Down, a line at a time
    scene.key(keys::PageUp);
    scene.detachAndCheck(5);
    for(int i=0; i < 200 && !scene.log->isFollowing(); ++i)
    {
        scene.key(keys::Down);
        scene.frame();
    }
    CHECK(scene.log->isFollowing());
    scene.frame();
    scene.checkAtEnd();
}

TEST_CASE("LogView follow: the thumb dragged up and the bar's up arrow detach")
{
    Scene scene;
    controls::Base *thumb=nullptr;

    for(controls::Base *child : scene.bar->getActualChildren())
    {
        if(gweni_cast<controls::internal::ScrollBarBar>(child))
            thumb=child;
    }
    REQUIRE(thumb != nullptr);

    const Rect at=onCanvas(thumb);
    const Point grab(at.x+at.w/2, at.y+at.h/2);

    scene.canvas->inputMouseMoved(grab.x, grab.y, 0, 0);
    scene.canvas->inputMouseButton(0, true);
    scene.canvas->inputMouseMoved(grab.x, grab.y-30, 0, -30);
    scene.canvas->inputMouseButton(0, false);
    scene.detachAndCheck(1);

    scene.follow();

    // the up arrow: the bar's first button, above the thumb
    const Rect barRect=onCanvas(scene.bar);

    scene.click(Point(barRect.x+barRect.w/2, barRect.y+2));
    scene.frame();
    CHECK_FALSE(scene.log->isFollowing());
    REQUIRE(scene.followEvents.size() == 3);
    CHECK(scene.followEvents.back() == 0);

    const auto before=scene.shown();

    scene.append(5);
    scene.frame();
    CHECK(scene.shown() == before);
    CHECK(scene.jumpShown());
}

// The bar's move is taken by whatever runs first after it, not only by the
// layout: the idle re-measure in the update phase (after a new width), or an
// edit that anchors at the offset (a block expanded). Either way the up arrow
// detaches the view, as it does with nothing in between.
TEST_CASE("LogView follow: the bar's up arrow detaches under the idle re-measure and an edit")
{
    Scene scene;

    // a new width: every item is measured again, one a frame
    scene.log->setRemeasureBudget(1);
    scene.log->setSize(280, 200);
    scene.frame();
    scene.frame();

    const Rect barRect=onCanvas(scene.bar);
    const Point upArrow(barRect.x+barRect.w/2, barRect.y+2);

    scene.click(upArrow);
    scene.frame();
    CHECK_FALSE(scene.log->isFollowing());
    REQUIRE(scene.followEvents.size() == 1);
    CHECK(scene.followEvents.back() == 0);

    scene.log->setRemeasureBudget(0);
    scene.follow();

    // a block on screen expanded between the press and the layout
    const RowId block=scene.log->append(controls::LogRow::block("block", {"a", "b", "c", "d", "e"}));

    scene.frame();
    scene.click(upArrow);
    CHECK(scene.log->setBlockExpanded(block, true));
    scene.frame();
    CHECK_FALSE(scene.log->isFollowing());
    REQUIRE(scene.followEvents.size() == 3);
    CHECK(scene.followEvents.back() == 0);
    CHECK(scene.jumpShown());
}
