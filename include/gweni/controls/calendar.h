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
#include <utility>
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
//! disabled: they take no press, cannot be selected and the cursor passes
//! over them.
//!
//! The views (WinUI's zoom-out): a press on the title shows the year's 12
//! months (View::Year), a second one 12 years (View::Decade: the decade and a
//! year either side, dimmed); a press on a month or a year zooms back in. They
//! reuse the first 12 cells, 4 to a row over the weekday row's and the grid's
//! room, the weekday row and the other 30 cells hidden, so the calendar keeps
//! its size. A month or a year is disabled when no date of it is inside the
//! limits (the filter is not asked).
//!
//! Selection (setSelectionMode): Single, the default, selects the clicked
//! date; Range takes a start on one click and an end on the next (the two
//! swapped if the end is earlier; a click after a whole range starts a new
//! one), and while only the start is set the hovered date (or the keyboard
//! cursor, whichever moved last) previews the range; None takes no selection.
//! A click on a day of the month before or after selects it and shows its
//! month. onSelectionChanged fires once per real change made by the user (a
//! range's start and its end each are one), never from a set*() call.
//!
//! The keyboard: the calendar takes the focus (a click on a cell gives it)
//! and has a cursor date of its own, drawn as a ring while it has the focus.
//! Left/Right move it a day, Up/Down a week, Home/End to the month's first
//! and last day, PageUp/PageDown a month, with Shift a year; a move to another
//! month shows it. A move that lands on a disabled date goes on in its
//! direction (a day at a time) to the first enabled one, and one that would
//! leave the limits does nothing. Space and Return select at the cursor (in
//! Range mode the start, then the end). In the Year view the arrows move a
//! month (Up/Down four), Home/End to January and December and PageUp/Down a
//! year; in the Decade view the same over years and decades; Space and Return
//! zoom in at the cursor.
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

    //! What the grid shows: the month's days, the year's months, or the
    //! decade's years.
    enum class View
    {
        Month,
        Year,
        Decade
    };

    //! The grid: 7 columns, 6 rows.
    static constexpr int Columns=7;
    static constexpr int Rows=6;
    static constexpr int CellCount=Columns*Rows;
    //! The Year and Decade views: 12 cells, 4 to a row.
    static constexpr int ZoomColumns=4;
    static constexpr int ZoomRows=3;
    static constexpr int ZoomCount=ZoomColumns*ZoomRows;

    //! Units: a cell, the header row, the weekday row and the room around and
    //! between them.
    static constexpr int CellWidth=30;
    static constexpr int CellHeight=24;
    static constexpr int HeaderHeight=24;
    static constexpr int WeekdayHeight=18;
    static constexpr int Gap=2;
    static constexpr int Inset=4;
    //! The previous and next buttons' square.
    static constexpr int ButtonSize=20;

    //! The month shown, \p month 1-12 (an invalid month is ignored), in the
    //! Month view or as the Year view's year. The cursor follows into it if it
    //! was not in it. Fires onDisplayedMonthChanged, not onSelectionChanged.
    void setDisplayedMonth(int year, int month);
    int getDisplayedYear() const { return m_year; }
    int getDisplayedMonth() const { return m_month; }
    //! The header's buttons: a month (a year in the Year view, a decade in
    //! the Decade view) back or on, nothing past the limits; true if the
    //! shown month changed.
    bool showPrevious();
    bool showNext();
    //! Whether showPrevious() (showNext()) has somewhere inside the limits to
    //! go: the header's button is disabled when not.
    bool canShowPrevious() const;
    bool canShowNext() const;

    //! The view; setView fires nothing.
    void setView(View view);
    View getView() const { return m_view; }
    //! The title's press: Month to Year to Decade (nothing in Decade).
    void zoomOut();
    //! The first year the Decade view shows (one before the decade).
    int decadeFirstYear() const;

    //! 0 Sunday .. 6 Saturday; Monday (1) by default.
    void setFirstDayOfWeek(int weekday);
    int getFirstDayOfWeek() const { return m_firstDayOfWeek; }

    //! The app's today, ringed in the grid; none by default.
    void setToday(std::optional<Date> today);
    const std::optional<Date> &getToday() const { return m_today; }

    //! The earliest and latest date that can be selected or reached (either
    //! open). A selection outside them stays selected.
    void setDateLimits(std::optional<Date> min, std::optional<Date> max);
    const std::optional<Date> &getMinDate() const { return m_min; }
    const std::optional<Date> &getMaxDate() const { return m_max; }

    //! Dates the filter returns false for are disabled (a weekend filter,
    //! holidays); an empty function enables every date in the limits.
    void setDateFilter(std::function<bool(const Date &)> filter);

    //! Inside the limits and taken by the filter.
    bool isDateEnabled(const Date &date) const;
    //! Inside the limits, with no regard to the filter.
    bool isInLimits(const Date &date) const;
    //! Some date of the month (the year) is inside the limits.
    bool isMonthEnabled(int year, int month) const;
    bool isYearEnabled(int year) const;
    //! \p date, or the limit it is past.
    Date clampToLimits(const Date &date) const;

    //! Switching mode clears the selection of the mode left.
    void setSelectionMode(SelectionMode mode);
    SelectionMode getSelectionMode() const { return m_selectionMode; }

    //! The single selection (Single mode); fires nothing, does not change
    //! the shown month.
    void setSelectedDate(std::optional<Date> date);
    const std::optional<Date> &getSelectedDate() const { return m_selected; }

    //! The range (Range mode): a start alone is a range being picked, whose
    //! next click (or Space) sets its end. Ends given the wrong way round are
    //! swapped; an end with no start is ignored. Fires nothing.
    void setRange(std::optional<Date> start, std::optional<Date> end);
    const std::optional<Date> &getRangeStart() const { return m_rangeStart; }
    const std::optional<Date> &getRangeEnd() const { return m_rangeEnd; }
    std::pair<std::optional<Date>, std::optional<Date>> getRange() const { return {m_rangeStart, m_rangeEnd}; }

    //! The keyboard cursor. setCursor shows its month (in the Month view);
    //! it may be put on a disabled date, which the keys then move off.
    void setCursor(const Date &date);
    const Date &getCursor() const { return m_cursor; }

    //! The names in the header, the weekday row and the Year view: 12 months
    //! from January, 7 weekdays from Sunday (Date::weekday()'s order), 12
    //! short month names. A list of another length is ignored. English by
    //! default.
    void setMonthNames(const std::vector<String> &names);
    void setShortMonthNames(const std::vector<String> &names);
    void setWeekdayNames(const std::vector<String> &names);
    const String &getMonthName(int month) const;
    const String &getShortMonthName(int month) const;
    const String &getWeekdayName(int weekday) const;

    //! The title's text: "November 2026" (Month), "2026" (Year), "2020 –
    //! 2029" (Decade).
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

    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    bool onKeyPageUp(bool down) override;
    bool onKeyPageDown(bool down) override;
    bool onKeySpace(bool down) override;
    bool onKeyReturn(bool down) override;

    void calculateSize(skin::Base *skin, Dim dim) override;

    //! A user's selection changed: info.control the calendar.
    event::Caller onSelectionChanged;
    //! The shown month (or view) changed, by the user or the app.
    event::Caller onDisplayedMonthChanged;

