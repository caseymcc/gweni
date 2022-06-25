/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_draganddrop_h_
#define _gweni_draganddrop_h_

#include <sstream>

#include <gweni/skin.h>
#include <gweni/uiTypes.h>

namespace gweni
{
namespace draganddrop
{
extern GWENI_EXPORT Package *CurrentPackage;
extern GWENI_EXPORT gweni::controls::Base *SourceControl;
extern GWENI_EXPORT gweni::controls::Base *HoveredControl;

bool GWENI_EXPORT start(gweni::controls::Base *control, Package *package);

bool GWENI_EXPORT nnMouseButton(gweni::controls::Base *hoveredControl, int x, int y,
    bool down);
void GWENI_EXPORT onMouseMoved(gweni::controls::Base *hoveredControl, int x, int y);

void GWENI_EXPORT renderOverlay(gweni::controls::Canvas *canvas, skin::Base *skin);

void GWENI_EXPORT controlDeleted(gweni::controls::Base *control);
}

}
#endif//_gweni_draganddrop_h_
