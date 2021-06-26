/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */



#ifndef _gweni_tooltip_h_
#define _gweni_tooltip_h_

#include <gweni/controls/base.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>

namespace gweni
{
namespace tooltip
{
GWENI_EXPORT bool toolTipActive();

GWENI_EXPORT void enable(controls::Base *control);
GWENI_EXPORT void disable(controls::Base *control);

GWENI_EXPORT void controlDeleted(controls::Base *control);

GWENI_EXPORT void renderToolTip(skin::Base *skin);
}
}

#endif//_gweni_tooltip_h_
