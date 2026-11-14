/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/calendar.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/inputHandler.h>
#include <gweni/skin.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

namespace
{

const std::vector<String> &defaultMonthNames()
{
    static const std::vector<String> names={"January", "February", "March", "April", "May", "June", "July",
        "August", "September", "October", "November", "December"};
    return names;
}

const std::vector<String> &defaultShortMonthNames()
{
    static const std::vector<String> names={"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
        "Nov", "Dec"};
    return names;
}

const std::vector<String> &defaultWeekdayNames()
{
    static const std::vector<String> names={"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    return names;
}

}//namespace

namespace internal
{

///////////////////////////////////////////////////////////////////////////
//CalendarCell

// Owner-placed by its calendar at a fixed size, its number centred; the
// calendar keeps the keyboard focus.
GWENI_CONTROL_CONSTRUCTOR(CalendarCell)
{
    setDock(DockPosition::None);
    setPlacement(Placement::Owner);
    setTextPadding(Padding(0, 0, 0, 0));
    setDefaultTextAlignment(Alignment::Center);
    setKeyboardInputEnabled(false);
    setTabable(false);
}

void CalendarCell::bind(const Date &date, const String &text, int column, int columns,
    const CalendarCellLook &look)
{
    m_date=date;
    // a Text that keeps its string marks nothing
    setText(text);

    const bool repaint=(look != m_look) || (column != m_column) || (columns != m_columns);

    m_column=column;
    m_columns=columns;
    m_look=look;
    if(look.disabled != isDisabled())
        // redraws it
        setDisabled(look.disabled);
    else if(repaint)
        redraw();
}

void CalendarCell::updateColors()
{
    const auto &colors=getSkin()->Colors.Calendar;

    if(m_look.disabled)
        setTextColor(colors.Disabled);
    else if(m_look.selected)
        setTextColor(colors.Selected);
    else if(!m_look.inMonth)
        setTextColor(colors.OtherMonth);
    else if(m_look.today)
        setTextColor(colors.Today);
    else
        setTextColor(colors.Day);
}

///////////////////////////////////////////////////////////////////////////
//CalendarTitle

GWENI_CONTROL_CONSTRUCTOR(CalendarTitle)
{
    setDock(DockPosition::None);
    setPlacement(Placement::Owner);
    setTextPadding(Padding(0, 0, 0, 0));
    setDefaultTextAlignment(Alignment::Center);
    setKeyboardInputEnabled(false);
    setTabable(false);
}

void CalendarTitle::updateColors()
{
    setTextColor(getSkin()->Colors.Calendar.Title);
}

}//namespace internal

///////////////////////////////////////////////////////////////////////////
//Calendar

// The parts are made once, in the order they are drawn: the header, the
// weekday row, then the 42 cells. Each is owner-placed at a size fixed here.
GWENI_CONTROL_CONSTRUCTOR(Calendar)
{
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setPadding(Padding(Inset, Inset, Inset, Inset));
    setKeyboardInputEnabled(true);
    setTabable(true);

    m_monthNames=defaultMonthNames();
    m_shortMonthNames=defaultShortMonthNames();
    m_weekdayNames=defaultWeekdayNames();
    m_cursor=Date(m_year, m_month, 1);

    m_previous=newChild<internal::ScrollBarButton>();
    m_previous->setDirectionLeft();
    m_next=newChild<internal::ScrollBarButton>();
    m_next->setDirectionRight();
    for(internal::ScrollBarButton *button : {m_previous, m_next})
    {
        button->setSize(ButtonSize, ButtonSize);
        button->setPlacement(Placement::Owner);
        button->setKeyboardInputEnabled(false);
        button->setTabable(false);
    }
    m_previous->onPressCaller.add(this, &Calendar::onPreviousPress);
    m_next->onPressCaller.add(this, &Calendar::onNextPress);

    m_title=newChild<internal::CalendarTitle>();
    m_title->updateColors();
    m_title->onPressCaller.add(this, &Calendar::onTitlePress);

    for(Label *&label : m_weekdays)
    {
        label=newChild<Label>();
        label->setDock(DockPosition::None);
        label->setPlacement(Placement::Owner);
        label->setTextPadding(Padding(0, 0, 0, 0));
        label->setTextAlignment(Alignment::Center);
        label->setMouseInputEnabled(false);
    }

    for(internal::CalendarCell *&cell : m_cells)
    {
        cell=newChild<internal::CalendarCell>();
        cell->m_calendar=this;
        cell->onDownCaller.add(this, &Calendar::onCellDown);
        cell->onPressCaller.add(this, &Calendar::onCellPress);
        cell->onHoverEnter.add(this, &Calendar::onCellHoverEnter);
        cell->onHoverLeave.add(this, &Calendar::onCellHoverLeave);
    }

    updateColors();
    rebind();
}

// The cells are deleted by Base's destructor, after the calendar's own part.
Calendar::~Calendar()
{
    for(internal::CalendarCell *cell : m_cells)
    {
        if(cell)
            cell->m_calendar=nullptr;
    }
}

void Calendar::setDisplayedMonth(int year, int month)
{
    if((month < 1) || (month > 12) || ((year == m_year) && (month == m_month)))
        return;

    m_year=year;
    m_month=month;
    // the cursor follows into the month shown, to its day there (the first
    // enabled one from it, else the last before it)
    if((m_cursor.year != year) || (m_cursor.month != month))
    {
        const Date target(year, month, std::min(m_cursor.day, Date::daysInMonth(year, month)));
        Date found=target;

        for(int direction : {1, -1})
        {
            bool done=false;

            for(Date date=target; (date.year == year) && (date.month == month); date=date.addDays(direction))
            {
                if(isDateEnabled(date))
                {
                    found=date;
                    done=true;
                    break;
                }
            }
            if(done)
                break;
        }
        m_cursor=found;
    }
    displayedMonthChanged();
}

void Calendar::displayedMonthChanged()
{
    rebind();
    onDisplayedMonthChanged.call(this);
}

int Calendar::decadeFirstYear() const
{
    const int decade=(m_year >= 0)?(m_year/10)*10:-(((-m_year)+9)/10)*10;

    return decade-1;
}

bool Calendar::isInLimits(const Date &date) const
{
    return !(m_min && (date < *m_min)) && !(m_max && (date > *m_max));
}

bool Calendar::isDateEnabled(const Date &date) const
{
    return isInLimits(date) && (!m_filter || m_filter(date));
}

bool Calendar::isMonthEnabled(int year, int month) const
{
    const Date first(year, month, 1);

    return !(m_max && (first > *m_max)) && !(m_min && (first.lastOfMonth() < *m_min));
}

bool Calendar::isYearEnabled(int year) const
{
    return !(m_max && (Date(year, 1, 1) > *m_max)) && !(m_min && (Date(year, 12, 31) < *m_min));
}

bool Calendar::canShowPrevious() const
{
    switch(m_view)
    {
    case View::Month:
        return !m_min || !(Date(m_year, m_month, 1).addDays(-1) < *m_min);
    case View::Year:
        return isYearEnabled(m_year-1);
    case View::Decade:
        return isYearEnabled(decadeFirstYear());
    }
    return false;
}

bool Calendar::canShowNext() const
{
    switch(m_view)
    {
    case View::Month:
        return !m_max || !(Date(m_year, m_month, 1).addMonths(1) > *m_max);
    case View::Year:
        return isYearEnabled(m_year+1);
    case View::Decade:
        return isYearEnabled(decadeFirstYear()+ZoomCount-1);
    }
    return false;
}

bool Calendar::showPrevious()
{
    if(!canShowPrevious())
        return false;

    const int months=(m_view == View::Month)?1:(m_view == View::Year)?12:120;
    const Date previous=Date(m_year, m_month, 1).addMonths(-months);

    setDisplayedMonth(previous.year, previous.month);
    return true;
}

bool Calendar::showNext()
{
    if(!canShowNext())
        return false;

    const int months=(m_view == View::Month)?1:(m_view == View::Year)?12:120;
    const Date next=Date(m_year, m_month, 1).addMonths(months);

    setDisplayedMonth(next.year, next.month);
    return true;
}

void Calendar::setView(View view)
{
    if(view == m_view)
        return;

    m_view=view;
    m_preview.reset();
    viewChanged();
}

// The views place the cells apart: the weekday row and the cells past the
// twelfth hidden in the zoomed-out ones, the first twelve placed 4 to a row.
void Calendar::viewChanged()
{
    const bool month=(m_view == View::Month);

    for(Label *label : m_weekdays)
        label->setHidden(!month);
    for(int i=ZoomCount; i < CellCount; ++i)
        m_cells[static_cast<size_t>(i)]->setHidden(!month);
    invalidate();
    rebind();
}

void Calendar::zoomOut()
{
    if(m_view == View::Month)
        setView(View::Year);
    else if(m_view == View::Year)
        setView(View::Decade);
    else
        return;
    onDisplayedMonthChanged.call(this);
}

void Calendar::zoomInOn(const Date &date)
{
    if(m_view == View::Decade)
    {
        if(!isYearEnabled(date.year))
            return;
        m_view=View::Year;
        m_cursor=Date(date.year, m_cursor.month, std::min(m_cursor.day, Date::daysInMonth(date.year, m_cursor.month)));
        m_year=date.year;
    }
    else if(m_view == View::Year)
    {
        if(!isMonthEnabled(date.year, date.month))
            return;
        m_view=View::Month;
        m_year=date.year;
        m_month=date.month;
        m_cursor=Date(date.year, date.month, std::min(m_cursor.day, Date::daysInMonth(date.year, date.month)));
        // onto an enabled day of that month, if it has one
        if(!isDateEnabled(m_cursor))
        {
            const Date from=m_cursor;

            if(!moveCursorTo(from, 1) && !moveCursorTo(from, -1))
                m_cursor=from;
        }
    }
    else
        return;

    m_preview.reset();
    viewChanged();
    onDisplayedMonthChanged.call(this);
}

void Calendar::setFirstDayOfWeek(int weekday)
{
    if((weekday < 0) || (weekday > 6) || (weekday == m_firstDayOfWeek))
        return;

    m_firstDayOfWeek=weekday;
    rebind();
}

void Calendar::setToday(std::optional<Date> today)
{
    if(today == m_today)
        return;

    m_today=today;
    rebind();
}

void Calendar::setDateLimits(std::optional<Date> min, std::optional<Date> max)
{
    if((min == m_min) && (max == m_max))
        return;

    m_min=min;
    m_max=max;
    rebind();
}

void Calendar::setDateFilter(std::function<bool(const Date &)> filter)
{
    m_filter=std::move(filter);
    rebind();
}

void Calendar::setSelectionMode(SelectionMode mode)
{
    if(mode == m_selectionMode)
        return;

    m_selectionMode=mode;
    m_selected.reset();
    m_rangeStart.reset();
    m_rangeEnd.reset();
    m_preview.reset();
    rebind();
}

void Calendar::setSelectedDate(std::optional<Date> date)
{
    if(date && !date->isValid())
        return;
    if(date == m_selected)
        return;

    m_selected=date;
    rebind();
}

void Calendar::setRange(std::optional<Date> start, std::optional<Date> end)
{
    if((start && !start->isValid()) || (end && !end->isValid()))
        return;
    if(!start)
        end.reset();
    if(start && end && (*end < *start))
        std::swap(start, end);
    if((start == m_rangeStart) && (end == m_rangeEnd))
        return;

    m_rangeStart=start;
    m_rangeEnd=end;
    m_preview.reset();
    rebind();
}

void Calendar::setCursor(const Date &date)
{
    if(!date.isValid())
        return;

    m_cursor=date;
    cursorMoved();
}

void Calendar::setMonthNames(const std::vector<String> &names)
{
    if((names.size() != 12) || (names == m_monthNames))
        return;

    m_monthNames=names;
    rebind();
}

void Calendar::setShortMonthNames(const std::vector<String> &names)
{
    if((names.size() != 12) || (names == m_shortMonthNames))
        return;

    m_shortMonthNames=names;
    rebind();
}

void Calendar::setWeekdayNames(const std::vector<String> &names)
{
    if((names.size() != 7) || (names == m_weekdayNames))
        return;

    m_weekdayNames=names;
    rebind();
}

const String &Calendar::getMonthName(int month) const
{
    static const String none;

    if((month < 1) || (month > 12))
        return none;
    return m_monthNames[static_cast<size_t>(month-1)];
}

const String &Calendar::getShortMonthName(int month) const
{
    static const String none;

    if((month < 1) || (month > 12))
        return none;
    return m_shortMonthNames[static_cast<size_t>(month-1)];
}

const String &Calendar::getWeekdayName(int weekday) const
{
    static const String none;

    if((weekday < 0) || (weekday > 6))
        return none;
    return m_weekdayNames[static_cast<size_t>(weekday)];
}

String Calendar::titleText() const
{
    switch(m_view)
    {
    case View::Month:
        return getMonthName(m_month)+" "+std::to_string(m_year);
    case View::Year:
        return std::to_string(m_year);
    case View::Decade:
        // the decade, without the year either side; an en dash between
        return std::to_string(decadeFirstYear()+1)+" \xe2\x80\x93 "+std::to_string(decadeFirstYear()+10);
    }
    return String();
}

Date Calendar::cellDate(int index) const
{
    const Date first(m_year, m_month, 1);
    const int lead=(first.weekday()-m_firstDayOfWeek+7)%7;

    return first.addDays(index-lead);
}

int Calendar::cellIndex(const Date &date) const
{
    const int64_t index=date.toDayNumber()-cellDate(0).toDayNumber();

    if((index < 0) || (index >= CellCount))
        return -1;
    return static_cast<int>(index);
}

internal::CalendarCell *Calendar::getCell(int index) const
{
    if((index < 0) || (index >= CellCount))
        return nullptr;
    return m_cells[static_cast<size_t>(index)];
}

Label *Calendar::getWeekdayLabel(int column) const
{
    if((column < 0) || (column >= Columns))
        return nullptr;
    return m_weekdays[static_cast<size_t>(column)];
}

bool Calendar::drawnRange(Date &low, Date &high) const
{
    if((m_selectionMode != SelectionMode::Range) || !m_rangeStart)
        return false;

    low=*m_rangeStart;
    high=m_rangeEnd?*m_rangeEnd:(m_preview?*m_preview:low);
    if(high < low)
        std::swap(low, high);
    return true;
}

internal::CalendarCellLook Calendar::dayLook(const Date &date)
{
    internal::CalendarCellLook look;
    Date low, high;

    look.inMonth=(date.year == m_year) && (date.month == m_month);
    look.today=m_today && (date == *m_today);
    look.selected=(m_selectionMode == SelectionMode::Single) && m_selected && (date == *m_selected);
    look.disabled=!isDateEnabled(date);
    look.cursor=showsCursor() && (date == m_cursor);
    if(drawnRange(low, high))
    {
        look.inRange=(low != high) && (date >= low) && (date <= high);
        look.rangeStart=(date == low);
        look.rangeEnd=(date == high);
    }
    return look;
}

void Calendar::rebind()
{
    m_title->setText(titleText());

    if(m_view == View::Month)
    {
        for(int column=0; column < Columns; ++column)
            m_weekdays[static_cast<size_t>(column)]->setText(getWeekdayName((m_firstDayOfWeek+column)%7));

        for(int i=0; i < CellCount; ++i)
        {
            const Date date=cellDate(i);

            m_cells[static_cast<size_t>(i)]->bind(date, std::to_string(date.day), i%Columns, Columns,
                dayLook(date));
        }
    }
    else
    {
        // the selection, today and the cursor, as a month or a year
        const bool year=(m_view == View::Year);
        auto same=[year](const Date &a, const Date &b)
        {
            return (a.year == b.year) && (!year || (a.month == b.month));
        };
        const bool focused=showsCursor();
        Date low, high;
        const bool range=drawnRange(low, high);

        for(int i=0; i < ZoomCount; ++i)
        {
            const Date date=year?Date(m_year, i+1, 1):Date(decadeFirstYear()+i, 1, 1);
            internal::CalendarCellLook look;

            look.inMonth=year || ((i > 0) && (i < ZoomCount-1));
            look.today=m_today && same(*m_today, date);
            look.selected=((m_selectionMode == SelectionMode::Single) && m_selected && same(*m_selected, date))
                || (range && (same(low, date) || same(high, date)));
            look.disabled=year?!isMonthEnabled(date.year, date.month):!isYearEnabled(date.year);
            look.cursor=focused && same(m_cursor, date);
            m_cells[static_cast<size_t>(i)]->bind(date, year?getShortMonthName(i+1):std::to_string(date.year),
                i%ZoomColumns, ZoomColumns, look);
        }
    }

    m_previous->setDisabled(!canShowPrevious());
    m_next->setDisabled(!canShowNext());
}

void Calendar::chooseDate(const Date &date)
{
    if(!isDateEnabled(date) || (m_selectionMode == SelectionMode::None))
    {
        if(isDateEnabled(date))
        {
            // None: the cursor and the month follow, nothing is selected
            m_cursor=date;
            cursorMoved();
        }
        return;
    }

    bool changed=false;

    if(m_selectionMode == SelectionMode::Single)
    {
        changed=(m_selected != date);
        m_selected=date;
    }
    else
    {
        if(!m_rangeStart || m_rangeEnd)
        {
            m_rangeStart=date;
            m_rangeEnd.reset();
        }
        else if(date < *m_rangeStart)
        {
            m_rangeEnd=m_rangeStart;
            m_rangeStart=date;
        }
        else
            m_rangeEnd=date;
        m_preview.reset();
        changed=true;
    }

    m_cursor=date;
    cursorMoved();
    if(changed)
        onSelectionChanged.call(this);
}

void Calendar::cursorMoved()
{
    const bool month=(m_view == View::Month);
    bool shown;

    if(m_view == View::Month)
        shown=(m_cursor.year == m_year) && (m_cursor.month == m_month);
    else if(m_view == View::Year)
        shown=(m_cursor.year == m_year);
    else
        shown=(m_cursor.year > decadeFirstYear()) && (m_cursor.year < decadeFirstYear()+ZoomCount-1);

    // a range being picked previews to the cursor once the keys move it
    if((m_selectionMode == SelectionMode::Range) && m_rangeStart && !m_rangeEnd && month)
        m_preview=m_cursor;

    if(!shown)
    {
        m_year=m_cursor.year;
        if(month)
            m_month=m_cursor.month;
        displayedMonthChanged();
        return;
    }
    rebind();
}

bool Calendar::moveCursorTo(const Date &target, int direction)
{
    // a year of disabled dates in a row is as far as it looks
    Date date=target;

    for(int tries=0; tries < 400; ++tries)
    {
        if(!isInLimits(date))
            return false;
        if(isDateEnabled(date))
        {
            m_cursor=date;
            return true;
        }
        date=date.addDays(direction);
    }
    return false;
}

bool Calendar::moveCursor(int64_t step, int unit)
{
    const Date from=m_cursor;
    Date target=from;

    if(unit == 0)
        target=from.addDays(step);
    else if(unit == 1)
        target=from.addMonths(step);
    else
        target=from.addYears(step);

    const int direction=(step < 0)?-1:1;

    if(m_view == View::Month)
    {
        if(!moveCursorTo(target, direction))
            return false;
    }
    else
    {
        // the zoomed-out views move over months and years, inside the limits
        const bool enabled=(m_view == View::Year)?isMonthEnabled(target.year, target.month)
            :isYearEnabled(target.year);

        if(!enabled)
            return false;
        m_cursor=target;
    }
    if(m_cursor != from)
        cursorMoved();
    return true;
}

bool Calendar::cursorKey(bool down, int64_t daySteps, int64_t viewSteps)
{
    if(!down)
        return true;

    if(m_view == View::Month)
        moveCursor(daySteps, 0);
    else
        moveCursor(viewSteps, (m_view == View::Year)?1:2);
    return true;
}

bool Calendar::onKeyLeft(bool down)
{
    return cursorKey(down, -1, -1);
}

bool Calendar::onKeyRight(bool down)
{
    return cursorKey(down, 1, 1);
}

bool Calendar::onKeyUp(bool down)
{
    return cursorKey(down, -7, -ZoomColumns);
}

bool Calendar::onKeyDown(bool down)
{
    return cursorKey(down, 7, ZoomColumns);
}

bool Calendar::onKeyHome(bool down)
{
    if(!down)
        return true;

    Date target=m_cursor;

    if(m_view == View::Month)
        target=m_cursor.firstOfMonth();
    else if(m_view == View::Year)
        target=Date(m_cursor.year, 1, std::min(m_cursor.day, 31));
    else
        target=Date(decadeFirstYear()+1, m_cursor.month, std::min(m_cursor.day,
            Date::daysInMonth(decadeFirstYear()+1, m_cursor.month)));

    if(m_view == View::Month)
    {
        // the first enabled day from the first (or the minimum), inside the
        // month
        const Date from=m_cursor;

        if(m_min && (target < *m_min))
            target=*m_min;
        if(moveCursorTo(target, 1) && ((m_cursor.month != from.month) || (m_cursor.year != from.year)))
            m_cursor=from;
    }
    else if((m_view == View::Year)?isMonthEnabled(target.year, target.month):isYearEnabled(target.year))
        m_cursor=target;
    cursorMoved();
    return true;
}

bool Calendar::onKeyend(bool down)
{
    if(!down)
        return true;

    Date target=m_cursor;

    if(m_view == View::Month)
        target=m_cursor.lastOfMonth();
    else if(m_view == View::Year)
        target=Date(m_cursor.year, 12, std::min(m_cursor.day, 31));
    else
        target=Date(decadeFirstYear()+10, m_cursor.month, std::min(m_cursor.day,
            Date::daysInMonth(decadeFirstYear()+10, m_cursor.month)));

    if(m_view == View::Month)
    {
        // the last enabled day back from the last (or the maximum)
        const Date from=m_cursor;

        if(m_max && (target > *m_max))
            target=*m_max;
        if(moveCursorTo(target, -1) && ((m_cursor.month != from.month) || (m_cursor.year != from.year)))
            m_cursor=from;
    }
    else if((m_view == View::Year)?isMonthEnabled(target.year, target.month):isYearEnabled(target.year))
        m_cursor=target;
    cursorMoved();
    return true;
}

bool Calendar::onKeyPageUp(bool down)
{
    if(!down)
        return true;

    const bool year=input::isShiftDown();

    if(m_view == View::Month)
    {
        // a month (a year) back, the day clamped, then off a disabled date
        // going on back
        const Date target=year?m_cursor.addYears(-1):m_cursor.addMonths(-1);
        const Date from=m_cursor;

        if(moveCursorTo(target, -1) && (m_cursor != from))
            cursorMoved();
    }
    else
        moveCursor((m_view == View::Year)?-12:-120, 1);
    return true;
}

bool Calendar::onKeyPageDown(bool down)
{
    if(!down)
        return true;

    const bool year=input::isShiftDown();

    if(m_view == View::Month)
    {
        const Date target=year?m_cursor.addYears(1):m_cursor.addMonths(1);
        const Date from=m_cursor;

        if(moveCursorTo(target, 1) && (m_cursor != from))
            cursorMoved();
    }
    else
        moveCursor((m_view == View::Year)?12:120, 1);
    return true;
}

bool Calendar::onKeySpace(bool down)
{
    if(!down)
        return true;

    if(m_view == View::Month)
        chooseDate(m_cursor);
    else
        zoomInOn(m_cursor);
    return true;
}

bool Calendar::onKeyReturn(bool down)
{
    return onKeySpace(down);
}

void Calendar::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();
    rebind();
}

