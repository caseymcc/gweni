/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/radioButton.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(RadioButton)
{
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setSize(15, 15);
    setMouseInputEnabled(true);
    setTabable(false);
}


}//namespace controls
}//namespace gweni
