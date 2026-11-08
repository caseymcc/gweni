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
    // Checked state is visual (box fill / checkmark / radio dot), so the skin
    // must regenerate the primitive — otherwise the toggle never re-renders.
    addStateChange(StateChange_Background);
    onCheckStatusChanged();
}

void CheckBox::setLabelHovered(bool hovered)
{
    if(m_labelHovered == hovered)
        return;

    m_labelHovered=hovered;
    redraw();
}

void CheckBox::onLabelHoverEnter(event::Info)
{
    setLabelHovered(true);
}

void CheckBox::onLabelHoverLeave(event::Info)
{
    setLabelHovered(false);
}

}//namespace controls
}//namespace gweni
