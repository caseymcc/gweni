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
    m_text->setFont(getSkin()->getDefaultFont());
    // Where the glyphs sit in the label (setTextAlignment); the label has no
    // alignment in its parent unless one is set. Subclasses that want another
    // text alignment set it themselves.
    setDefaultTextAlignment(Alignment::Left|Alignment::CenterV);
    setMouseInputEnabled(false);
    setBounds(0, 0, 100, 10);
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
}

void Label::setText(const String &str, bool doEvents)
{
#ifdef DEBUG_ID
    String local=str+" ("+std::to_string(getControlId())+")";

    if(m_text->getText() == local)
        return;

    m_text->setText(local);
#else //DEBUG_ID
    if(m_text->getText() == str)
        return;

    m_text->setText(str);
//    redraw();
#endif //DEBUG_ID

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

// A wrapped m_text rebuilds its lines itself when it is laid out at a new
// width (Text::arrange): the new bounds invalidate it as placement.
void Label::onBoundsChanged(Rect oldChildBounds)
{
    ParentClass::onBoundsChanged(oldChildBounds);
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
