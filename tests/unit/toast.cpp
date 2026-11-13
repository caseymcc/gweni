/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Toasts (Canvas::notify, controls::Toast; toast.h) on a real Canvas with a
 *  StubRenderer and a stepped clock, driven through the canvas's input calls.
 *
 *  Stacking: the host is Toast::Gap in from the canvas's top-right corner,
 *  Toast::Width wide, newest at the top, Gap between toasts; the host is a
 *  canvas overlay above an open modal and below the ToolTip box.
 *  Lifetime: each kind's default timeout (Info 4, Positive 4, Warning 6,
 *  Negative 8 s), the canvas's setToastTimeout and a per-call timeout; a toast
 *  is still shown just before its deadline and hidden at it, then deleted,
 *  and a timeout of 0 never expires. nextWorkTime() wakes for the earliest
 *  deadline, not before; no animation runs while toasts show.
 *  The hover pause: the mouse on any toast stands every toast's clock still
 *  (none expires however long it rests), and leaving resumes each with the
 *  time it had left (to the frame, not earlier). Removal: dismiss() hides it
 *  at once and the toast below closes the gap in that frame's layout; its
 *  close button dismisses it, a click on its body does not, and neither moves
 *  the app's keyboard focus; the mouse between toasts reaches the app; beyond
 *  the limit (5, or setToastLimit) the oldest goes, hovered or not; every
 *  primitive and text id comes back once the toasts are gone.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/toast.h>
#include <gweni/controls/toolTip.h>
#include <gweni/controls/windowButtons.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/primitiveIds.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <iterator>
#include <vector>

using namespace gweni;
using controls::Toast;
using controls::ToastKind;
using controls::ToastOptions;

namespace
{

size_t liveIds()
{
    return primitiveIdCount()-releasedPrimitiveIdCount()+textIdCount()-releasedTextIdCount();
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Button *appButton=nullptr;
    controls::TextBox *appText=nullptr;
    int appPresses=0;
    //! a quarter of a 1/60 s step is exact in binary: the clock in sixtieths
    double clock=10.0;

    struct Presses: public event::Handler
    {
        int *count;
        void pressed(event::Info) { ++*count; }
    } presses;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);

        // under where the stack goes
        appButton=canvas->newChild<controls::Button>();
        appButton->setText("App");
        appButton->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        appButton->setBounds(400, 0, 400, 600);
        presses.count=&appPresses;
        appButton->onPressCaller.add(&presses, &Presses::pressed);

        appText=canvas->newChild<controls::TextBox>();
        appText->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        appText->setBounds(10, 560, 120, 20);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        canvas->inputMouseButton(0, false);
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

    //! The clock \p seconds on, then a frame.
    void advance(double seconds)
    {
        clock+=seconds;
        frame();
    }

    void move(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        frame();
    }

