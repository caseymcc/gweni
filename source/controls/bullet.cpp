/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/bullet.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(Bullet)
{
    updateColumn();
}

void Bullet::setFont(String strFacename, int iSize, bool bBold)
{
    // ends in setFont(const Font &), which sizes the column
    ParentClass::setFont(strFacename, iSize, bBold);
}

void Bullet::setFont(const Font &font)
{
    ParentClass::setFont(font);
    updateColumn();
    addStateChange(StateChange_Background);
}

void Bullet::setTextColor(const Color &col)
{
    ParentClass::setTextColor(col);
    addStateChange(StateChange_Background);
}

void Bullet::setSkinTextColor(internal::SkinTextColor which)
{
    ParentClass::setSkinTextColor(which);
    addStateChange(StateChange_Background);
}

void Bullet::setTextColorOverride(const Color &col)
{
    ParentClass::setTextColorOverride(col);
    addStateChange(StateChange_Background);
}

void Bullet::setColumnGap(int gap)
{
    if(gap == m_columnGap)
        return;

    m_columnGap=gap;
    updateColumn();
}

// The column is the left padding, so the text is laid out after it and the
// label's preferred width counts it.
void Bullet::updateColumn()
{
    const int column=static_cast<int>(getFont().size+0.5f)+m_columnGap;

    if(m_padding.left == column)
        return;

    Padding padding=m_padding;

    padding.left=column;
    setPadding(padding);
}

Rect Bullet::bulletRect()
{
    const int fontSize=static_cast<int>(getFont().size+0.5f);
    const int diameter=std::max(3, static_cast<int>(getFont().size*0.4f+0.5f));
    const Rect &text=m_text->getBounds();
    const Padding &textPadding=m_text->getPadding();
    const int lineTop=text.y+textPadding.top;
    const int lineHeight=wrap()?m_text->getLineBox(0).h:text.h-textPadding.top-textPadding.bottom;

    return Rect((fontSize-diameter)/2, lineTop+(lineHeight-diameter)/2, diameter, diameter);
}

Color Bullet::bulletColor()
{
    const Color &colorOverride=m_text->getTextColorOverride();

    return colorOverride.a == 0?m_text->getTextColor():colorOverride;
}

// The circle follows the text's first line.
void Bullet::onChildBoundsChanged(Rect oldChildBounds, Base *child)
{
    ParentClass::onChildBoundsChanged(oldChildBounds, child);

    if(child == m_text)
        addStateChange(StateChange_Background);
}

}//namespace controls
}//namespace gweni
