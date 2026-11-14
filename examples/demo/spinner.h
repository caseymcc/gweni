/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_spinner_example_h_
#define _gweni_spinner_example_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
class Button;
class GroupBox;
class Spinner;
}

//! Spinners: the three sizes without a track and with one, a Small one in a
//! "Saving…" Button beside its text, a Medium one in a card (a GroupBox) with
//! Start and Stop buttons (it stays as a still ring while stopped), and a
//! button that hides and shows the Large spinner with a track.
class SpinnerDemo: public controls::Base
{
public:
    GWENI_CONTROL(SpinnerDemo, controls::Base);

public:
    //! Every spinner, in the order they were made.
    const std::vector<controls::Spinner *> &spinners() const { return m_spinners; }
    //! The card's spinner, which Start and Stop drive.
    controls::Spinner *cardSpinner() const { return m_card; }
    //! The one in the Saving… button.
    controls::Spinner *buttonSpinner() const { return m_inButton; }
    //! The one the Hide button hides and shows.
    controls::Spinner *hideTarget() const { return m_hideTarget; }
    controls::Button *startButton() const { return m_start; }
    controls::Button *stopButton() const { return m_stop; }
    controls::Button *hideButton() const { return m_hide; }
    controls::Button *savingButton() const { return m_saving; }
    controls::GroupBox *card() const { return m_cardBox; }

private:
    void onStart(event::Info info);
    void onStop(event::Info info);
    void onHide(event::Info info);

    std::vector<controls::Spinner *> m_spinners;
    controls::Spinner *m_card=nullptr;
    controls::Spinner *m_inButton=nullptr;
    controls::Spinner *m_hideTarget=nullptr;
    controls::Button *m_start=nullptr;
    controls::Button *m_stop=nullptr;
    controls::Button *m_hide=nullptr;
    controls::Button *m_saving=nullptr;
    controls::GroupBox *m_cardBox=nullptr;
};

}//namespace gweni

#endif//_gweni_spinner_example_h_