    void click(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    void clickOn(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();
        click(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    void moveOnto(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();
        move(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    bool shown(Toast *toast) const
    {
        const std::vector<Toast *> all=canvas->toasts();
        return std::find(all.begin(), all.end(), toast) != all.end();
    }

    int position(controls::Base *control) const
    {
        const auto &children=canvas->getActualChildren();
        return int(std::distance(children.begin(), std::find(children.begin(), children.end(), control)));
    }
};

}//namespace

TEST_CASE("Toast: stacked in the top-right corner, newest at the top")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    Toast *first=canvas->notify(ToastKind::Info, "First");
    Toast *second=canvas->notify(ToastKind::Positive, "Second", ToastOptions{-1.0, "Saved"});
    scene.frame();

    controls::Base *host=canvas->toastHost();
    REQUIRE(host != nullptr);
    CHECK(host->getParent() == static_cast<controls::Base *>(canvas));
    CHECK(host->getX() == 800-Toast::Width-Toast::Gap);
    CHECK(host->getWidth() == Toast::Width);
    CHECK(first->getWidth() == Toast::Width);

    const Point firstAt=first->localPosToCanvas();
    const Point secondAt=second->localPosToCanvas();
    CHECK(secondAt.y == Toast::Gap);
    CHECK(firstAt.y == secondAt.y+second->getHeight()+Toast::Gap);
    CHECK(second->getTitle() == "Saved");
    CHECK_FALSE(second->getTitleLabel()->hidden());
    CHECK(first->getTitleLabel()->hidden());
    CHECK(canvas->toasts() == std::vector<Toast *>{first, second});

    // a multi-line text wraps inside the width, so the toast is taller
    Toast *longer=canvas->notify(ToastKind::Warning,
        "A warning long enough to wrap onto several lines inside the toast's fixed width, and then some more");
    scene.frame();
    CHECK(longer->getWidth() == Toast::Width);
    CHECK(longer->getHeight() > first->getHeight());
    CHECK(longer->getTextLabel()->getHeight() > first->getTextLabel()->getHeight());
}

TEST_CASE("Toast: each kind's timeout, the canvas's defaults and a call's own")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    CHECK(canvas->getToastTimeout(ToastKind::Info) == 4.0);
    CHECK(canvas->getToastTimeout(ToastKind::Positive) == 4.0);
    CHECK(canvas->getToastTimeout(ToastKind::Warning) == 6.0);
    CHECK(canvas->getToastTimeout(ToastKind::Negative) == 8.0);

    const double start=scene.clock;
    Toast *info=canvas->notify(ToastKind::Info, "info");
    Toast *positive=canvas->notify(ToastKind::Positive, "positive");
    Toast *warning=canvas->notify(ToastKind::Warning, "warning");
    Toast *negative=canvas->notify(ToastKind::Negative, "negative");
    const size_t infoId=info->getControlId();
    CHECK(info->getTimeout() == 4.0);
    CHECK(negative->getTimeout() == 8.0);
    scene.frame();

    // the wake is the first deadline, and not before it
    CHECK(canvas->nextWorkTime() == doctest::Approx(start+4.0));
    CHECK(canvas->nextWorkTime() >= start+4.0);

    scene.clock=start+4.0-1.0/60;
    scene.frame();
    CHECK(scene.shown(info));
    CHECK(scene.shown(positive));
    CHECK(canvas->getUpdateStats().animsRun == 0);

    scene.clock=start+4.0;
    scene.frame();
    CHECK_FALSE(scene.shown(info));
    CHECK_FALSE(scene.shown(positive));
    // hidden at once, deleted with the next delayed deletes
    if(controls::Base *stillThere=controls::Base::getControlById(infoId))
        CHECK(stillThere->hidden());
    CHECK(scene.shown(warning));
    CHECK(canvas->nextWorkTime() == controls::Canvas::WorkNow);

    scene.clock=start+6.0-1.0/60;
    scene.frame();
    CHECK(scene.shown(warning));
    scene.clock=start+6.0;
    scene.frame();
    CHECK_FALSE(scene.shown(warning));
    scene.clock=start+8.0-1.0/60;
    scene.frame();
    CHECK(scene.shown(negative));
    scene.clock=start+8.0;
    scene.frame();
    CHECK(canvas->toasts().empty());

    // the canvas's default, and a call's own, and 0: until dismissed
    canvas->setToastTimeout(ToastKind::Info, 1.5);
    const double later=scene.clock;
    Toast *quick=canvas->notify(ToastKind::Info, "quick");
    Toast *own=canvas->notify(ToastKind::Info, "own", ToastOptions{2.5, ""});
    Toast *sticky=canvas->notify(ToastKind::Negative, "sticky", ToastOptions{0.0, ""});
    CHECK(sticky->getTimeout() == 0.0);
    scene.clock=later+1.5;
    scene.frame();
    CHECK_FALSE(scene.shown(quick));
    CHECK(scene.shown(own));
    scene.clock=later+2.5;
    scene.frame();
    CHECK_FALSE(scene.shown(own));
    scene.advance(1000.0);
    CHECK(scene.shown(sticky));
    // nothing more is due
    scene.frame();
    CHECK(canvas->nextWorkTime() == controls::Canvas::NoWork);
}

TEST_CASE("Toast: hovering any toast pauses every timer, and leaving resumes each with its time left")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    const double start=scene.clock;
    Toast *info=canvas->notify(ToastKind::Info, "info");          // due at +4
    Toast *warning=canvas->notify(ToastKind::Warning, "warning"); // due at +6
    scene.frame();

    // at +1, onto the info toast (the older, lower one)
    scene.clock=start+1.0;
    scene.moveOnto(info);
    REQUIRE(canvas->inToastHost(HoveredControl));
    CHECK(canvas->toastsPaused());
    // long past both deadlines: both still there, and nothing to wake for
    scene.advance(20.0);
    CHECK(scene.shown(info));
    CHECK(scene.shown(warning));
    CHECK(canvas->nextWorkTime() == controls::Canvas::NoWork);

    // off at +21: 3 s left for info, 5 s for warning
    scene.clock=start+21.0;
    scene.move(10, 10);
    CHECK_FALSE(canvas->toastsPaused());
    scene.clock=start+24.0-1.0/60;
    scene.frame();
    CHECK(scene.shown(info));
    scene.clock=start+24.0;
    scene.frame();
    CHECK_FALSE(scene.shown(info));
    CHECK(scene.shown(warning));
    scene.clock=start+26.0-1.0/60;
    scene.frame();
    CHECK(scene.shown(warning));
    scene.clock=start+26.0;
    scene.frame();
    CHECK_FALSE(scene.shown(warning));
}

TEST_CASE("Toast: dismiss, the close button, the body and the focus")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    scene.clickOn(scene.appText);
    REQUIRE(KeyboardFocus == scene.appText);

    Toast *older=canvas->notify(ToastKind::Info, "older", ToastOptions{0.0, ""});
    Toast *newer=canvas->notify(ToastKind::Positive, "newer", ToastOptions{0.0, ""});
    scene.frame();
    const int topY=newer->localPosToCanvas().y;
    REQUIRE(older->localPosToCanvas().y > topY);

