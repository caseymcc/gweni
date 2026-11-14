/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#include <gweni/controls/board.h>
#include <gweni/controls/boardColumn.h>

#include <algorithm>

using namespace gweni;
using namespace gweni::controls;

GWENI_CONTROL_CONSTRUCTOR(Board)
{
    setDock(DockPosition::Center);
    setScroll(true, false);
    setMouseInputEnabled(true);
    // The virtual-height path holds the inner panel at the viewport height.
    // Columns own their vertical scrolling; their content is never Board's Y.
    setVirtualContentHeight(0);
}

void Board::setColumnWidth(int width)
{
    width=std::max(1, width);
    if(m_columnWidth == width)
        return;
    m_columnWidth=width;
    invalidate();
}

void Board::setGap(int gap)
{
    gap=std::max(0, gap);
    if(m_gap == gap)
        return;
    m_gap=gap;
    invalidate();
}

void Board::addChild(Base *child)
{
    // Enrol before measuring, including newChild() before recursiveInit().
    if(gweni_cast<BoardColumn>(child))
        child->setPlacement(Placement::Owner);
    ParentClass::addChild(child);
}

void Board::removeChild(Base *child)
{
    const bool column=gweni_cast<BoardColumn>(child) != nullptr;
    ParentClass::removeChild(child);
    if(column)
        child->setPlacement(Placement::Parent);
}

Size Board::contentSize() const
{
    int width=0;
    countLayoutChildrenScanned(m_innerPanel->getActualChildren());
    for(Base *child:m_innerPanel->getActualChildren())
        if(!child->hidden() && !child->isAttached() && gweni_cast<BoardColumn>(child))
            width+=(width?m_gap:0)+m_columnWidth;
    return Size(width, 0);
}

void Board::contentsPlaced(skin::Base * /*skin*/)
{
    int x=0;
    countLayoutChildrenScanned(m_innerPanel->getActualChildren());
    for(Base *child:m_innerPanel->getActualChildren())
        if(!child->hidden() && !child->isAttached() && gweni_cast<BoardColumn>(child))
        {
            placeChild(child, x, 0, m_columnWidth, getViewportHeight());
            x+=m_columnWidth+m_gap;
        }
}
