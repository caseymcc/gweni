/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/separator.h>

namespace gweni
{
namespace controls
{

// No size is asked for, so the separator is its preferred size where nothing
// gives it another: one unit across, nothing along. It grows along the rule
// only, so a layout with room to spare gives none of it to the rule's
// thickness.
GWENI_CONTROL_CONSTRUCTOR(Separator)
{
    setMouseInputEnabled(false);
    setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});
}

void Separator::setVertical()
{
    if(!m_horizontal)
        return;

    m_horizontal=false;
    setSizeFlags({SizeFlag::Shrink, SizeFlag::Expand});

    if(m_caption)
        m_caption->setHidden(true);

    invalidate();
    invalidateParent();
    addStateChange(StateChange_Background);
}

void Separator::setHorizontal()
{
    if(m_horizontal)
        return;

    m_horizontal=true;
    setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});

    if(m_caption)
        m_caption->setHidden(getText().empty());

    invalidate();
    invalidateParent();
    addStateChange(StateChange_Background);
}

void Separator::setText(const String &text)
{
    if(!m_caption)
    {
        if(text.empty())
            return;

        // placed by the request alone: an unaligned child keeps its position
        m_caption=newChild<Label>();
        m_caption->setPos(m_captionInset, 0);
    }

    const bool wasShown=captionShown();

    m_caption->setText(text);
    m_caption->setHidden(text.empty() || !m_horizontal);

    // shown or hidden, the rule changes from one piece to two; a new width
    // moves the right half (onChildBoundsChanged)
    if(captionShown() != wasShown)
    {
        invalidate();
        invalidateParent();
        addStateChange(StateChange_Background);
    }
}

const String &Separator::getText() const
{
    static const String none;

    return m_caption?m_caption->getText():none;
}

bool Separator::captionShown() const
{
    return m_horizontal && m_caption && !m_caption->getText().empty();
}

void Separator::setCaptionInset(int inset)
{
    if(inset == m_captionInset)
        return;

    m_captionInset=inset;

    if(m_caption)
        m_caption->setPos(m_captionInset, 0);

    invalidate();
    invalidateParent();
}

void Separator::setCaptionGap(int gap)
{
    if(gap == m_captionGap)
        return;

    m_captionGap=gap;
    invalidate();
    invalidateParent();
    addStateChange(StateChange_Background);
}

// One unit across the rule and nothing along it; with a caption, the caption's
// height and room for it, its inset and one gap after it.
void Separator::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    const bool along=(dim == Dim::X) == m_horizontal;
    int content;

    if(captionShown())
    {
        const Size &caption=m_caption->getPreferredSize();

        content=(dim == Dim::X)?m_captionInset+caption.width+m_captionGap:caption.height;
    }
    else
        content=along?0:1;

    content+=(dim == Dim::X)?m_padding.left+m_padding.right:m_padding.top+m_padding.bottom;

    if(dim == Dim::X)
        m_preferredSize.width=preferredFromContent(content, dim);
    else
        m_preferredSize.height=preferredFromContent(content, dim);
}

// The rule stops captionGap() short of the caption, so a caption of another
// width redraws it.
void Separator::onChildBoundsChanged(Rect oldChildBounds, Base *child)
{
    ParentClass::onChildBoundsChanged(oldChildBounds, child);

    if(child == m_caption)
        addStateChange(StateChange_Background);
}

}//namespace controls
}//namespace gweni
