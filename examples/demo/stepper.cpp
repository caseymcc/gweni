/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/stepper.h"
#include "gweni/controls/label.h"
#include "gweni/controls/stepper.h"
#include "gweni/controls/textBox.h"

namespace gweni
{

namespace
{

//! The linear stepper's height: its header, its tallest page and its bar.
constexpr int LinearHeight=140;

// A section's caption, docked Top in \p parent.
void caption(controls::Base *parent, const char *text)
{
    controls::Label *label=parent->newChild<controls::Label>();

    label->setText(text);
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(0, 6, 0, 6));
}

// A line of text docked Top in a step's page.
void pageText(controls::Base *page, const char *text)
{
    controls::Label *label=page->newChild<controls::Label>();

    label->setText(text);
    label->setDock(DockPosition::Top);
}

controls::Stepper *stepper(controls::Base *parent)
{
    controls::Stepper *stepper=parent->newChild<controls::Stepper>();

    stepper->setDock(DockPosition::Top);
    return stepper;
}

}//namespace

// The linear stepper across the top; under it the vertical non-linear one at
// the left and the one with a disabled step at the right.
GWENI_CONTROL_CONSTRUCTOR(StepperDemo)
{
    setDock(DockPosition::Center);
    setPadding(Padding(10, 4, 10, 4));

    caption(this, "Linear: Next asks for a name on step 2");
    m_linear=stepper(this);
    // a height of its own: a page of another height moves nothing below it
    m_linear->setHeight(LinearHeight);
    m_linear->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    pageText(m_linear->addStep("Account"), "Sign in or create an account.");

    controls::Base *details=m_linear->addStep("Details", "Your name");

    pageText(details, "Name (Next is refused while it is empty):");
    m_name=details->newChild<controls::TextBox>();
    m_name->setDock(DockPosition::Top);
    m_name->setWidth(200);
    m_name->setSizeFlags({SizeFlag::Fixed, SizeFlag::Elastic});
    pageText(m_linear->addStep("Extras"), "Pick any extras.");
    m_linear->setStepOptional(2);
    pageText(m_linear->addStep("Confirm"), "Check everything and finish.");
    m_linear->onStepChanging.add(this, &StepperDemo::onLinearChanging);
    m_linear->onFinish.add(this, &StepperDemo::onLinearFinish);

    controls::Base *row=newChild<controls::Base>();

    row->setDock(DockPosition::Top);
    row->setMargin(Margin(0, 12, 0, 0));

    controls::Base *left=row->newChild<controls::Base>();

    left->setDock(DockPosition::Left);
    left->setWidth(260);
    left->setSizeFlags({SizeFlag::Fixed, SizeFlag::Elastic});
    caption(left, "Vertical, non-linear: click a step");
    m_vertical=stepper(left);
    m_vertical->setOrientation(controls::Stepper::Orientation::Vertical);
    m_vertical->setLinear(false);
    pageText(m_vertical->addStep("Select campaign"), "Choose the campaign.");
    pageText(m_vertical->addStep("Create an ad group", "Targeting"), "Name the ad group.");
    pageText(m_vertical->addStep("Create an ad"), "Write the ad.");

    controls::Base *right=row->newChild<controls::Base>();

    right->setDock(DockPosition::Center);
    right->setMargin(Margin(16, 0, 0, 0));
    caption(right, "Step 3 disabled: Next passes over it");
    m_disabled=stepper(right);
    pageText(m_disabled->addStep("Order"), "The order.");
    pageText(m_disabled->addStep("Pay"), "Payment.");
    pageText(m_disabled->addStep("Gift"), "Not for this order.");
    pageText(m_disabled->addStep("Ship"), "Shipping.");
    m_disabled->setStepState(2, controls::Stepper::StepState::Disabled);
}

// Leaving the details step forward with no name is refused and marks it;
// with a name it goes, and the mark goes with it.
void StepperDemo::onLinearChanging(event::Info info)
{
    const int from=info.point.x;
    const int to=info.point.y;

    if((from != 1) || (to <= from))
        return;

    if(m_name->getText().empty())
    {
        m_linear->vetoStepChange();
        m_linear->setStepState(1, controls::Stepper::StepState::Error);
    }
    else
        m_linear->setStepState(1, controls::Stepper::StepState::Auto);
}

// Finished: every step shows done.
void StepperDemo::onLinearFinish(event::Info)
{
    for(int i=0; i < m_linear->getStepCount(); ++i)
        m_linear->setStepState(i, controls::Stepper::StepState::Done);
}

}//namespace gweni
