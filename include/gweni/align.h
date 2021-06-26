/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_align_h_
#define _gweni_align_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace align
{

inline void placeBelow(controls::Base *ctrl, controls::Base *below, int border=0)
{
    ctrl->setPos(ctrl->getX(), below->bottom()+border);
}

}//namespace align
}//namespace gweni

#endif//_gweni_align_h_