protected:

    void arrange(skin::Base *skin, Dim dim) override;
    void onKeyboardFocus() override;
    void onLostKeyboardFocus() override;

    //! Every shown cell's date, text and look, the title and the weekday row:
    //! each written only where it changed.
    void rebind();
    //! The look of a day cell showing \p date.
    internal::CalendarCellLook dayLook(const Date &date);
    //! The cursor's ring shows: the calendar has the focus.
    bool showsCursor();
    //! The range as drawn: the range, or the start and the preview.
    bool drawnRange(Date &low, Date &high) const;

    void onCellDown(event::Info info);
    void onCellPress(event::Info info);
    void onCellHoverEnter(event::Info info);
    void onCellHoverLeave(event::Info info);
    void onPreviousPress(event::Info info);
    void onNextPress(event::Info info);
    void onTitlePress(event::Info info);

    //! A day clicked or chosen at the cursor: selects it as the mode says,
    //! showing its month, firing onSelectionChanged if it changed.
    void chooseDate(const Date &date);
    //! A month (Year view) or year (Decade view) chosen: zooms in on it.
    void zoomInOn(const Date &date);

    //! The keys: \p step units of the view (days, months, years) from the
    //! cursor, going on past disabled dates in that direction; nothing if
    //! that leaves the limits. \p unit: 0 days, 1 months, 2 years.
    bool moveCursor(int64_t step, int unit);
    //! The first and last date the view would show at \p year / \p month.
    void viewSpan(int year, int month, Date &first, Date &last) const;
    //! The cursor to \p target, or the first enabled date from it going
    //! \p direction (+1 or -1) a day at a time; nothing past the limits.
    bool moveCursorTo(const Date &target, int direction);
    //! The first enabled day of \p year / \p month from \p day (clamped to it), else the last
    //! enabled one before it; \p day itself if the month has none. Never leaves the month.
    Date enabledDayIn(int year, int month, int day) const;
    //! Keeps the cursor on what the view shows and enables: an enabled day of the month
    //! (enabledDayIn), the nearest enabled month of the year, the nearest enabled year of
    //! the decade. Called when the month shown, the view, the limits or the filter change.
    void fitCursor();
    //! The cursor moved: shows its month (year, decade) and rebinds.
    void cursorMoved();
    //! The cursor to what a pressed cell shows: its day, or its month or year
    //! with the cursor's day kept.
    void cursorToCell(const internal::CalendarCell *cell);
    //! PageUp (-1) and PageDown (1), the view's way.
    bool pageCursor(int direction);
    //! A key's press, the view's way; consumes the release.
    bool cursorKey(bool down, int64_t daySteps, int64_t viewSteps);

    //! The shown month changed: binds and fires.
    void displayedMonthChanged();
    //! Places the cells for the view: invalidated when it changes.
    void viewChanged();

    std::array<internal::CalendarCell *, CellCount> m_cells{};
    std::array<Label *, Columns> m_weekdays{};
    internal::CalendarTitle *m_title=nullptr;
    internal::ScrollBarButton *m_previous=nullptr;
    internal::ScrollBarButton *m_next=nullptr;

    int m_year=1970;
    int m_month=1;
    View m_view=View::Month;
    int m_firstDayOfWeek=1;
    std::optional<Date> m_today;
    std::optional<Date> m_min;
    std::optional<Date> m_max;
    std::function<bool(const Date &)> m_filter;
    SelectionMode m_selectionMode=SelectionMode::Single;
    std::optional<Date> m_selected;
    std::optional<Date> m_rangeStart;
    std::optional<Date> m_rangeEnd;
    //! The range's other end while only the start is set: the hovered date
    //! or the cursor.
    std::optional<Date> m_preview;
    Date m_cursor;
    bool m_losingFocus=false;
    std::vector<String> m_monthNames;
    std::vector<String> m_shortMonthNames;
    std::vector<String> m_weekdayNames;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_calendar_h_
