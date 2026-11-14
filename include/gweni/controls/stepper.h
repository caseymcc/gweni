/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_stepper_h_
#define _gweni_controls_stepper_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>

#include <vector>

namespace gweni
{
namespace controls
{

class Stepper;

//! How a step is shown: what Stepper::getStepLook() works out from its state
//! and the current step.
enum class StepLook
{
    Pending,    //!< after the current step, a number in a plain circle
    Current,    //!< the current step, its number in the accent circle
    Done,       //!< a check in place of the number
    Error,      //!< "!" in place of the number, the title in the error colour
    Disabled    //!< muted, skipped by next/back and not clickable
};

//! One step's header in a Stepper: its indicator, a circle of IndicatorSize
//! with the step's number, a check (drawIcon kind 0) when done or "!" when in
//! error, and its title (the Button's text) beside it, LabelGap to its right,
//! with the subtitle and the "Optional" caption under the title when set. The
//! indicator is at the step's top left, so the connectors meet every
//! indicator's centre whatever lines a step's title block holds.
//!
//! The number and "!" are drawn by the skin's StepperStep renderer as a text
//! run of its own (Badge's way), not by a Text child: a step's look changing
//! writes its slots and glyphs and lays nothing out. The title's colour
//! follows the look (StepperTitle, StepperTitleCurrent, StepperErrorText,
//! the disabled one).
//!
//! It is the Stepper's to place and to give its look (setLook); a step is a
//! Button so that a non-linear stepper's steps take the mouse and the keys.
class GWENI_EXPORT StepperStep: public Button
{
public:

    GWENI_CONTROL(StepperStep, Button);

public:

    //! The indicator's diameter, and the units from it to the title.
    static constexpr int IndicatorSize=24;
    static constexpr int LabelGap=8;

    //! The stepper it is a step of, or null.
    Stepper *getStepper() const { return m_stepper; }
    //! Its index in the stepper.
    int getIndex() const { return m_index; }

    StepLook getLook() const { return m_look; }
    //! A new look marks the step's background and sets its title's colour;
    //! the same look marks nothing.
    void setLook(StepLook look);

    //! The text under the title; empty hides it.
    void setSubtitle(const String &subtitle);
    String getSubtitle() const;

    //! The "Optional" caption under the title (and the subtitle).
    void setOptional(bool optional);
    bool isOptional() const { return m_optional; }

    //! The indicator, in the step's own coordinates.
    Rect getIndicatorRect() const { return Rect(0, 0, IndicatorSize, IndicatorSize); }
    //! What the indicator shows as text: the step's number, "!" in error,
    //! nothing when done (the check is drawn instead).
    String getIndicatorText() const;
    //! Where that text is drawn, in the parent's coordinates (as a Text's
    //! textOrigin()): centred in the indicator.
    Point indicatorTextOrigin() const;

    //! The title colour for the look.
    void updateColors() override;

    void calculateSize(skin::Base *skin, Dim dim) override;

protected:

    friend class Stepper;

    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

    //! The lines under the title that show, their Texts made on first use.
    internal::Text *makeCaption(internal::Text *&text);
    int titleBlockHeight() const;

    Stepper *m_stepper=nullptr;
    int m_index=0;
    StepLook m_look=StepLook::Pending;
    bool m_optional=false;
    internal::Text *m_subtitle=nullptr;
    internal::Text *m_optionalText=nullptr;
    //! The indicator text's measured sizes: the number's and "!"'s.
    Point m_numberSize;
    Point m_errorSize;
};

//! The 1-unit rule joining two steps' indicators, through its middle along
//! its length (horizontal) or its height (vertical); in the done colour
//! (StepperConnectorDone) when the step after it shows as done.
class GWENI_EXPORT StepperConnector: public Base
{
public:

    GWENI_CONTROL(StepperConnector, Base);

public:

    void setDone(bool done);
    bool isDone() const { return m_done; }

    void setVertical(bool vertical);
    bool isVertical() const { return m_vertical; }

protected:

    bool m_done=false;
    bool m_vertical=false;
};

//! Numbered steps with a page each and one shown at a time (Material's and
//! MUI's Stepper, Qt's QWizard). addStep() returns the step's page, a Base the
//! app fills; only the current step's page is shown and the others stay
//! hidden, as a TabControl's pages are, so a hidden page is never laid out.
//!
//! PageControl is the older, plainer form of this: pages and Next/Back/Finish
//! buttons, with no step indicators, no states and no veto. The Stepper keeps
//! its approach (one container holding every page, the current one alone
//! shown) and adds the header; PageControl is left as it is.
//!
//! Each step has a state (setStepState): Auto, the default, shows the steps
//! before the current one as done and those after it as pending; Done, Error
//! and Disabled show as said whatever the current step is. getStepLook() is
//! what shows.
//!
//! Horizontal (the default): the steps in a row, a StepperConnector between
//! each two, stretched to the stepper's width, and the page under the whole
//! header. Vertical (MUI's vertical stepper): the steps in a column, the
//! current step's page under its header, indented to its title, with the
//! connector running down beside it; the steps after it move with the page.
//!
//! The steps, connectors and the body (the page area) are owner-placed by the
//! stepper, which draws nothing itself.
class GWENI_EXPORT Stepper: public Base
{
public:

    GWENI_CONTROL(Stepper, Base);

public:

    //! A container: its steps, connectors and pages draw.
    GWENI_DRAWS_NOTHING

    ~Stepper() override;

    enum class Orientation
    {
        Horizontal,
        Vertical
    };

    enum class StepState
    {
        Auto,
        Done,
        Error,
        Disabled
    };

    //! Units between a step and a horizontal connector, the least length of
    //! one, and between an indicator and a vertical connector.
    static constexpr int ConnectorGap=8;
    static constexpr int ConnectorMinLength=24;
    static constexpr int VerticalConnectorGap=4;
    //! Units between the horizontal header and the page.
    static constexpr int HeaderGap=16;
    //! Units between two steps in a vertical stepper, and between the
    //! current step and its page.
    static constexpr int StepSpacing=24;
    static constexpr int BodyGap=8;

    //! A new step at the end, titled \p title with \p subtitle under it, and
    //! its page, hidden unless it is the current step (the first step added
    //! is).
    Base *addStep(const String &title, const String &subtitle=String());

    int getStepCount() const { return static_cast<int>(m_steps.size()); }
    int getCurrentStep() const { return m_current; }
    //! Makes step \p index current: its page shown, the old one hidden, the
    //! looks that change marked. It fires nothing; nothing out of range.
    void setCurrentStep(int index);

    void setStepState(int index, StepState state);
    StepState getStepState(int index) const;
    //! What step \p index shows (see the class comment).
    StepLook getStepLook(int index) const;

    //! An "Optional" caption under the step's title.
    void setStepOptional(int index, bool optional=true);
    bool isStepOptional(int index) const;

    //! Null out of range.
    Base *getPage(int index) const;
    Base *getCurrentPage() const { return getPage(m_current); }
    StepperStep *getStep(int index) const;
    //! The connector between step \p index and the next one.
    StepperConnector *getConnector(int index) const;
    //! The control holding the pages, placed under the header (horizontal)
    //! or the current step (vertical).
    Base *getBody() const { return m_body; }

    void setOrientation(Orientation orientation);
    Orientation getOrientation() const { return m_orientation; }
    bool isVertical() const { return m_orientation == Orientation::Vertical; }

    void calculateSize(skin::Base *skin, Dim dim) override;

protected:

    void arrange(skin::Base *skin, Dim dim) override;

    //! Every step's look and every connector's colour from the states and
    //! the current step; only what changed is marked.
    void updateLooks();

    struct Step
    {
        StepState state=StepState::Auto;
        Base *page=nullptr;
    };

    std::vector<Step> m_stepData;
    std::vector<StepperStep *> m_steps;
    std::vector<StepperConnector *> m_connectors;
    Base *m_body=nullptr;
    Base *m_pageArea=nullptr;
    int m_current=0;
    Orientation m_orientation=Orientation::Horizontal;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_stepper_h_
