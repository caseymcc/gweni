/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/timeEdit.h>

namespace gweni
{
namespace controls
{

namespace
{

std::optional<internal::SectionedEdit::Value> toValue(const std::optional<TimeOfDay> &time)
{
    if(!time)
        return std::nullopt;

    internal::SectionedEdit::Value value;
    value.time=*time;
    return value;
}

std::optional<TimeOfDay> toTime(const std::optional<internal::SectionedEdit::Value> &value)
{
    if(!value)
        return std::nullopt;
    return value->time;
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(TimeEdit)
{
    setFormat("HH:mm");
}

void TimeEdit::setTime(const std::optional<TimeOfDay> &time)
{
    if(time && !time->isValid())
        return;
    setFieldValue(toValue(time));
}

std::optional<TimeOfDay> TimeEdit::getTime() const
{
    return toTime(getFieldValue());
}

void TimeEdit::setTwelveHour(bool twelveHour)
{
    if(twelveHour != isTwelveHour())
        setFormat(twelveHour?"h:mm AP":"HH:mm");
}

bool TimeEdit::isTwelveHour() const
{
    for(const Section &section : getSections())
    {
        if(section.kind == SectionKind::Hour12)
            return true;
    }
    return false;
}

void TimeEdit::setRange(const std::optional<TimeOfDay> &min, const std::optional<TimeOfDay> &max)
{
    setFieldRange(toValue(min), toValue(max));
}

std::optional<TimeOfDay> TimeEdit::getMinTime() const
{
    return toTime(getFieldMin());
}

std::optional<TimeOfDay> TimeEdit::getMaxTime() const
{
    return toTime(getFieldMax());
}

}//namespace controls
}//namespace gweni
