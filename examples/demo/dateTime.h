/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_datetime_example_h_
#define _gweni_datetime_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class DateEdit;
class Label;
class TimeEdit;
}

//! DateEdits and TimeEdits in a form, today 2026-11-14: a date in ISO and in
//! D/M/Y, one limited to Nov 5 - Dec 18 on weekdays only, one that may be
//! empty; a time in 24 h, in 12 h and in 24 h stepping by 15 minutes; and a
//! label echoing every value.
class DateTimeDemo: public controls::Base
{
public:
    GWENI_CONTROL(DateTimeDemo, controls::Base);

public:
    //! The fields a test drives.
    controls::DateEdit *isoDate() const { return m_iso; }
    controls::DateEdit *dmyDate() const { return m_dmy; }
    controls::DateEdit *limitedDate() const { return m_limited; }
    controls::DateEdit *optionalDate() const { return m_optional; }
    controls::TimeEdit *time24() const { return m_time24; }
    controls::TimeEdit *time12() const { return m_time12; }
    controls::TimeEdit *quarterTime() const { return m_quarter; }
    controls::Label *valuesLabel() const { return m_values; }

    //! Whether a change sets the label's text (on by default): a test weighing
    //! a field's own cost turns it off, since the label's new text is laid out.
    void setLabelUpdates(bool updates);

private:
    void onValueChanged(event::Info info);
    void updateLabel();

    controls::DateEdit *m_iso=nullptr;
    controls::DateEdit *m_dmy=nullptr;
    controls::DateEdit *m_limited=nullptr;
    controls::DateEdit *m_optional=nullptr;
    controls::TimeEdit *m_time24=nullptr;
    controls::TimeEdit *m_time12=nullptr;
    controls::TimeEdit *m_quarter=nullptr;
    controls::Label *m_values=nullptr;
    bool m_labelUpdates=true;
};

}//namespace gweni

#endif//_gweni_datetime_example_h_
