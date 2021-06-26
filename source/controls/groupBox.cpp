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
    setAlignment(Position::Top|Position::Left);
    invalidate();
    m_innerPanel=new Base(this);
    m_innerPanel->dock(Position::Fill);
    m_innerMargin=6;
}

void GroupBox::layout(skin::Base *skin)
{
    m_innerPanel->setMargin(Margin(m_innerMargin, textHeight()/2 + m_innerMargin,
        m_innerMargin, m_innerMargin));
    ParentClass::layout(skin);
}

void GroupBox::render(skin::Base *skin)
{
//    skin->drawGroupBox(this, skin::Generate, textX(), textHeight(), textWidth());
    skin->drawControl(this);
}

}//namespace controls
}//namespace gweni