    // a click on a body does nothing, and keeps the app's focus
    scene.clickOn(older->getTextLabel());
    CHECK(scene.shown(older));
    CHECK(KeyboardFocus == scene.appText);
    CHECK(scene.appPresses == 0);

    // dismissing the newer: the older moves up into its place in that frame
    newer->dismiss();
    CHECK(newer->hidden());
    CHECK_FALSE(scene.shown(newer));
    newer->dismiss();
    scene.frame();
    CHECK(older->localPosToCanvas().y == topY);

    // its close button dismisses it, and keeps the app's focus too
    scene.clickOn(older->getCloseButton());
    CHECK_FALSE(scene.shown(older));
    CHECK(KeyboardFocus == scene.appText);
    CHECK(scene.appPresses == 0);
}

TEST_CASE("Toast: the mouse between and around the toasts reaches the app")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    Toast *a=canvas->notify(ToastKind::Info, "a");
    Toast *b=canvas->notify(ToastKind::Info, "b");
    scene.frame();

    // the gap between the two, inside the host
    const Point bAt=b->localPosToCanvas();
    const int gapY=bAt.y+b->getHeight()+Toast::Gap/2;
    REQUIRE(gapY < a->localPosToCanvas().y);
    CHECK(canvas->getControlAt(bAt.x+20, gapY) == static_cast<controls::Base *>(scene.appButton));
    scene.click(bAt.x+20, gapY);
    CHECK(scene.appPresses == 1);
    // and on a toast it does not
    scene.clickOn(a);
    CHECK(scene.appPresses == 1);
}

TEST_CASE("Toast: beyond the limit the oldest goes, hovered or not")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    CHECK(canvas->getToastLimit() == 5);
    std::vector<Toast *> made;
    for(int i=0; i < 5; ++i)
        made.push_back(canvas->notify(ToastKind::Info, "t"+std::to_string(i)));
    scene.frame();
    CHECK(canvas->toasts().size() == 5);

    Toast *sixth=canvas->notify(ToastKind::Info, "t5");
    CHECK(canvas->toasts().size() == 5);
    CHECK_FALSE(scene.shown(made[0]));
    CHECK(scene.shown(sixth));
    scene.frame();

    // hovered, the oldest still goes
    scene.moveOnto(made[1]);
    REQUIRE(canvas->toastsPaused());
    canvas->notify(ToastKind::Info, "t6");
    CHECK_FALSE(scene.shown(made[1]));
    CHECK(canvas->toasts().size() == 5);

    canvas->setToastLimit(2);
    canvas->notify(ToastKind::Info, "t7");
    CHECK(canvas->toasts().size() == 2);
}

TEST_CASE("Toast: above an open modal and below the ToolTip box")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    // a modal panel over the whole canvas
    controls::Base *modal=canvas->newChild<controls::Base>();
    modal->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    modal->setBounds(0, 0, 800, 600);
    canvas->pushModal(modal);
    Toast *toast=canvas->notify(ToastKind::Negative, "over the modal", ToastOptions{0.0, "Failed"});
    scene.frame();
    CHECK(scene.position(canvas->toastHost()) > scene.position(modal));
    CHECK(toast->getZIndex() > modal->getZIndex());
    CHECK(canvas->modalAllows(toast->getCloseButton()));

    // the tooltip box stays after it
    scene.appText->setToolTipText("tip");
    modal->hide();
    scene.moveOnto(scene.appText);
    scene.advance(1.0);
    scene.advance(1.0);
    REQUIRE(canvas->toolTipControl() != nullptr);
    CHECK(scene.position(canvas->toolTipControl()) > scene.position(canvas->toastHost()));

    // its close button works over the modal
    modal->show();
    canvas->pushModal(modal);
    scene.frame();
    scene.clickOn(toast->getCloseButton());
    CHECK(canvas->toasts().empty());
}

TEST_CASE("Toast: every primitive and text id comes back")
{
    Scene scene;
    controls::Canvas *canvas=scene.canvas;

    // the host made, and its ids counted in the baseline
    canvas->notify(ToastKind::Info, "warm-up")->dismiss();
    scene.frame();
    scene.frame();
    const size_t ids=liveIds();

    canvas->notify(ToastKind::Info, "expires");
    Toast *titled=canvas->notify(ToastKind::Warning, "titled", ToastOptions{0.0, "Title"});
    Toast *closed=canvas->notify(ToastKind::Negative, "closed by its button", ToastOptions{0.0, ""});
    scene.frame();
    CHECK(liveIds() > ids);

    scene.advance(5.0);
    titled->dismiss();
    scene.clickOn(closed->getCloseButton());
    scene.frame();
    scene.frame();
    CHECK(canvas->toasts().empty());
    CHECK(liveIds() == ids);
}
