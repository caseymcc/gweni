/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/dateTime.h"
#include "gweni/controls/dateEdit.h"
#include "gweni/controls/label.h"
#include "gweni/controls/timeEdit.h"

namespace gweni
{

namespace
{

// The demo's "today": fixed, so every run draws the same page.
constexpr Date Today(2026, 11, 14);
//! The caption column's width.
constexpr int CaptionWidth=220;

// A row docked Top: a caption docked Left, then \p field docked Left.
template<typename Field>
Field *formRow(controls::Base *demo, const char *caption)
{
    controls::Base *row=demo->newChild<controls::Base>();

    row->setDock(DockPosition::Top);
    row->setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});
    row->setMargin(Margin(0, 0, 0, 6));

    controls::Label *label=row->newChild<controls::Label>();

    label->setText(caption);
    label->setDock(DockPosition::Left);
    label->setWidth(CaptionWidth);
    label->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    label->setAlignment(Alignment::Left|Alignment::CenterV);

    Field *field=row->newChild<Field>();

    field->setDock(DockPosition::Left);
    return field;
}

String dateText(const std::optional<Date> &date)
{
    return date?date->toIso():String("-");
}

String timeText(const std::optional<TimeOfDay> &time)
{
    return time?time->toText():String("-");
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(DateTimeDemo)
{
    setDock(DockPosition::Center);
    setPadding(Padding(10, 6, 10, 4));

    m_iso=formRow<controls::DateEdit>(this, "Date, ISO");
    m_iso->setToday(Today);
    m_iso->setDate(Date(2026, 11, 14));

    m_dmy=formRow<controls::DateEdit>(this, "Date, D/M/Y");
    m_dmy->setFormat("DD/MM/YYYY");
    m_dmy->setToday(Today);
    m_dmy->setDate(Date(2026, 12, 24));

    m_limited=formRow<controls::DateEdit>(this, "Nov 5 - Dec 18, weekdays only");
    m_limited->setToday(Today);
    m_limited->setRange(Date(2026, 11, 5), Date(2026, 12, 18));
    m_limited->setDateFilter([](const Date &date) { return (date.weekday() != 0) && (date.weekday() != 6); });
    m_limited->setDate(Date(2026, 11, 13));

    m_optional=formRow<controls::DateEdit>(this, "Optional date (may be empty)");
    m_optional->setToday(Today);
    m_optional->setAllowEmpty(true);
    m_optional->setDate(std::nullopt);
    m_optional->setPlaceholder("none");

    m_time24=formRow<controls::TimeEdit>(this, "Time, 24 h");
    m_time24->setTime(TimeOfDay(14, 30));

    m_time12=formRow<controls::TimeEdit>(this, "Time, 12 h");
    m_time12->setTwelveHour(true);
    m_time12->setTime(TimeOfDay(9, 5));

    m_quarter=formRow<controls::TimeEdit>(this, "Time, 15-minute steps");
    m_quarter->setMinuteStep(15);
    m_quarter->setTime(TimeOfDay(9, 0));

    m_values=newChild<controls::Label>();
    m_values->setDock(DockPosition::Top);
    m_values->setMargin(Margin(0, 6, 0, 0));
    updateLabel();

    for(controls::internal::SectionedEdit *field :
        std::initializer_list<controls::internal::SectionedEdit *>{m_iso, m_dmy, m_limited, m_optional,
            m_time24, m_time12, m_quarter})
        field->onValueChanged.add(this, &DateTimeDemo::onValueChanged);
}

void DateTimeDemo::setLabelUpdates(bool updates)
{
    m_labelUpdates=updates;
    if(updates)
        updateLabel();
}

void DateTimeDemo::onValueChanged(event::Info)
{
    if(m_labelUpdates)
        updateLabel();
}

// "Dates: 2026-11-14, 2026-12-24, 2026-11-13, -   Times: 14:30, 09:05, 09:00"
void DateTimeDemo::updateLabel()
{
    m_values->setText("Dates: "+dateText(m_iso->getDate())+", "+dateText(m_dmy->getDate())+", "
        +dateText(m_limited->getDate())+", "+dateText(m_optional->getDate())+"   Times: "
        +timeText(m_time24->getTime())+", "+timeText(m_time12->getTime())+", "+timeText(m_quarter->getTime()));
}

}//namespace gweni
