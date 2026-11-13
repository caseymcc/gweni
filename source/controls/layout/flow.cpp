/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/layout/flow.h>

#include <algorithm>

namespace gweni
{
namespace controls
{
namespace layout
{

namespace
{

//! A child the Flow lays out as a card: shown, and not attached to a corner
//! of the Flow (Base places those).
bool isCard(const Base *child)
{
    return !child->hidden() && !child->isAttached();
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(Flow)
{
    setDock(DockPosition::Top);
}

void Flow::setGap(int gap)
{
    gap=std::max(gap, 0);
    if(gap == m_gap)
        return;

    m_gap=gap;
    invalidate();
}

void Flow::setMinCardWidth(int width)
{
    width=std::max(width, 1);
    if(width == m_minCardWidth)
        return;

    m_minCardWidth=width;
    invalidate();
}

void Flow::setMaxColumns(int columns)
{
    columns=std::max(columns, 1);
    if(columns == m_maxColumns)
        return;

    m_maxColumns=columns;
    invalidate();
}

void Flow::setBreakpoints(std::vector<Breakpoint> breakpoints)
{
    std::stable_sort(breakpoints.begin(), breakpoints.end(),
        [](const Breakpoint &a, const Breakpoint &b) { return a.below < b.below; });

    m_breakpoints=std::move(breakpoints);
    invalidate();
}

int Flow::columnsFor(int width, int contentWidth) const
{
    if(!m_breakpoints.empty())
    {
        for(const Breakpoint &breakpoint:m_breakpoints)
        {
            if(width < breakpoint.below)
                return std::max(breakpoint.columns, 1);
        }
        return m_maxColumns;
    }

    const int fit=(std::max(contentWidth, 0)+m_gap)/(m_minCardWidth+m_gap);

    return std::min(std::max(fit, 1), m_maxColumns);
}

void Flow::columnSpan(int contentWidth, int columns, int gap, int column, int &x, int &w)
{
    columns=std::max(columns, 1);

    const int each=std::max(0, contentWidth-(columns-1)*gap)/columns;

    x=column*(each+gap);
    w=(column == columns-1)?std::max(0, contentWidth-x):each;
}

int Flow::laidOutColumns() const
{
    if(m_columns > 0)
        return m_columns;

    const Padding &padding=getPadding();

    return columnsFor(getWidth(), getWidth()-padding.left-padding.right);
}

int Flow::rowHeight(size_t row, const Base *card) const
{
    return (row < m_rowHeights.size())?m_rowHeights[row]:card->getPreferredSize().height;
}

void Flow::calculateSize(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        // the cards are measured for their own subtrees; none counts
        ParentClass::calculateSize(skin, dim);
        m_preferredSize.width=preferredFromContent(m_minCardWidth+padding.left+padding.right, Dim::X);
        return;
    }

    if(inLayoutPass() && !laidOutThisPass(Dim::X))
        return;

    ParentClass::calculateSize(skin, dim);

    const int columns=laidOutColumns();
    int content=0;
    int index=0;

    m_rowHeights.clear();
    for(const Base *child:m_children)
    {
        if(!isCard(child))
            continue;

        if(index%columns == 0)
            m_rowHeights.push_back(0);
        m_rowHeights.back()=std::max(m_rowHeights.back(), child->getPreferredSize().height);
        ++index;
    }

    for(size_t row=0; row < m_rowHeights.size(); ++row)
        content+=m_rowHeights[row]+((row > 0)?m_gap:0);

    m_preferredSize.height=preferredFromContent(content+padding.top+padding.bottom, Dim::Y);
}

void Flow::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        const int contentWidth=std::max(0, getWidth()-padding.left-padding.right);

        m_columns=columnsFor(getWidth(), contentWidth);

        int index=0;

        for(Base *child:m_children)
        {
            if(!isCard(child))
                continue;

            int x, w;

            columnSpan(contentWidth, m_columns, m_gap, index%m_columns, x, w);
            placeChild(child, padding.left+x, child->getY(), w, child->getHeight());
            ++index;
        }
    }
    else
    {
        const int columns=laidOutColumns();
        int index=0;
        int y=padding.top;

        for(Base *child:m_children)
        {
            if(!isCard(child))
                continue;

            const size_t row=static_cast<size_t>(index/columns);

            if(index > 0 && index%columns == 0)
                y+=rowHeight(row-1, child)+m_gap;

            const int height=rowHeight(row, child);

            placeChild(child, child->getX(), y, child->getWidth(), height);
            ++index;
        }
    }

    ParentClass::arrange(skin, dim);
}

void Flow::onChildAdded(Base *child)
{
    if(!child->isAttached())
        child->setPlacement(Placement::Owner);
    ParentClass::onChildAdded(child);
}

void Flow::onChildRemoved(Base *child)
{
    if(!child->isAttached())
        child->setPlacement(Placement::Parent);
    ParentClass::onChildRemoved(child);
}

}//namespace layout
}//namespace controls
}//namespace gweni
