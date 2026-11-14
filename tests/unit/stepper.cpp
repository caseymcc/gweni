/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Stepper on a real Canvas with the real (un-init()'d) textured skin on a
 *  StubRenderer, clicked through input::onMouseClicked and keyed through
 *  input::onKeyEvent.
 *
 *  The looks: in Auto every step before the current one shows as done, the
 *  current one as current and the rest as pending, following the current
 *  step as it moves, and a connector is done exactly when the step after it
 *  shows as done; an explicit Done, Error or Disabled shows whatever the
 *  current step is, an Error step's indicator reading "!" and a done one
 *  nothing (its check drawn instead), and Auto gives the moving look back.
 *  setCurrentStep fires none of the events.
 *
 *  Navigation: next() and back() pass over Disabled steps; Next reads
 *  "Finish" exactly when no enabled step follows the current one, and then
 *  asks onStepChanging with to = getStepCount() and fires onFinish, the step
 *  unchanged. onStepChanging carries (from, to) in info.point and through
 *  getChangingFrom()/getChangingTo(); a handler that vetoes leaves the current
 *  step, the shown page and every look as they were and fires no
 *  onStepChanged, and one that also sets the step's state to Error makes its
 *  indicator show it. The Next and Back buttons are next() and back();
 *  setShowNavigation hides the bar.
 *
 *  Linear (the default) steps take no click and no focus. Non-linear: a
 *  click on a step goes to it through the same veto, a Disabled one does
 *  nothing; the steps are one Tab stop, the current step, in which Right and
 *  Left (Down and Up when vertical) move the focus over the enabled steps and
 *  Return and Space go to the focused one, through the veto too; Escape is
 *  not taken.
 *
 *  Only the current step's page is shown, and a page's content is never laid
 *  out while it is hidden: a probe in each page counts no layout entry until
 *  its step is current.
 */
#include "headlessFixture.h"

#include <gweni/controls/stepper.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <utility>
#include <vector>

using namespace gweni;
using controls::StepLook;
using controls::Stepper;
using controls::StepperStep;

namespace
{

//! A change's (from, to).
using Change=std::pair<int, int>;

//! A plain control that counts the layout entries it gets.
class LayoutProbe: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(LayoutProbe, controls::Base)
    {
        setDock(DockPosition::Top);
        setHeight(20);
        setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    }

    GWENI_DRAWS_NOTHING

    int entries=0;

protected:
    void calculateSize(skin::Base *skin, Dim dim) override
    {
        ++entries;
        ParentClass::calculateSize(skin, dim);
    }

    void arrange(skin::Base *skin, Dim dim) override
    {
        ++entries;
        ParentClass::arrange(skin, dim);
    }
};

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=nextClock();

    //! The input handler keeps the last click's time and place across cases,
    //! so each case's clock starts well after the last one's.
    static float nextClock()
    {
        static float start=0.0f;

        start+=1000.0f;
        return start;
    }

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        for(int key : {keys::Shift, keys::Tab})
            input::onKeyEvent(canvas, key, false);
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

    //! A stepper of \p count steps filling the canvas, a LayoutProbe in each
    //! page (into \p probes).
    Stepper *stepper(int count, std::vector<LayoutProbe *> *probes=nullptr)
    {
        Stepper *stepper=canvas->newChild<Stepper>();

        stepper->setDock(DockPosition::Center);
        for(int i=0; i < count; ++i)
        {
            controls::Base *page=stepper->addStep("Step "+std::to_string(i+1));
            LayoutProbe *probe=page->newChild<LayoutProbe>();

            if(probes)
                probes->push_back(probe);
        }
        frame();
        return stepper;
    }

    void click(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        clock+=1.0f;
        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        input::onMouseClicked(canvas, 0, true);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }
};

//! Records the stepper's events; vetoes while \p veto, setting the step it
//! leaves to Error when \p markError.
struct Events: public event::Handler
{
    Stepper *stepper=nullptr;
    std::vector<Change> changing;
    std::vector<Change> changingSeen;
    std::vector<Change> changed;
    int finished=0;
    bool veto=false;
    bool markError=false;

    explicit Events(Stepper *s): stepper(s)
    {
        stepper->onStepChanging.add(this, &Events::onChanging);
        stepper->onStepChanged.add(this, &Events::onChanged);
        stepper->onFinish.add(this, &Events::onFinished);
    }

    void onChanging(event::Info info)
    {
        changing.push_back(Change(info.point.x, info.point.y));
        changingSeen.push_back(Change(stepper->getChangingFrom(), stepper->getChangingTo()));
        CHECK(info.integer == static_cast<size_t>(info.point.y));
        if(veto)
        {
            stepper->vetoStepChange();
            if(markError)
                stepper->setStepState(info.point.x, Stepper::StepState::Error);
        }
    }

