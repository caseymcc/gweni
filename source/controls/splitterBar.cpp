/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/controls/splitterBar.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(SplitterBar)
{
    setTarget(this);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    restrictToParent(true);
}

void SplitterBar::layout(skin::Base * /*skin*/)
{
    moveTo(getX(), getY());
}

}//namespace controls
}//namespace gweni
