/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controlList.h>

namespace gweni
{

void ControlList::enable()
{
    for(auto &&item : m_list)
    {
        item->setDisabled(false);
    }
}

void ControlList::disable()
{
    for(auto &&item : m_list)
    {
        item->setDisabled(true);
    }
}

void ControlList::show()
{
    for(auto &&item : m_list)
    {
        item->show();
    }
}

void ControlList::hide()
{
    for(auto &&item : m_list)
    {
        item->hide();
    }
}

gweni::String ControlList::getValue()
{
    for(auto &&item : m_list)
    {
        return item->getValue();
    }

    return "";
}

void ControlList::setValue(const gweni::String &value)
{
    for(auto &&item : m_list)
    {
        item->setValue(value);
    }
}

void ControlList::moveBy(const gweni::Point &point)
{
    for(auto &&item : m_list)
    {
        item->moveBy(point.x, point.y);
    }
}

void ControlList::doAction()
{
    for(auto &&item : m_list)
    {
        item->doAction();
    }
}

void ControlList::setActionInternal(gweni::event::Handler *object,
    void (gweni::event::Handler:: *f)(
        gweni::event::Info), const gweni::event::Packet &packet)
{
    for(auto &&item : m_list)
    {
        item->setAction(object, f, packet);
    }
}

} // namespace gweni
