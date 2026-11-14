/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_calendar_example_h_
#define _gweni_calendar_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Calendar;
class Label;
}

//! Four Calendars on November 2026, today the 14th: a single-select one with
//! today set, a range one, one with limits and a weekday filter, and one
//! whose week starts on Sunday; and a label naming what each has selected.
class CalendarDemo: public controls::Base
{
public:
    GWENI_CONTROL(CalendarDemo, controls::Base);

public:
    //! The calendars a test drives.
    controls::Calendar *singleCalendar() const { return m_single; }
    controls::Calendar *rangeCalendar() const { return m_range; }
    controls::Calendar *limitedCalendar() const { return m_limited; }
    controls::Calendar *sundayCalendar() const { return m_sunday; }
    controls::Label *selectionLabel() const { return m_selection; }

    //! Whether a selection change sets the label's text (on by default): a
    //! test weighing a calendar's own cost turns it off, since the label's new
    //! text is laid out.
    void setLabelUpdates(bool updates);

private:
    void onSelectionChanged(event::Info info);
    void updateLabel();

    controls::Calendar *m_single=nullptr;
    controls::Calendar *m_range=nullptr;
    controls::Calendar *m_limited=nullptr;
    controls::Calendar *m_sunday=nullptr;
    controls::Label *m_selection=nullptr;
    bool m_labelUpdates=true;
};

}//namespace gweni

#endif//_gweni_calendar_example_h_
