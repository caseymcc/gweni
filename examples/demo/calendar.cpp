/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/calendar.h"
#include "gweni/controls/calendar.h"
#include "gweni/controls/label.h"

namespace gweni
{

namespace
{

// The demo's "today": fixed, so every run draws the same page.
constexpr Date Today(2026, 11, 14);

// A captioned calendar on November 2026, docked Left in its row.
controls::Calendar *card(controls::Base *row, const char *text)
{
    controls::Base *card=row->newChild<controls::Base>();

    card->setDock(DockPosition::Left);
    card->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    card->setMargin(Margin(0, 0, 16, 0));

    controls::Label *label=card->newChild<controls::Label>();

    label->setText(text);
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(0, 0, 0, 3));

    controls::Calendar *calendar=card->newChild<controls::Calendar>();

    calendar->setDock(DockPosition::Top);
    calendar->setDisplayedMonth(Today.year, Today.month);
    calendar->setToday(Today);
    return calendar;
}

// A row of two cards, docked Top in the demo.
controls::Base *row(controls::Base *demo)
{
    controls::Base *row=demo->newChild<controls::Base>();

    row->setDock(DockPosition::Top);
    row->setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});
    row->setMargin(Margin(0, 0, 0, 8));
    return row;
}

String dateText(const std::optional<Date> &date)
{
    return date?date->toIso():String("-");
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(CalendarDemo)
{
    setDock(DockPosition::Center);
    setPadding(Padding(10, 4, 10, 4));

    controls::Base *top=row(this);

    m_single=card(top, "Single, today ringed");
    m_single->setSelectedDate(Date(2026, 11, 11));

    m_range=card(top, "Range");
    m_range->setSelectionMode(controls::Calendar::SelectionMode::Range);
    m_range->setRange(Date(2026, 11, 9), Date(2026, 11, 13));

    controls::Base *bottom=row(this);

    m_limited=card(bottom, "Nov 5 - Dec 18, weekdays only");
    m_limited->setDateLimits(Date(2026, 11, 5), Date(2026, 12, 18));
    m_limited->setDateFilter([](const Date &date) { return (date.weekday() != 0) && (date.weekday() != 6); });

    m_sunday=card(bottom, "The week from Sunday");
    m_sunday->setFirstDayOfWeek(0);

    m_selection=newChild<controls::Label>();
    m_selection->setDock(DockPosition::Top);
    updateLabel();

    for(controls::Calendar *calendar : {m_single, m_range, m_limited, m_sunday})
        calendar->onSelectionChanged.add(this, &CalendarDemo::onSelectionChanged);
}

void CalendarDemo::setLabelUpdates(bool updates)
{
    m_labelUpdates=updates;
    if(updates)
        updateLabel();
}

void CalendarDemo::onSelectionChanged(event::Info)
{
    if(m_labelUpdates)
        updateLabel();
}

// "Single: 2026-11-11   Range: 2026-11-09 to 2026-11-13   Weekday: -   Sunday: -"
void CalendarDemo::updateLabel()
{
    m_selection->setText("Single: "+dateText(m_single->getSelectedDate())+"   Range: "
        +dateText(m_range->getRangeStart())+" to "+dateText(m_range->getRangeEnd())+"   Weekday: "
        +dateText(m_limited->getSelectedDate())+"   Sunday: "+dateText(m_sunday->getSelectedDate()));
}

}//namespace gweni
