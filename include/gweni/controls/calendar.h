/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_calendar_h_
#define _gweni_controls_calendar_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/date.h>

#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace gweni
{
namespace controls
{

class Calendar;
class Label;

namespace internal
{

class ScrollBarButton;

//! How a calendar cell looks: what the calendar has bound it to show. Each
//! flag is the calendar's to set (Calendar::rebind); the cell repaints only
//! when its look changes.
struct GWENI_EXPORT CalendarCellLook
{
    //! A day of the shown month (false: the month before or after, dimmed).
    bool inMonth=true;
    //! The app's today (Calendar::setToday), drawn with a ring.
    bool today=false;
    //! The single selection, or either end of the range.
    bool selected=false;
    //! Inside the range (its ends included), joined with its neighbours.
    bool inRange=false;
    //! The range's start and end (both on a one-day range).
    bool rangeStart=false;
    bool rangeEnd=false;
    //! Outside the limits or refused by the filter: no press, no hover.
    bool disabled=false;
    //! The keyboard cursor, while the calendar has the focus.
    bool cursor=false;

    bool operator==(const CalendarCellLook &other) const
    {
        return (inMonth == other.inMonth) && (today == other.today) && (selected == other.selected)
            && (inRange == other.inRange) && (rangeStart == other.rangeStart) && (rangeEnd == other.rangeEnd)
            && (disabled == other.disabled) && (cursor == other.cursor);
    }
    bool operator!=(const CalendarCellLook &other) const { return !(*this == other); }
};

//! One cell of a Calendar's grid: a Button whose Text is the day's number (a
//! month's name, a year, in the zoomed-out views), owner-placed by its
//! calendar. It takes no keyboard focus of its own (the calendar has it); a
//! press goes to the calendar. Its date, column and look are set by the
//! calendar, each write only when it changes, so rebinding a cell that keeps
//! them marks nothing.
class GWENI_EXPORT CalendarCell: public Button
{
public:

    GWENI_CONTROL(CalendarCell, Button);

public:

    Calendar *getCalendar() const { return m_calendar; }

    //! The date it shows: a day, the first of a month or of a year.
    const Date &getDate() const { return m_date; }
    //! Its column in the grid (0..6 in the month view, the row of seven is
    //! where a range's look is rounded).
    int getColumn() const { return m_column; }
    //! The columns of its row (7 in the month view): the last one rounds a
    //! range's right side.
    int getColumns() const { return m_columns; }
    const CalendarCellLook &getLook() const { return m_look; }

    //! The skin's Calendar text colour for its look.
    void updateColors() override;

protected:

    friend class gweni::controls::Calendar;

    //! Each a write only when it changes; a new look or column repaints it.
    void bind(const Date &date, const String &text, int column, int columns, const CalendarCellLook &look);

    Calendar *m_calendar=nullptr;
    Date m_date;
    int m_column=0;
    int m_columns=7;
    CalendarCellLook m_look;
};

//! A Calendar's title, "November 2026": a flat button (nothing drawn at
//! rest) whose press zooms the calendar out.
class GWENI_EXPORT CalendarTitle: public Button
{
public:

    GWENI_CONTROL(CalendarTitle, Button);

public:

    //! The skin's Calendar title colour, in every state.
    void updateColors() override;
};

}//namespace internal

//! A month calendar (QCalendarWidget, WinUI CalendarView, Material's date
//! picker) over gweni::Date: a header of previous and next buttons around the
//! title, a row of weekday names starting at getFirstDayOfWeek() and a grid of
//! 7x6 day cells, a fixed 42, always made and owner-placed, so its size never
//! depends on the month. Showing another month (setDisplayedMonth, the
//! buttons) binds each cell to its new day and look, making and deleting no
//! control: a cell whose number and look stay writes nothing.
//!
//! The grid's first cell is the first day of the week on or before the
//! month's first day, so the month starts in the first row and the days of the
//! months either side fill the rest, dimmed. The first day of the week is
//! Monday (1, ISO 8601's) unless setFirstDayOfWeek says otherwise (0 Sunday ..
//! 6 Saturday, as Date::weekday()).
//!
//! "Today" is the app's (setToday): nothing here reads a clock or a time
//! zone. Dates outside setDateLimits(), or that setDateFilter() refuses, are
//! disabled: they take no press and cannot be selected.
//!
//! Selection (setSelectionMode): Single, the default, selects the clicked
//! date; None takes no selection. A click on a day of the month before or
//! after selects it and shows its month. onSelectionChanged fires once per
//! real change made by the user, never from a set*() call.
class GWENI_EXPORT Calendar: public Base
{
public:

    GWENI_CONTROL(Calendar, Base);

public:

    ~Calendar() override;

    enum class SelectionMode
    {
        None,
        Single,
        Range
    };

    //! The grid: 7 columns, 6 rows.
    static constexpr int Columns=7;
    static constexpr int Rows=6;
    static constexpr int CellCount=Columns*Rows;

    //! Units: a cell, the header row, the weekday row and the room around and
    //! between them.
    static constexpr int CellWidth=32;
    static constexpr int CellHeight=28;
    static constexpr int HeaderHeight=28;
    static constexpr int WeekdayHeight=20;
    static constexpr int Gap=4;
    static constexpr int Inset=6;
    //! The previous and next buttons' square.
    static constexpr int ButtonSize=24;

    //! The month shown, \p month 1-12 (an invalid month is ignored). Binds
    //! the cells; fires nothing.
    void setDisplayedMonth(int year, int month);
    int getDisplayedYear() const { return m_year; }
    int getDisplayedMonth() const { return m_month; }
    //! The month before or after the shown one, as the header's buttons do
    //! (nothing past the limits); true if the shown month changed.
    bool showPreviousMonth();
    bool showNextMonth();
    //! Whether a month before (after) the shown one has a date inside the
    //! limits: the header's button is disabled when not.
    bool canShowPreviousMonth() const;
    bool canShowNextMonth() const;

    //! 0 Sunday .. 6 Saturday; Monday (1) by default.
    void setFirstDayOfWeek(int weekday);
    int getFirstDayOfWeek() const { return m_firstDayOfWeek; }

    //! The app's today, ringed in the grid; none by default.
    void setToday(std::optional<Date> today);
    const std::optional<Date> &getToday() const { return m_today; }

    //! The earliest and latest date that can be selected or shown as enabled
    //! (either open). A selection outside them stays selected.
    void setDateLimits(std::optional<Date> min, std::optional<Date> max);
    const std::optional<Date> &getMinDate() const { return m_min; }
    const std::optional<Date> &getMaxDate() const { return m_max; }

    //! Dates the filter returns false for are disabled (a weekend filter,
    //! holidays); an empty function enables every date in the limits.
    void setDateFilter(std::function<bool(const Date &)> filter);

    //! Inside the limits and taken by the filter.
    bool isDateEnabled(const Date &date) const;

    void setSelectionMode(SelectionMode mode);
    SelectionMode getSelectionMode() const { return m_selectionMode; }

    //! The single selection (Single mode); fires nothing, does not change
    //! the shown month.
    void setSelectedDate(std::optional<Date> date);
    const std::optional<Date> &getSelectedDate() const { return m_selected; }

    //! The names in the header and the weekday row: 12 months from January,
    //! 7 weekdays from Sunday (Date::weekday()'s order). A list of another
    //! length is ignored. English by default.
    void setMonthNames(const std::vector<String> &names);
    void setWeekdayNames(const std::vector<String> &names);
    const String &getMonthName(int month) const;
    const String &getWeekdayName(int weekday) const;

    //! The title's text for the shown month: "November 2026".
    String titleText() const;

    //! The date cell \p index (0..41, row by row) shows in the month view.
    Date cellDate(int index) const;
    //! The index of the cell showing \p date in the month view, -1 if none.
    int cellIndex(const Date &date) const;

    //! The parts, for tests and the skin.
    internal::CalendarCell *getCell(int index) const;
    internal::CalendarTitle *getTitle() const { return m_title; }
    internal::ScrollBarButton *getPreviousButton() const { return m_previous; }
    internal::ScrollBarButton *getNextButton() const { return m_next; }
    //! The weekday row's label at column \p column.
    Label *getWeekdayLabel(int column) const;

    //! The weekday labels' colour.
    void updateColors() override;

    void calculateSize(skin::Base *skin, Dim dim) override;

    //! A user's selection changed: info.control the calendar.
    event::Caller onSelectionChanged;
    //! The shown month changed (by the user or the app).
    event::Caller onDisplayedMonthChanged;

protected:

    void arrange(skin::Base *skin, Dim dim) override;

    //! Every cell's date, text and look, the title and the weekday row: each
    //! written only where it changed.
    void rebind();
    //! The look of a day cell showing \p date.
    internal::CalendarCellLook dayLook(const Date &date) const;

    void onCellPress(event::Info info);
    void onPreviousPress(event::Info info);
    void onNextPress(event::Info info);

    //! A cell clicked or chosen in the month view: selects it as the mode
    //! says, showing its month, firing onSelectionChanged if it changed.
    void chooseDate(const Date &date);

    //! The shown month changed: binds and fires.
    void displayedMonthChanged();

    std::array<internal::CalendarCell *, CellCount> m_cells{};
    std::array<Label *, Columns> m_weekdays{};
    internal::CalendarTitle *m_title=nullptr;
    internal::ScrollBarButton *m_previous=nullptr;
    internal::ScrollBarButton *m_next=nullptr;

    int m_year=1970;
    int m_month=1;
    int m_firstDayOfWeek=1;
    std::optional<Date> m_today;
    std::optional<Date> m_min;
    std::optional<Date> m_max;
    std::function<bool(const Date &)> m_filter;
    SelectionMode m_selectionMode=SelectionMode::Single;
    std::optional<Date> m_selected;
    std::vector<String> m_monthNames;
    std::vector<String> m_weekdayNames;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_calendar_h_
