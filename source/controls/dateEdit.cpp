/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/dateEdit.h>
#include <gweni/controls/popup.h>
#include <gweni/inputHandler.h>

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

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(DateEditButton)
{
    // a press focuses the field, not the button
    setKeyboardInputEnabled(false);
    setTabable(false);
    // the width asked for (no text to fit), the dock's height
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
}

GWENI_CONTROL_CONSTRUCTOR(DateEditCalendar)
{
}

void DateEditCalendar::setOwner(DateEdit *owner)
{
    m_ownerId=owner?owner->getControlId():0;
}

bool DateEditCalendar::onKeyEscape(bool down)
{
    DateEdit *owner=dynamic_cast<DateEdit *>(Base::getControlById(m_ownerId));

    if(!owner)
        return ParentClass::onKeyEscape(down);
    if(down)
        owner->closeCalendar(true);
    return true;
}

}//namespace internal

GWENI_CONTROL_CONSTRUCTOR(DateEdit)
{
    m_button=newChild<internal::DateEditButton>();
    m_button->setDock(DockPosition::Right);
    m_button->setWidth(ButtonWidth);
    m_button->onPressCaller.add(this, &DateEdit::onButtonPressed);
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
    m_todaySet=true;
}

bool DateEdit::acceptsValue(const Value &value) const
{
    return !m_filter || m_filter(value.date);
}

bool DateEdit::isCalendarOpen() const
{
    return m_popup && m_popup->isOpen();
}

void DateEdit::openCalendar()
{
    if(isCalendarOpen() || isDisabled() || !getCanvas())
        return;

    // what was typed is the date the calendar opens on
    commit();

    if(!m_popup)
    {
        // this control's: deleted with it, wherever it is then
        m_popup=createPopup();
        if(!m_popup)
            return;
        m_calendar=m_popup->newChild<internal::DateEditCalendar>();
        m_calendar->setOwner(this);
        m_calendar->setSelectionMode(Calendar::SelectionMode::Single);
        m_calendar->onDateChosen.add(this, &DateEdit::onDateChosen);
    }

    const std::optional<Date> date=getDate();

    m_calendar->setView(Calendar::View::Month);
    m_calendar->setDateLimits(getMinDate(), getMaxDate());
    m_calendar->setDateFilter(m_filter);
    m_calendar->setToday(m_todaySet?std::optional<Date>(getToday()):std::nullopt);
    m_calendar->setSelectedDate(date);
    m_calendar->setCursor(date?*date:clampValue(getEmptyStepValue()).date);

    m_popup->open(this, PopupPlacement::Below);
    m_calendar->focus();
}

void DateEdit::closeCalendar(bool focusField)
{
    if(isCalendarOpen())
        m_popup->close();
    if(focusField)
        focus();
}

void DateEdit::toggleCalendar()
{
    if(isCalendarOpen())
        closeCalendar(true);
    else
        openCalendar();
}

void DateEdit::onButtonPressed(event::Info)
{
    toggleCalendar();
}

void DateEdit::onDateChosen(event::Info)
{
    const std::optional<Date> date=m_calendar->getSelectedDate();

    closeCalendar(true);
    if(date)
        userSetValue(toValue(date));
}

bool DateEdit::onKeyDown(bool down)
{
    if(!input::isKeyDown(keys::Alt))
        return ParentClass::onKeyDown(down);

    if(down)
        openCalendar();
    return true;
}

bool DateEdit::onKeyF4(bool down)
{
    if(down)
        openCalendar();
    return true;
}

}//namespace controls
}//namespace gweni
