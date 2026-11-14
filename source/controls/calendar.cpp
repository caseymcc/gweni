/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/calendar.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollBarButton.h>
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
    m_weekdayNames=defaultWeekdayNames();

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
        cell->onPressCaller.add(this, &Calendar::onCellPress);
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
    displayedMonthChanged();
}

void Calendar::displayedMonthChanged()
{
    rebind();
    onDisplayedMonthChanged.call(this);
}

bool Calendar::canShowPreviousMonth() const
{
    // the last day of the month before must not be before the minimum
    return !m_min || !(Date(m_year, m_month, 1).addDays(-1) < *m_min);
}

bool Calendar::canShowNextMonth() const
{
    return !m_max || !(Date(m_year, m_month, 1).addMonths(1) > *m_max);
}

bool Calendar::showPreviousMonth()
{
    if(!canShowPreviousMonth())
        return false;

    const Date previous=Date(m_year, m_month, 1).addMonths(-1);

    setDisplayedMonth(previous.year, previous.month);
    return true;
}

bool Calendar::showNextMonth()
{
    if(!canShowNextMonth())
        return false;

    const Date next=Date(m_year, m_month, 1).addMonths(1);

    setDisplayedMonth(next.year, next.month);
    return true;
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

bool Calendar::isDateEnabled(const Date &date) const
{
    if(m_min && (date < *m_min))
        return false;
    if(m_max && (date > *m_max))
        return false;
    return !m_filter || m_filter(date);
}

void Calendar::setSelectionMode(SelectionMode mode)
{
    if(mode == m_selectionMode)
        return;

    m_selectionMode=mode;
    if(mode != SelectionMode::Single)
        m_selected.reset();
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

void Calendar::setMonthNames(const std::vector<String> &names)
{
    if((names.size() != 12) || (names == m_monthNames))
        return;

    m_monthNames=names;
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

const String &Calendar::getWeekdayName(int weekday) const
{
    static const String none;

    if((weekday < 0) || (weekday > 6))
        return none;
    return m_weekdayNames[static_cast<size_t>(weekday)];
}

String Calendar::titleText() const
{
    return getMonthName(m_month)+" "+std::to_string(m_year);
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

internal::CalendarCellLook Calendar::dayLook(const Date &date) const
{
    internal::CalendarCellLook look;

    look.inMonth=(date.year == m_year) && (date.month == m_month);
    look.today=m_today && (date == *m_today);
    look.selected=(m_selectionMode == SelectionMode::Single) && m_selected && (date == *m_selected);
    look.disabled=!isDateEnabled(date);
    return look;
}

void Calendar::rebind()
{
    m_title->setText(titleText());
    for(int column=0; column < Columns; ++column)
        m_weekdays[static_cast<size_t>(column)]->setText(getWeekdayName((m_firstDayOfWeek+column)%7));

    for(int i=0; i < CellCount; ++i)
    {
        const Date date=cellDate(i);

        m_cells[static_cast<size_t>(i)]->bind(date, std::to_string(date.day), i%Columns, Columns, dayLook(date));
    }

    m_previous->setDisabled(!canShowPreviousMonth());
    m_next->setDisabled(!canShowNextMonth());
}

void Calendar::chooseDate(const Date &date)
{
    if(!isDateEnabled(date))
        return;

    if((date.year != m_year) || (date.month != m_month))
    {
        m_year=date.year;
        m_month=date.month;
        // bound with the selection below
        onDisplayedMonthChanged.call(this);
    }

    const bool changed=(m_selectionMode == SelectionMode::Single) && (m_selected != date);

    if(changed)
        m_selected=date;
    rebind();
    if(changed)
        onSelectionChanged.call(this);
}

void Calendar::onCellPress(event::Info info)
{
    const internal::CalendarCell *cell=static_cast<const internal::CalendarCell *>(info.controlCaller);

    if(!cell || (cell->m_calendar != this))
        return;
    chooseDate(cell->getDate());
}

void Calendar::onPreviousPress(event::Info)
{
    showPreviousMonth();
}

void Calendar::onNextPress(event::Info)
{
    showNextMonth();
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

// A fixed size: the header, the weekday row and the grid, whatever the month.
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
// children marked, and a part given new bounds here is.
void Calendar::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();
    const int width=Columns*CellWidth;
    const int buttonY=padding.top+(HeaderHeight-ButtonSize)/2;
    const int weekdayY=padding.top+HeaderHeight+Gap;
    const int gridY=weekdayY+WeekdayHeight;

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

            placeChild(cell, padding.left+(i%Columns)*CellWidth, cell->getY(), CellWidth, cell->getHeight());
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

            placeChild(cell, cell->getX(), gridY+(i/Columns)*CellHeight, cell->getWidth(), CellHeight);
        }
    }

    ParentClass::arrange(skin, dim);
}

}//namespace controls
}//namespace gweni
