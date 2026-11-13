/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/badge.h>
#include <gweni/skin.h>

#include <algorithm>
#include <string>

namespace gweni
{
namespace controls
{

// A count of 0 shows nothing, so a new badge is hidden; made on its anchor
// (newChild), it is attached to the top-right corner. Hit-testing passes an
// attached child over (Base::attachTo), so it takes no mouse input.
GWENI_CONTROL_CONSTRUCTOR(Badge)
{
    setKeyboardInputEnabled(false);
    setHidden(true);

    if(Base *anchor=getParent())
        attachTo(anchor, Corner::TopRight);
}

void Badge::attach(Base *anchor, Corner corner, const Point &offset)
{
    attachTo(anchor, corner, offset);
}

void Badge::setCount(int count)
{
    if(count == m_count)
        return;
    m_count=count;
    refresh();
}

void Badge::setCap(int cap)
{
    cap=std::max(1, cap);
    if(cap == m_cap)
        return;
    m_cap=cap;
    refresh();
}

void Badge::setShowZero(bool show)
{
    if(show == m_showZero)
        return;
    m_showZero=show;
    refresh();
}

void Badge::setDot(bool dot)
{
    if(dot == m_dot)
        return;
    m_dot=dot;
    refresh();
}

const gweni::Font &Badge::getFont()
{
    return getSkin()->getDefaultFont();
}

gweni::Point Badge::textOrigin() const
{
    const Rect &bounds=getBounds();

    return gweni::Point(bounds.x+(bounds.w-m_textSize.x)/2, bounds.y+(bounds.h-m_textSize.y)/2);
}

gweni::Point Badge::sizeFor(const gweni::Point &textSize) const
{
    if(m_dot)
        return gweni::Point(DotSize, DotSize);

    // a single digit is a circle, a longer number a pill
    const int height=std::max(DotSize, textSize.y);

    return gweni::Point(std::max(height, textSize.x+2*PaddingX), height);
}

// The number is measured here, in X, where any layout of the badge starts:
// what refresh() measured for its own decision is measured again, so a
// renderer's new text layout (textLayoutSerial(), which lays the whole tree
// out) reaches it.
void Badge::calculateSize(skin::Base * /*skin*/, Dim dim)
{
    if(dim == Dim::X)
        m_textSize=m_text.empty()?gweni::Point(0, measureLineHeight(getFont())):measureText(getFont(), m_text);

    const gweni::Point size=sizeFor(m_textSize);

    if(dim == Dim::X)
        m_preferredSize.width=size.x;
    else
        m_preferredSize.height=size.y;
}

// Only what changed is marked: a new string redraws the badge's text and
// bubble (StateChange_Text), and lays the badge out only if the size it takes
// changes; on no canvas yet, it is laid out once it is on one.
void Badge::refresh()
{
    const bool shown=m_dot || (m_count != 0) || m_showZero;
    String text;

    if(shown && !m_dot)
        text=(m_count > m_cap)?std::to_string(m_cap)+"+":std::to_string(m_count);

    if(text != m_text)
    {
        m_text=text;
        addStateChange(StateChange_Text);

        if(!getCanvas())
        {
            invalidate();
            setHidden(!shown);
            return;
        }
        if(!m_text.empty())
            m_textSize=measureText(getFont(), m_text);
    }

    const gweni::Point size=sizeFor(m_textSize);

    if((size.x != getWidth()) || (size.y != getHeight()))
        invalidate();
    setHidden(!shown);
}

}//namespace controls
}//namespace gweni
