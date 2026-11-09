/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/menu.h>

namespace gweni
{
namespace controls
{

namespace
{
bool sameRect(const Rect &a, const Rect &b)
{
    return (a.x == b.x) && (a.y == b.y) && (a.w == b.w) && (a.h == b.h);
}
}

GWENI_CONTROL_CONSTRUCTOR(Popup)
{
    setPadding(Padding(6, 6, 6, 6));
    // placed and sized by itself, next to its anchor, not by the canvas
    setPlacement(Placement::Owner);
    setHidden(true);
}

Popup::~Popup()
{
    // a canvas closes it in ~Base (Canvas::popupControlDeleted)
    setOwner(nullptr);
}

void Popup::setOwner(Base *owner)
{
    if(owner == m_owner)
        return;

    if(m_owner)
    {
        Popup **link=&m_owner->m_ownedPopups;

        while(*link != this)
            link=&(*link)->m_nextOwned;
        *link=m_nextOwned;
    }

    m_owner=owner;
    m_nextOwned=nullptr;

    if(owner)
    {
        m_nextOwned=owner->m_ownedPopups;
        owner->m_ownedPopups=this;
    }
}

void Popup::open(Base *anchor, PopupPlacement side)
{
    Canvas *canvas=anchor ? anchor->getCanvas() : nullptr;

    if(!canvas)
        return;

    canvas->showPopup(this, anchor, side);
}

void Popup::close()
{
    setHidden(true);
}

bool Popup::isOpen()
{
    Canvas *canvas=getCanvas();
    return canvas && (canvas->shownPopup() == this);
}

void Popup::setHidden(bool hidden)
{
    if(hidden && isOpen())
    {
        // the canvas hides it, then fires onClose
        getCanvas()->closePopup();
        return;
    }

    ParentClass::setHidden(hidden);
}

void Popup::setAnchor(Base *anchor, PopupPlacement side)
{
    m_anchor=anchor;
    m_side=side;
    m_placedFor=Rect();
}

Rect Popup::anchorRect() const
{
    if(!m_anchor)
        return Rect();

    const Point origin=m_anchor->localPosToCanvas();
    return Rect(origin.x, origin.y, m_anchor->getWidth(), m_anchor->getHeight());
}

void Popup::followAnchor()
{
    if(!sameRect(anchorRect(), m_placedFor))
        invalidate();
}

// The canvas's arrange skips it (Placement::Owner), so it takes the size the
// canvas would give a dock-None child (policyResize) and the position
// placement() gives it, before the subtree is arranged. Each axis needs only
// that axis's size: X its width, Y its height, final when its arrange runs.
// The anchor was arranged before it in this pass, as the popup comes after
// the anchor's top-level ancestor among the canvas's children.
void Popup::arrange(skin::Base *skin, Dim dim)
{
    Canvas *canvas=getCanvas();

    if(canvas && (getParent() == canvas))
    {
        policyResize(getPreferredSize(), dim);

        const Rect area(0, 0, canvas->getWidth(), canvas->getHeight());
        const Rect anchor=anchorRect();
        const Point pos=placement(anchor, Point(getWidth(), getHeight()), area, m_side);

        if(dim == Dim::X)
            setArrangedBounds(pos.x, getY(), getWidth(), getHeight());
        else
        {
            setArrangedBounds(getX(), pos.y, getWidth(), getHeight());
            m_placedFor=anchor;
        }
    }

    ParentClass::arrange(skin, dim);
}

Point Popup::placement(const Rect &anchor, const Point &size, const Rect &area,
    PopupPlacement side, int gap)
{
    const int right=area.x+area.w;
    const int bottom=area.y+area.h;
    Point pos;

    if(side == PopupPlacement::Below)
    {
        pos=Point(anchor.x, anchor.y+anchor.h+gap);

        if(pos.y+size.y > bottom)
            pos.y=anchor.y-gap-size.y;

        if(pos.x+size.x > right)
            pos.x=anchor.x+anchor.w-size.x;
    }
    else
    {
        pos=Point(anchor.x+anchor.w+gap, anchor.y);

        if(pos.x+size.x > right)
            pos.x=anchor.x-gap-size.x;

        if(pos.y+size.y > bottom)
            pos.y=anchor.y+anchor.h-size.y;
    }

    return Menu::keepInside(pos, size, area);
}

}//namespace controls
}//namespace gweni