    void onChanged(event::Info info)
    {
        changed.push_back(Change(info.point.x, info.point.y));
    }

    void onFinished(event::Info)
    {
        ++finished;
    }
};

std::vector<StepLook> looks(const Stepper *stepper)
{
    std::vector<StepLook> result;

    for(int i=0; i < stepper->getStepCount(); ++i)
        result.push_back(stepper->getStepLook(i));
    return result;
}

std::vector<bool> connectorsDone(const Stepper *stepper)
{
    std::vector<bool> result;

    for(int i=0; i+1 < stepper->getStepCount(); ++i)
        result.push_back(stepper->getConnector(i)->isDone());
    return result;
}

//! Which pages are shown.
std::vector<bool> shownPages(const Stepper *stepper)
{
    std::vector<bool> result;

    for(int i=0; i < stepper->getStepCount(); ++i)
        result.push_back(!stepper->getPage(i)->hidden());
    return result;
}

}//namespace

TEST_CASE("Stepper: Auto states follow the current step, and the connectors into done steps")
{
    Scene scene;
    Stepper *stepper=scene.stepper(4);

    CHECK(stepper->getStepCount() == 4);
    CHECK(stepper->getCurrentStep() == 0);
    CHECK(looks(stepper) == std::vector<StepLook>{StepLook::Current, StepLook::Pending, StepLook::Pending,
        StepLook::Pending});
    CHECK(connectorsDone(stepper) == std::vector<bool>{false, false, false});

    stepper->setCurrentStep(2);
    CHECK(looks(stepper) == std::vector<StepLook>{StepLook::Done, StepLook::Done, StepLook::Current,
        StepLook::Pending});
    CHECK(connectorsDone(stepper) == std::vector<bool>{true, false, false});
    for(int i=0; i < 4; ++i)
        CHECK(stepper->getStep(i)->getLook() == stepper->getStepLook(i));
    CHECK(stepper->getStep(0)->getIndicatorText() == "");
    CHECK(stepper->getStep(2)->getIndicatorText() == "3");

    stepper->setCurrentStep(1);
    CHECK(looks(stepper) == std::vector<StepLook>{StepLook::Done, StepLook::Current, StepLook::Pending,
        StepLook::Pending});
    CHECK(connectorsDone(stepper) == std::vector<bool>{false, false, false});
}

TEST_CASE("Stepper: explicit states show whatever the current step is, and Auto gives the moving look back")
{
    Scene scene;
    Stepper *stepper=scene.stepper(4);

    stepper->setStepState(3, Stepper::StepState::Done);
    stepper->setStepState(1, Stepper::StepState::Error);
    stepper->setStepState(2, Stepper::StepState::Disabled);
    CHECK(looks(stepper) == std::vector<StepLook>{StepLook::Current, StepLook::Error, StepLook::Disabled,
        StepLook::Done});
    CHECK(connectorsDone(stepper) == std::vector<bool>{false, false, true});
    CHECK(stepper->getStep(1)->getIndicatorText() == "!");
    CHECK(stepper->getStep(3)->getIndicatorText() == "");
    CHECK(stepper->getStep(2)->isDisabled());

    // the current step in error shows the error
    stepper->setCurrentStep(1);
    CHECK(stepper->getStepLook(1) == StepLook::Error);

    for(int i=1; i < 4; ++i)
        stepper->setStepState(i, Stepper::StepState::Auto);
    CHECK(looks(stepper) == std::vector<StepLook>{StepLook::Done, StepLook::Current, StepLook::Pending,
        StepLook::Pending});
    CHECK_FALSE(stepper->getStep(2)->isDisabled());
}

TEST_CASE("Stepper: setCurrentStep fires none of the events")
{
    Scene scene;
    Stepper *stepper=scene.stepper(3);
    Events events(stepper);

    stepper->setCurrentStep(2);
    stepper->setCurrentStep(0);
    stepper->setCurrentStep(7);
    CHECK(stepper->getCurrentStep() == 0);
    CHECK(events.changing.empty());
    CHECK(events.changed.empty());
    CHECK(events.finished == 0);
}

TEST_CASE("Stepper: next and back pass over Disabled steps")
{
    Scene scene;
    Stepper *stepper=scene.stepper(5);
    Events events(stepper);

    stepper->setStepState(1, Stepper::StepState::Disabled);
    stepper->setStepState(2, Stepper::StepState::Disabled);

    CHECK(stepper->next());
    CHECK(stepper->getCurrentStep() == 3);
    CHECK(events.changing == std::vector<Change>{Change(0, 3)});
    CHECK(events.changed == std::vector<Change>{Change(0, 3)});

    CHECK(stepper->back());
    CHECK(stepper->getCurrentStep() == 0);
    // nothing before the first step
    CHECK_FALSE(stepper->back());
    CHECK(stepper->getCurrentStep() == 0);

    // a Disabled step is not gone to
    CHECK_FALSE(stepper->goTo(2));
    CHECK(stepper->getCurrentStep() == 0);

    // the last step disabled: Next on step 3 is Finish
    stepper->setStepState(4, Stepper::StepState::Disabled);
    stepper->setCurrentStep(3);
    CHECK(stepper->isAtLastStep());
    CHECK(stepper->nextButton()->getText() == "Finish");
}

