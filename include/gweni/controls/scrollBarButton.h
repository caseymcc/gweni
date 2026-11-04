/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_scrollbarbotton_h_
#define _gweni_controls_scrollbarbotton_h_

#include <gweni/controls/button.h>

namespace gweni
{
namespace controls
{
namespace internal
{

class GWENI_EXPORT ScrollBarButton: public controls::Button
{
public:

    GWENI_CONTROL(ScrollBarButton, Button);

public:
    //void render(skin::Base *skin) override;

    void setDirectionUp();
    void setDirectionDown();
    void setDirectionLeft();
    void setDirectionRight();

    Alignment getDirection() { return m_direction; }

protected:

    Alignment m_direction;

};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_scrollbarbotton_h_
