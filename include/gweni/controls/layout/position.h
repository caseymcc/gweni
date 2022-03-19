/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_layout_position_h_
#define _gweni_controls_layout_position_h_

#include <gweni/controls/label.h>
#include <gweni/utility.h>


namespace gweni
{
namespace controls
{
namespace layout
{
class GWENI_EXPORT CalcPosition: public Base
{
public:

    GWENI_CONTROL_INLINE(CalcPosition, Base)
    {
        setPosition(Position::Left | Position::Top);
    }

public:
    void postLayout(skin::Base *skin) override
    {
        for(auto &&child : getChildren())
        {
            if(child->getDock() != DockPosition::None)
                continue;

            child->setPosition(m_position);
        }
    }

    void setPosition(Position p)
    {
        if(m_position != p)
        {
            m_position=p;
            invalidate();
        }
    }

private:

    Position m_position;
};


class GWENI_EXPORT Center: public CalcPosition
{
    GWENI_CONTROL_INLINE(Center, CalcPosition)
    {
        setPosition(Position::Center);
    }
};


}//namespace layout
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_layout_position_h_