// Called while KeyboardFocus is still this calendar (Base::focus() tells the
// old holder first), so the cursor's ring is taken off by hand.
void Calendar::onLostKeyboardFocus()
{
    ParentClass::onLostKeyboardFocus();
    m_losingFocus=true;
    rebind();
    m_losingFocus=false;
}

bool Calendar::showsCursor()
{
    return isFocussed() && !m_losingFocus;
}

void Calendar::onCellDown(event::Info info)
{
    const internal::CalendarCell *cell=static_cast<const internal::CalendarCell *>(info.controlCaller);

    if(!cell || (cell->m_calendar != this))
        return;
    focus();
}

void Calendar::onCellPress(event::Info info)
{
    const internal::CalendarCell *cell=static_cast<const internal::CalendarCell *>(info.controlCaller);

    if(!cell || (cell->m_calendar != this))
        return;
    if(m_view == View::Month)
        chooseDate(cell->getDate());
    else
        zoomInOn(cell->getDate());
}

// While a range has its start alone, the hovered day is its other end.
void Calendar::onCellHoverEnter(event::Info info)
{
    const internal::CalendarCell *cell=static_cast<const internal::CalendarCell *>(info.controlCaller);

    if(!cell || (cell->m_calendar != this) || (m_view != View::Month) || (m_selectionMode != SelectionMode::Range)
        || !m_rangeStart || m_rangeEnd || cell->getLook().disabled)
        return;
    if(m_preview == cell->getDate())
        return;

    m_preview=cell->getDate();
    rebind();
}

