/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_timeedit_h_
#define _gweni_controls_timeedit_h_

#include <gweni/controls/sectionedEdit.h>
#include <gweni/date.h>

#include <optional>

namespace gweni
{
namespace controls
{

//! A time of day typed or stepped (QTimeEdit, WinUI's TimePicker without its
//! flyout): an internal::SectionedEdit in 24-hour "HH:mm" or, with
//! setTwelveHour(true), 12-hour "h:mm AP" form. Up and Down step the hour,
//! the minute (by setMinuteStep(), to the step's next multiple) or AM/PM
//! under the caret, wrapping inside the section; PageUp and PageDown take
//! twelve hours or ten minutes; A or P in the AP section sets AM or PM. Text
//! is read on Return and when the field loses the focus, in either clock
//! ("1430", "2:30 pm", "12 am" is 00:00, "12 pm" is 12:00), a time that is
//! none reverting. A time is clamped to setRange(). onValueChanged fires once
//! per change by the user, never from setTime or a setter.
//!
//! It has no popup. The toolkits' time pickers are a spinning list per part
//! (WinUI) or a clock face (Material), both ways of picking an hour and a
//! minute that take more steps than typing four digits, and both are for
//! touch, which gweni does not do yet (#615): a typed time and Up/Down per
//! part, with a coarse minute step for a slot-like time, cover what they
//! pick. Seconds and time zones are out: the value is a plain TimeOfDay.
//!
//! It draws as a TextBox, its width the format's at its font (see
//! SectionedEdit).
class GWENI_EXPORT TimeEdit: public internal::SectionedEdit
{
public:

    GWENI_CONTROL(TimeEdit, internal::SectionedEdit);

public:

    //! The time shown, empty for none (only with setAllowEmpty(true)). Set
    //! with no event, clamped to the range.
    void setTime(const std::optional<TimeOfDay> &time);
    std::optional<TimeOfDay> getTime() const;

    //! 12-hour "h:mm AP" (true) or 24-hour "HH:mm" (false, the default). The
    //! time is kept; only its text changes.
    void setTwelveHour(bool twelveHour);
    bool isTwelveHour() const;

    //! The times the field takes, both ends included, either empty for none.
    //! The time is clamped at once, with no event. An end that is no time
    //! (25:00) refuses the call, the range and the time kept.
    void setRange(const std::optional<TimeOfDay> &min, const std::optional<TimeOfDay> &max);
    std::optional<TimeOfDay> getMinTime() const;
    std::optional<TimeOfDay> getMaxTime() const;

protected:

    //! A time's formats alone.
    bool formatAllowed(bool date) const override { return !date; }
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_timeedit_h_
