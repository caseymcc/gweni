/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/groupBox.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(GroupBox)
{
    // Set to true, because it's likely that our
    // children will want mouse input, and they
    // can't get it without us..
    setMouseInputEnabled(true);
    setTextPadding(Padding(10, 0, 0, 0));
    setAlignment(Alignment::Top|Alignment::Left);
    invalidate();
    m_innerPanel=newChild<Base>();
    m_innerPanel->setDock(DockPosition::Center);
    m_innerMargin=6;
}

// The inner panel's margin clears the title, from the measured title rather
// than m_text's bounds, which the pass has not set yet. Settled before this
// control is measured, so the sizes taken from the margin are this pass's.
void GroupBox::calculateSize(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X)
    {
        PlacementScope placement;

        m_innerPanel->setMargin(Margin(m_innerMargin, m_text->refreshSize(false).height/2 + m_innerMargin,
            m_innerMargin, m_innerMargin));
    }

    ParentClass::calculateSize(skin, dim);
}

}//namespace controls
}//namespace gweni
