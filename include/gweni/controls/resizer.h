/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_resizer_h_
#define _gweni_controls_resizer_h_

#include <gweni/controls/base.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/dragger.h>


namespace gweni
{
namespace controls
{
namespace internal
{

class GWENI_EXPORT Resizer: public Dragger
{
public:

    GWENI_CONTROL(Resizer, Dragger);

public:
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    virtual void setResizeDirection(Alignment dir);

    event::Caller onResize;

protected:

    Alignment m_resizeDirection;

};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_resizer_h_
