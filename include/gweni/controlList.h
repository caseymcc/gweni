/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controllist_h_
#define _gweni_controllist_h_

#include <algorithm>

namespace gweni
{

struct Point;

namespace controls
{

class Base;

}//namespace controls

namespace event
{

class Handler;
struct Information;
class Packet;
struct Info;

}//namespace event

template <typename _TYPE>
class EasyList
{
public:

    typedef std::list<_TYPE> List;

    void add(_TYPE control)
    {
        if(contains(control))
            return;

        m_list.push_back(control);
    }

    void remove(_TYPE control)
    {
        m_list.remove(control);
    }

    void add(const List &list)
    {
        for(auto const &item : list)
        {
            add(item);
        }
    }

    void add(const EasyList<_TYPE> &list)
    {
        add(list.list);
    }

    bool contains(_TYPE control) const
    {
        typename List::const_iterator it=std::find(m_list.cbegin(), m_list.cend(), control);

        return it != m_list.cend();
    }

    inline void clear()
    {
        m_list.clear();
    }

    List m_list;
};


class ControlList: public EasyList<controls::Base *>
{
public:

    void enable();
    void disable();

    void show();
    void hide();

    String getValue();
    void setValue(const String &value);

    template <typename T>
    void setAction(event::Handler *ob,
        void (T:: *f)(event::Info),
        const event::Packet &packet)
    {
        setActionInternal(ob,
            static_cast<void(event::Handler:: *) (event::Info)>(f),
            packet);
    }

    void moveBy(const Point &point);

    void doAction();

protected:

    void setActionInternal(event::Handler *object,
        void (event::Handler:: *f)(event::Info),
        const event::Packet &packet);
};

}//namespace gweni

#endif//_gweni_controllist_h_
