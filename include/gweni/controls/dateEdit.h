/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dateedit_h_
#define _gweni_controls_dateedit_h_

#include <gweni/controls/button.h>
#include <gweni/controls/calendar.h>
#include <gweni/controls/sectionedEdit.h>
#include <gweni/date.h>

#include <functional>
#include <optional>

namespace gweni
{
namespace controls
{

class DateEdit;
class Popup;

namespace internal
{

//! A DateEdit's calendar button, docked at the field's right inside it: a
//! press opens the calendar, or closes it when it is open. It takes no
//! keyboard input, so a press on it focuses the field (Base's press looks up
//! for the nearest control that takes it), and it is no Tab stop.
class GWENI_EXPORT DateEditButton: public Button
{
public:

    GWENI_CONTROL(DateEditButton, Button);
};

//! The Calendar a DateEdit opens: Escape closes the popup and gives the focus
//! back to the field (the canvas offers the key to the focus in the popup
//! first).
class GWENI_EXPORT DateEditCalendar: public Calendar
{
public:

    GWENI_CONTROL(DateEditCalendar, Calendar);

public:

    bool onKeyEscape(bool down) override;

    void setOwner(DateEdit *owner);

private:

    //! The DateEdit's id: looked up, so a calendar outliving it does nothing.
    size_t m_ownerId=0;
};

}//namespace internal

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
//! **The calendar.** A button docked at its right (internal::DateEditButton),
//! a click on it, Alt+Down or F4 open a Popup below the field (Base::
//! createPopup, made on the first open and kept, with its Calendar) holding a
//! Calendar in Single mode with the field's date selected, its range as the
//! calendar's limits, its filter, its today and its cursor on the date (on
//! today's, clamped, for an empty field), and the keyboard focus. Typed text
//! is read first. Choosing a day (a click, Space or Return) sets the field
//! (onValueChanged if it changed), closes the popup and gives the focus back
//! to the field; Escape closes it with nothing changed and the focus back. A
//! press outside closes it as any popup's does, the focus going where the
//! press puts it. The range and the filter are the calendar's, so a typed
//! date and a picked one agree.
//!
//! The canvas shows one popup at a time: a DateEdit inside a Popup closes
//! that popup when its calendar opens, as a ColorEdit's picker does.
//!
//! It draws as a TextBox, its width the format's at its font with the button
//! (see SectionedEdit).
class GWENI_EXPORT DateEdit: public internal::SectionedEdit
{
public:

    GWENI_CONTROL(DateEdit, internal::SectionedEdit);

public:

    //! The calendar button's width. The button is docked Right inside the
    //! field and counted in its width, so the format's widest text fits left
    //! of it; text typed past that scrolls under it, as in a NumericUpDown.
    static constexpr int ButtonWidth=16;

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

    //! Opens the calendar (see the class comment); nothing when it is open,
    //! the field is disabled, read-only (setEditable(false)) or not on a
    //! canvas.
    void openCalendar();
    //! Closes it, the focus back to the field when \p focusField.
    void closeCalendar(bool focusField=true);
    //! openCalendar, or closeCalendar when it is open.
    void toggleCalendar();
    bool isCalendarOpen() const;

    //! The popup and its calendar, nullptr until the first open.
    Popup *getPopup() const { return m_popup; }
    internal::DateEditCalendar *getCalendar() const { return m_calendar; }
    internal::DateEditButton *getButton() const { return m_button; }

    //! Alt+Down opens the calendar; Down alone steps the section.
    bool onKeyDown(bool down) override;
    bool onKeyF4(bool down) override;

    //! The button follows: disabled with the field, and while it is
    //! read-only; either closes an open calendar.
    void setDisabled(bool disabled) override;
    void setEditable(bool editable) override;

protected:

    //! The button disabled while the field is disabled or read-only.
    void updateButton();

    //! A date's formats alone.
    bool formatAllowed(bool date) const override { return date; }
    //! The filter.
    bool acceptsValue(const Value &value) const override;

    void onButtonPressed(event::Info info);
    void onDateChosen(event::Info info);

    std::function<bool(const Date &)> m_filter;
    //! Whether setToday() said which day it is (the calendar's today).
    bool m_todaySet=false;

    internal::DateEditButton *m_button=nullptr;
    //! This control's (Base::createPopup): deleted with it.
    Popup *m_popup=nullptr;
    internal::DateEditCalendar *m_calendar=nullptr;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_dateedit_h_
