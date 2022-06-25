/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/label.h>
#include <gweni/platforms/commonPlatform.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(Label)
{
    m_createdFont=nullptr;
    m_text=newChild<internal::Text>();
//    m_text->setFont(getSkin()->getDefaultFont());
    m_text->setFont(getDefaultFont());
    setMouseInputEnabled(false);
    setBounds(0, 0, 100, 10);
//    setAlignment(Position::Left | Position::CenterV);
    setAlignment(Alignment::Left | Alignment::CenterV);
    setSizeFlags(SizeFlags(SizeFlag::Elastic, SizeFlag::Elastic));
}

void Label::preDelete(skin::Base *skin)
{
    if(m_createdFont)
    {
        skin->releaseFont(m_createdFont);
        delete m_createdFont;
        m_createdFont=nullptr;
        setFont(skin->getDefaultFont());
    }
}

void Label::postLayout(skin::Base * /*skin*/)
{
//    m_text->setPosition(m_align);
}

//void Label::setAlignment(Position iAlign)
//{
//    if(m_align == iAlign)
//        return;
//
//    m_align=iAlign;
//    invalidate();
//}
//
//Position Label::getAlignment()
//{
//    return m_align;
//}

void Label::setText(const String &str, bool doEvents)
{
    if(m_text->getText() == str)
        return;

    m_text->setText(str);
//    redraw();

    if(doEvents)
        onTextChanged();
}

void Label::sizeToContents()
{
    m_text->setPos(m_padding.left, m_padding.top);
    m_text->refreshSize();
    setSize(m_text->getWidth()+m_padding.left+m_padding.right,
        m_text->getHeight()+m_padding.top+m_padding.bottom);
}

Rect Label::getCharacterPosition(int value)
{
    Rect p=m_text->getCharacterPosition(value);
    p.x+=m_text->getX();
    p.y+=m_text->getY();
    return p;
}

void Label::onBoundsChanged(Rect oldChildBounds)
{
    ParentClass::onBoundsChanged(oldChildBounds);

    if(m_text->wrap())
    {
        m_text->refreshSize();
        invalidate();
    }
}

void Label::setFont(String strFacename, int iSize, bool bBold)
{
    if(m_createdFont)
    {
        getSkin()->releaseFont(m_createdFont);
        delete m_createdFont;
        m_createdFont=nullptr;
        setFont(getSkin()->getDefaultFont());
    }

    m_createdFont=new Font();
    GWENI_ASSERT_MSG(m_createdFont != nullptr, "Couldn't Create Font!");
    m_createdFont->bold=bBold;
    m_createdFont->facename=strFacename;
    m_createdFont->size=iSize;
    setFont(*m_createdFont);
    m_text->refreshSize();
}

}//namespace controls
}//namespace gweni