void Calendar::onCellHoverLeave(event::Info info)
{
    const internal::CalendarCell *cell=static_cast<const internal::CalendarCell *>(info.controlCaller);

    if(!cell || (cell->m_calendar != this) || !m_preview || (*m_preview != cell->getDate()))
        return;

    m_preview.reset();
    rebind();
}

void Calendar::onPreviousPress(event::Info)
{
    showPrevious();
}

void Calendar::onNextPress(event::Info)
{
    showNext();
}

void Calendar::onTitlePress(event::Info)
{
    zoomOut();
}

void Calendar::updateColors()
{
    if(!getSkin())
        return;

    const auto &colors=getSkin()->Colors.Calendar;

    for(Label *label : m_weekdays)
    {
        if(label)
            label->setTextColor(colors.Weekday);
    }
}

// A fixed size: the header, the weekday row and the grid, whatever the month
// or the view.
void Calendar::calculateSize(skin::Base *skin, Dim dim)
{
    // the parts are measured for their own subtrees; none counts
    ParentClass::calculateSize(skin, dim);

    const Padding &padding=getPadding();

    if(dim == Dim::X)
        m_preferredSize.width=preferredFromContent(Columns*CellWidth+padding.left+padding.right, Dim::X);
    else
        m_preferredSize.height=preferredFromContent(HeaderHeight+Gap+WeekdayHeight+Rows*CellHeight
            +padding.top+padding.bottom, Dim::Y);
}

