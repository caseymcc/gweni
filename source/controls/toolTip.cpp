/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/toolTip.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(ToolTip)
{
    setMouseInputEnabled(false);
    setPadding(Padding(5, 3, 5, 3));
    setTextColorOverride(getSkin()->Colors.TooltipText);
    // exactly the size setContentFor() measures, wherever it is placed
    setSizeFlags(SizeFlags(SizeFlag::Fixed, SizeFlag::Fixed));
}

void ToolTip::updateColors()
{
    setTextColorOverride(getSkin()->Colors.TooltipText);
}

ToolTip::~ToolTip()
{
    releaseContent(false);
}

Point ToolTip::placement(const Rect &anchor, const Point &size, const Rect &area)
{
    Point pos(anchor.x, anchor.y+anchor.h+Gap);

    if(pos.y+size.y > area.y+area.h)
    {
        const int above=anchor.y-Gap-size.y;

        pos.y=(above >= area.y)?above:std::max(area.y, area.y+area.h-size.y);
    }

    if(pos.x+size.x > area.x+area.w)
        pos.x=area.x+area.w-size.x;
    if(pos.x < area.x)
        pos.x=area.x;

    return pos;
}

static Rect canvasRect(Base *control)
{
    const Point origin=control->localPosToCanvas();
    return Rect(origin.x, origin.y, control->getWidth(), control->getHeight());
}

void ToolTip::setContentFor(Base *owner, const Rect &area)
{
    releaseContent(false);

    Point size;

    if(Base *custom=owner->getToolTip())
    {
        setText("");

        custom->setParent(this);
        custom->setHidden(false);
        custom->setMouseInputEnabled(false);
        custom->setPos(m_padding.left, m_padding.top);
        m_content=custom;
        m_contentOwner=owner;

        size=Point(custom->getWidth()+m_padding.left+m_padding.right,
            custom->getHeight()+m_padding.top+m_padding.bottom);
    }
    else
    {
        setText(owner->getToolTipText());
        sizeToContents();
        size=getSize();
    }

    const Point pos=placement(canvasRect(owner), size, area);
    setBounds(pos.x, pos.y, size.x, size.y);
}

void ToolTip::placeFor(Base *owner, const Rect &area)
{
    const Point pos=placement(canvasRect(owner), getSize(), area);

    if(pos.x != getX() || pos.y != getY())
        setPos(pos.x, pos.y);
}

void ToolTip::releaseContent(bool ownerDeleted)
{
    Base *content=m_content;
    Base *owner=m_contentOwner;

    m_content=nullptr;
    m_contentOwner=nullptr;

    if(!content)
        return;

    if(ownerDeleted)
    {
        content->setParent(nullptr);
        delete content;
        return;
    }

    // where Base::setToolTip() keeps it while it is not shown
    content->setParent(owner);
    content->setHidden(true);
}

}//namespace controls
}//namespace gweni
