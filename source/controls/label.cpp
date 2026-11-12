/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/label.h>
#include <gweni/platforms/commonPlatform.h>

#include <algorithm>
#include <cmath>

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

Label::~Label()
{
    if(m_createdFont)
    {
        getSkin()->releaseFont(m_createdFont);
        delete m_createdFont;
    }
}

void Label::preDelete(skin::Base *skin)
{
    if(m_createdFont)
    {
        setFont(skin->getDefaultFont());
        skin->releaseFont(m_createdFont);
        delete m_createdFont;
        m_createdFont=nullptr;
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

// An app call: the label's own size is a request. Where the text sits in it
// and the text's size are computed (the text alignment places it).
void Label::sizeToContents()
{
    placeChildPos(m_text, m_padding.left, m_padding.top);
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
// width (Text::arrange): the new bounds invalidate it as placement. The
// children's clip rects follow the size.
void Label::onBoundsChanged(Rect oldChildBounds)
{
    ParentClass::onBoundsChanged(oldChildBounds);
    refreshChildClipRects();
}

// Whether the Text lies wholly inside the Label follows its bounds.
void Label::onChildBoundsChanged(Rect oldChildBounds, Base *child)
{
    ParentClass::onChildBoundsChanged(oldChildBounds, child);

    if(m_text && (child == m_text))
        refreshChildClipRects();
}

// Measured by tests/labelInkOverhang.cpp: OpenSans with every printable ASCII
// glyph as the last (after 'A') and as the first (before one), at 8 to 24 px
// and canvas scales 1, 1.25, 1.5, 1.75 and 2, grayscale and LCD text, drawn as
// coverage and as SDF. The ink reached past the measured box by up to 4
// device px (a leading glyph at 19-24 px times 1.75 and 2) and 3 on the right
// (13-16 px times 2), and the fewest whole units that showed all of it, the
// box's edges snapped, were 2 at 8-18 and 23 px and 3 at 19-22 and 24 px: about a twelfth
// of the size (a glyph's overhang) and a unit for the anti-aliasing or LCD
// fringe and the snapping.
int Label::inkMargin(float size)
{
    return static_cast<int>(std::ceil(std::max(0.0f, size)/12.0f))+1;
}

void Label::refreshChildClipRects()
{
    if(!m_text)
        return;

    const Rect oldChild=m_childClip;
    const Rect oldText=m_textClip;
    const Rect &text=m_text->getBounds();

    m_childClip=Rect(0, 0, m_bounds.w, m_bounds.h);
    if((text.x >= 0) && (text.y >= 0) && (text.x+text.w <= m_bounds.w) && (text.y+text.h <= m_bounds.h))
    {
        const int margin=inkMargin(m_text->getFont().size);

        m_textClip=Rect(-margin, 0, m_bounds.w+2*margin, m_bounds.h);
    }
    else
        m_textClip=m_childClip;

    // A subclass may clip its children otherwise (TabButton, none).
    const bool childMarks=!(oldChild == m_childClip) && clipRectRegionChanged(oldChild, m_childClip);
    const bool textMarks=!(oldText == m_textClip) && (getChildClipRect(m_text) == &m_textClip)
        && clipRectRegionChanged(oldText, m_textClip);

    if(textMarks)
        m_text->addStateChangeRecursive(StateChange_Geometry);
    if(!childMarks)
        return;
    for(Base *child:m_children)
    {
        if((child != m_text) && (getChildClipRect(child) == &m_childClip))
            child->addStateChangeRecursive(StateChange_Geometry);
    }
}

// The Label loads the font it creates and frees it in preDelete or the next
// setFont. The new one is loaded before the old one is freed, so setting an
// equal font does not free and load it again.
void Label::setFont(String strFacename, int iSize, bool bBold)
{
    Font *previous=m_createdFont;

    m_createdFont=new Font();
    GWENI_ASSERT_MSG(m_createdFont != nullptr, "Couldn't Create Font!");
    m_createdFont->bold=bBold;
    m_createdFont->facename=strFacename;
    m_createdFont->size=iSize;
    getSkin()->loadFont(m_createdFont);
    setFont(*m_createdFont);

    if(previous)
    {
        getSkin()->releaseFont(previous);
        delete previous;
    }

    m_text->refreshSize();
}

}//namespace controls
}//namespace gweni