// The parts are placed before Base's arrange enters them (SegmentedButtons'
// pattern): from its changed-children walk on, Base's arrange enters only the
// children marked, and a part given new bounds here is. The zoomed-out views
// place the first 12 cells 4 to a row over the weekday row's and the grid's
// room; the hidden parts keep their bounds.
void Calendar::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();
    const int width=Columns*CellWidth;
    const int height=WeekdayHeight+Rows*CellHeight;
    const int buttonY=padding.top+(HeaderHeight-ButtonSize)/2;
    const int weekdayY=padding.top+HeaderHeight+Gap;
    const int gridY=weekdayY+WeekdayHeight;
    const bool month=(m_view == View::Month);

    if(dim == Dim::X)
    {
        placeChild(m_previous, padding.left, m_previous->getY(), ButtonSize, ButtonSize);
        placeChild(m_next, padding.left+width-ButtonSize, m_next->getY(), ButtonSize, ButtonSize);
        placeChild(m_title, padding.left+ButtonSize, m_title->getY(), width-2*ButtonSize, m_title->getHeight());
        for(int column=0; column < Columns; ++column)
        {
            Label *label=m_weekdays[static_cast<size_t>(column)];

            placeChild(label, padding.left+column*CellWidth, label->getY(), CellWidth, label->getHeight());
        }
        for(int i=0; i < CellCount; ++i)
        {
            internal::CalendarCell *cell=m_cells[static_cast<size_t>(i)];

            if(month)
                placeChild(cell, padding.left+(i%Columns)*CellWidth, cell->getY(), CellWidth, cell->getHeight());
            else if(i < ZoomCount)
            {
                const int column=i%ZoomColumns;
                const int x=column*width/ZoomColumns;

                placeChild(cell, padding.left+x, cell->getY(), (column+1)*width/ZoomColumns-x, cell->getHeight());
            }
        }
    }
    else
    {
        placeChild(m_previous, m_previous->getX(), buttonY, ButtonSize, ButtonSize);
        placeChild(m_next, m_next->getX(), buttonY, ButtonSize, ButtonSize);
        placeChild(m_title, m_title->getX(), padding.top, m_title->getWidth(), HeaderHeight);
        for(Label *label : m_weekdays)
            placeChild(label, label->getX(), weekdayY, label->getWidth(), WeekdayHeight);
        for(int i=0; i < CellCount; ++i)
        {
            internal::CalendarCell *cell=m_cells[static_cast<size_t>(i)];

            if(month)
                placeChild(cell, cell->getX(), gridY+(i/Columns)*CellHeight, cell->getWidth(), CellHeight);
            else if(i < ZoomCount)
            {
                const int row=i/ZoomColumns;
                const int y=row*height/ZoomRows;

                placeChild(cell, cell->getX(), weekdayY+y, cell->getWidth(), (row+1)*height/ZoomRows-y);
            }
        }
    }

    ParentClass::arrange(skin, dim);
}

}//namespace controls
}//namespace gweni
