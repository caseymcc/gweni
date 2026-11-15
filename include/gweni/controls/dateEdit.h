/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dateedit_h_
#define _gweni_controls_dateedit_h_

#include <gweni/controls/sectionedEdit.h>
#include <gweni/date.h>

#include <functional>
#include <optional>

namespace gweni
{
namespace controls
{

//! A date typed or stepped (QDateEdit, WinUI's DatePicker): an
//! internal::SectionedEdit in a date format, "YYYY-MM-DD" unless setFormat()
//! gives another ("DD/MM/YYYY", "M/D/YYYY", ...; a time's is refused). Up and
//! Down step the day, month or year under the caret, wrapping inside the
//! section (the 31st to the 1st of the same month, December to January of
//! the same year), a month or year step clamping the day; PageUp and PageDown
//! take a year (ten in the year section). Text is read on Return and when the
//! field loses the focus, leniently in the format's order ("2026-11-1",
//! "11/1/2026" for M/D/YYYY, "20261101"); a date that is none, or that
//! setDateFilter() refuses, reverts. A date is clamped to setRange(). An
//! empty field is no date when setAllowEmpty(true) allows it. onValueChanged
//! fires once per change by the user, never from setDate or a setter.
//!
//! The range and the filter are the ones its calendar takes, so a typed date
//! and a picked one agree. It draws as a TextBox, its width the format's at
//! its font (see SectionedEdit).
class GWENI_EXPORT DateEdit: public internal::SectionedEdit
{
public:

    GWENI_CONTROL(DateEdit, internal::SectionedEdit);

public:

    //! The date shown, empty for none (only with setAllowEmpty(true)). Set
    //! with no event, clamped to the range; the filter is not asked.
    void setDate(const std::optional<Date> &date);
    std::optional<Date> getDate() const;

    //! The dates the field takes, both ends included, either empty for none
    //! (the calendar's setDateLimits). The date is clamped at once, with no
    //! event.
    void setRange(const std::optional<Date> &min, const std::optional<Date> &max);
    std::optional<Date> getMinDate() const;
    std::optional<Date> getMaxDate() const;

    //! Dates the filter returns false for are refused: typed, they revert;
    //! a step passes over them (a weekday-only field steps Friday to Monday).
    //! A null filter takes every date.
    void setDateFilter(std::function<bool(const Date &)> filter);
    //! Whether the field takes \p date: in the range and the filter's.
    bool isDateEnabled(const Date &date) const;

    //! The app's today: what a step from an empty field shows (clamped), and
    //! the date a field that may not be empty starts with when it has none.
    //! There is no clock in gweni (gweni/date.h): an app that wants today
    //! says which day it is. 1970-01-01 until set.
    void setToday(const Date &today);
    const Date &getToday() const { return getEmptyStepValue().date; }

protected:

    //! A date's formats alone.
    bool formatAllowed(bool date) const override { return date; }
    //! The filter.
    bool acceptsValue(const Value &value) const override;

    std::function<bool(const Date &)> m_filter;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_dateedit_h_