TEST_CASE("Stepper: Next reads Finish on the last step, and Finish asks then fires onFinish")
{
    Scene scene;
    Stepper *stepper=scene.stepper(3);
    Events events(stepper);

    CHECK(stepper->nextButton()->getText() == "Next");
    scene.click(stepper->nextButton());
    CHECK(stepper->getCurrentStep() == 1);
    CHECK(stepper->nextButton()->getText() == "Next");
    scene.click(stepper->nextButton());
    CHECK(stepper->getCurrentStep() == 2);
    CHECK(stepper->nextButton()->getText() == "Finish");
    CHECK(events.finished == 0);

    scene.click(stepper->nextButton());
    CHECK(events.finished == 1);
    CHECK(stepper->getCurrentStep() == 2);
    REQUIRE_FALSE(events.changing.empty());
    CHECK(events.changing.back() == Change(2, 3));
    CHECK(events.changed.size() == 2);

    // a vetoed Finish fires no onFinish
    events.veto=true;
    CHECK_FALSE(stepper->next());
    CHECK(events.finished == 1);

    events.veto=false;
    scene.click(stepper->backButton());
    CHECK(stepper->getCurrentStep() == 1);
    CHECK(stepper->nextButton()->getText() == "Next");

    stepper->setShowNavigation(false);
    CHECK_FALSE(stepper->showsNavigation());
    CHECK(stepper->navigation()->hidden());
}

TEST_CASE("Stepper: a vetoed change leaves everything as it was, and the handler may mark the step Error")
{
    Scene scene;
    Stepper *stepper=scene.stepper(4);
    Events events(stepper);

    stepper->setCurrentStep(1);
    scene.frame();

    const std::vector<StepLook> before=looks(stepper);
    const std::vector<bool> connectorsBefore=connectorsDone(stepper);

    events.veto=true;
    CHECK_FALSE(stepper->next());
    CHECK(stepper->getCurrentStep() == 1);
    CHECK(shownPages(stepper) == std::vector<bool>{false, true, false, false});
    CHECK(looks(stepper) == before);
    CHECK(connectorsDone(stepper) == connectorsBefore);
    CHECK(events.changing == std::vector<Change>{Change(1, 2)});
    CHECK(events.changingSeen == std::vector<Change>{Change(1, 2)});
    CHECK(events.changed.empty());
    // outside a handler there is no change being asked about
    CHECK(stepper->getChangingFrom() == -1);
    CHECK(stepper->getChangingTo() == -1);

    // a Back vetoed too
    CHECK_FALSE(stepper->back());
    CHECK(stepper->getCurrentStep() == 1);

    events.markError=true;
    CHECK_FALSE(stepper->next());
    CHECK(stepper->getCurrentStep() == 1);
    CHECK(stepper->getStepLook(1) == StepLook::Error);
    CHECK(stepper->getStep(1)->getLook() == StepLook::Error);
    CHECK(stepper->getStep(1)->getIndicatorText() == "!");
    CHECK(events.changed.empty());

    // let through, the step leaves Error only when the app says
    events.veto=false;
    CHECK(stepper->next());
    CHECK(stepper->getCurrentStep() == 2);
    CHECK(stepper->getStepLook(1) == StepLook::Error);
    stepper->setStepState(1, Stepper::StepState::Auto);
    CHECK(stepper->getStepLook(1) == StepLook::Done);
}

TEST_CASE("Stepper: linear steps take no click and no focus")
{
    Scene scene;
    Stepper *stepper=scene.stepper(3);
    Events events(stepper);

    CHECK(stepper->isLinear());
    scene.click(stepper->getStep(2));
    CHECK(stepper->getCurrentStep() == 0);
    CHECK(events.changing.empty());
    CHECK(stepper->getTabStop() == nullptr);
    CHECK_FALSE(stepper->getStep(1)->getMouseInputEnabled());
    CHECK_FALSE(stepper->getStep(1)->getKeyboardInputEnabled());
    CHECK_FALSE(stepper->getStep(1)->isTabable());

    // the app's goTo still asks and goes
    CHECK(stepper->goTo(2));
    CHECK(events.changing == std::vector<Change>{Change(0, 2)});
}

