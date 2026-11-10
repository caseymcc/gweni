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
        setPositionAlignment(Alignment::Left |Alignment::Top);
    }

public:
    //! Layout only: it positions its children.
    GWENI_DRAWS_NOTHING

    //! Its undocked children are aligned in it: the alignment is handed to
    //! them before they are arranged, as placement (no preferred size reads
    //! a child's alignment), and the generic arrange places them by it.
    void arrange(skin::Base *skin, Dim dim) override
    {
        {
            PlacementScope placement;

            for(auto &&child : getChildren())
            {
                if(child->getDock() == DockPosition::None)
                    child->setAlignment(m_position);
            }
        }
        ParentClass::arrange(skin, dim);
    }

    void setPositionAlignment(Alignment p)
    {
        if(m_position != p)
        {
            m_position=p;
            invalidate();
        }
    }

private:

    Alignment m_position;
};


class GWENI_EXPORT Center: public CalcPosition
{
    GWENI_CONTROL_INLINE(Center, CalcPosition)
    {
        setPositionAlignment(Alignment::Center);
    }
};


}//namespace layout
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_layout_position_h_
