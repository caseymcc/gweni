/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/dateEdit.h>

#include <utility>

namespace gweni
{
namespace controls
{

namespace
{

std::optional<internal::SectionedEdit::Value> toValue(const std::optional<Date> &date)
{
    if(!date)
        return std::nullopt;

    internal::SectionedEdit::Value value;
    value.date=*date;
    return value;
}

std::optional<Date> toDate(const std::optional<internal::SectionedEdit::Value> &value)
{
    if(!value)
        return std::nullopt;
    return value->date;
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(DateEdit)
{
    setFormat("YYYY-MM-DD");
}

void DateEdit::setDate(const std::optional<Date> &date)
{
    if(date && !date->isValid())
        return;
    setFieldValue(toValue(date));
}

std::optional<Date> DateEdit::getDate() const
{
    return toDate(getFieldValue());
}

void DateEdit::setRange(const std::optional<Date> &min, const std::optional<Date> &max)
{
    setFieldRange(toValue(min), toValue(max));
}

std::optional<Date> DateEdit::getMinDate() const
{
    return toDate(getFieldMin());
}

std::optional<Date> DateEdit::getMaxDate() const
{
    return toDate(getFieldMax());
}

void DateEdit::setDateFilter(std::function<bool(const Date &)> filter)
{
    m_filter=std::move(filter);
}

bool DateEdit::isDateEnabled(const Date &date) const
{
    const Value value=*toValue(date);

    return (clampValue(value) == value) && acceptsValue(value);
}

void DateEdit::setToday(const Date &today)
{
    if(!today.isValid())
        return;

    Value value;
    value.date=today;
    setEmptyStepValue(value);
}

bool DateEdit::acceptsValue(const Value &value) const
{
    return !m_filter || m_filter(value.date);
}

}//namespace controls
}//namespace gweni
