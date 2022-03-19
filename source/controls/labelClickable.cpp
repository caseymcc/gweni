/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/labelClickable.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(LabelClickable)
{
    setIsToggle(false);
    setCursor(cursor::Finger);
    setAlignment(Alignment::Left|Alignment::CenterV);
}

void LabelClickable::updateColors()
{
    if(isDisabled())
        return setTextColor(getSkin()->Colors.Button.Disabled);

    if(isDepressed() || getToggleState())
        return setTextColor(getSkin()->Colors.Label.Bright);

    if(isHovered())
        return setTextColor(getSkin()->Colors.Button.Hover);

    setTextColor(getSkin()->Colors.Button.Normal);
}

}//namespace controls
}//namespace gweni
