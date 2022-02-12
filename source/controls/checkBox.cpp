/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/checkBox.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(CheckBox)
{
    setSize(15, 15);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    m_checked=true;
    toggle();
}


void CheckBox::onPress(event::Info)
{
    if(isDisabled())
        return;

    if(isChecked() && !allowUncheck())
        return;

    toggle();
}

void CheckBox::onCheckStatusChanged()
{
    if(isChecked())
        onChecked.call(this);
    else
        onUnChecked.call(this);

    onCheckChanged.call(this);
}

void CheckBox::setChecked(bool checked)
{
    if(m_checked == checked)
        return;

    m_checked=checked;
    onCheckStatusChanged();
}

}//namespace controls
}//namespace gweni