TEST_CASE("Stepper: non-linear steps go to their step on a click, through the veto")
{
    Scene scene;
    Stepper *stepper=scene.stepper(4);
    Events events(stepper);

    stepper->setLinear(false);
    stepper->setStepState(3, Stepper::StepState::Disabled);

    scene.click(stepper->getStep(2));
    CHECK(stepper->getCurrentStep() == 2);
    CHECK(events.changing == std::vector<Change>{Change(0, 2)});
    CHECK(events.changed == std::vector<Change>{Change(0, 2)});

    // a Disabled step takes no click
    scene.click(stepper->getStep(3));
    CHECK(stepper->getCurrentStep() == 2);
    CHECK(events.changing.size() == 1);

    events.veto=true;
    scene.click(stepper->getStep(0));
    CHECK(stepper->getCurrentStep() == 2);
    REQUIRE_FALSE(events.changing.empty());
    CHECK(events.changing.back() == Change(2, 0));
    CHECK(events.changed.size() == 1);
    CHECK(shownPages(stepper) == std::vector<bool>{false, false, true, false});
}

TEST_CASE("Stepper: non-linear steps are one Tab stop, the arrows move the focus and Return or Space go")
{
    Scene scene;
    Stepper *stepper=scene.stepper(4);
    Events events(stepper);

    stepper->setLinear(false);
    stepper->setStepState(2, Stepper::StepState::Disabled);
    stepper->setCurrentStep(1);
    scene.frame();

    CHECK(stepper->getTabStop() == stepper->getStep(1));
    CHECK(stepper->getStep(1)->isTabable());
    CHECK_FALSE(stepper->getStep(0)->isTabable());

    stepper->getStep(1)->focus();
    // Right passes over the Disabled step; Up and Down are not a horizontal
    // stepper's
    scene.key(keys::Right);
    CHECK(KeyboardFocus == stepper->getStep(3));
    CHECK(stepper->getCurrentStep() == 1);
    scene.key(keys::Down);
    CHECK(KeyboardFocus == stepper->getStep(3));

    scene.key(keys::Return);
    CHECK(stepper->getCurrentStep() == 3);
    CHECK(events.changing == std::vector<Change>{Change(1, 3)});

    scene.key(keys::Left);
    scene.key(keys::Left);
    CHECK(KeyboardFocus == stepper->getStep(0));

    // Space goes through the veto too
    events.veto=true;
    scene.key(keys::Space);
    CHECK(stepper->getCurrentStep() == 3);
    REQUIRE_FALSE(events.changing.empty());
    CHECK(events.changing.back() == Change(3, 0));
    events.veto=false;
    scene.key(keys::Space);
    CHECK(stepper->getCurrentStep() == 0);

    // Escape is not the stepper's
    CHECK_FALSE(stepper->getStep(0)->onKeyEscape(true));

    // vertical: Down and Up move, Right does not
    stepper->setOrientation(Stepper::Orientation::Vertical);
    scene.frame();
    scene.key(keys::Down);
    CHECK(KeyboardFocus == stepper->getStep(1));
    scene.key(keys::Right);
    CHECK(KeyboardFocus == stepper->getStep(1));
    scene.key(keys::Up);
    CHECK(KeyboardFocus == stepper->getStep(0));

    // back to linear drops the focus and the Tab stop
    stepper->setLinear(true);
    CHECK(KeyboardFocus != stepper->getStep(0));
    CHECK(stepper->getTabStop() == nullptr);
}

TEST_CASE("Stepper: only the current page shows, and a hidden page is never laid out")
{
    Scene scene;
    std::vector<LayoutProbe *> probes;
    Stepper *stepper=scene.stepper(3, &probes);

    CHECK(shownPages(stepper) == std::vector<bool>{true, false, false});
    CHECK(probes[0]->entries > 0);
    CHECK(probes[1]->entries == 0);
    CHECK(probes[2]->entries == 0);
    CHECK(probes[0]->getHeight() == 20);

    CHECK(stepper->next());
    scene.frame();
    CHECK(shownPages(stepper) == std::vector<bool>{false, true, false});
    CHECK(probes[1]->entries > 0);
    CHECK(probes[2]->entries == 0);
    CHECK(stepper->getCurrentPage() == stepper->getPage(1));

    // the shown page fills the area above the navigation bar
    const controls::Base *page=stepper->getPage(1);

    CHECK(page->getWidth() == stepper->getBody()->getWidth());
    CHECK(stepper->navigation()->getY() >= page->getY()+page->getHeight());

    stepper->setOrientation(Stepper::Orientation::Vertical);
    scene.frame();
    CHECK(probes[2]->entries == 0);
    // vertical: the body under the current step, the next step under it
    CHECK(stepper->getBody()->getY() > stepper->getStep(1)->getY());
    CHECK(stepper->getStep(2)->getY() > stepper->getBody()->getY()+stepper->getBody()->getHeight());
    CHECK(stepper->getBody()->getX() == StepperStep::IndicatorSize+StepperStep::LabelGap);
}
