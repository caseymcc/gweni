/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */


#include <gweni/controls/layout/table.h>

#include <algorithm>
#include <climits>

namespace gweni
{
namespace controls
{
namespace layout
{

bool Table::hitRowsSearchable()
{
    if(!m_hitRowsStale)
        return m_hitRowsSorted;

    m_hitRowsStale=false;
    m_hitRows.clear();
    m_hitRowsSorted=true;

    int end=INT_MIN;

    for(Base *child : m_children)
    {
        if(child->getDock() != DockPosition::Top)
        {
            m_hitRowsSorted=false;
            break;
        }

        // the base walk gets nullptr from these wherever the point is
        const Rect &bounds=child->getBounds();

        if(child->hidden() || bounds.w <= 0 || bounds.h <= 0)
            continue;

        if(bounds.y < end)
        {
            m_hitRowsSorted=false;
            break;
        }

        end=bounds.y+bounds.h;
        m_hitRows.push_back(child);
    }

    if(!m_hitRowsSorted)
        m_hitRows.clear();

    return m_hitRowsSorted;
}

Base *Table::getControlAt(int x, int y, bool bOnlyIfMouseEnabled)
{
    countHitTestVisit();

    if(hidden())
        return nullptr;

    if(x < 0 || y < 0 || x >= getWidth() || y >= getHeight())
        return nullptr;

    if(!hitRowsSearchable())
        return getControlAtWalk(x, y, bOnlyIfMouseEnabled);

    // The rows do not overlap, so only the last one starting at or above y
    // can hold the point; every other row gives the base walk nullptr.
    auto after=std::upper_bound(m_hitRows.begin(), m_hitRows.end(), y,
        [](int py, const Base *row){ return py < row->getY(); });

    if(after != m_hitRows.begin())
    {
        Base *row=*(after-1);

        if(Base *found=row->getControlAt(x-row->getX(), y-row->getY(), bOnlyIfMouseEnabled))
            return found;
    }

    if(bOnlyIfMouseEnabled && !getMouseInputEnabled())
        return nullptr;

    return this;
}

}//namespace layout
}//namespace controls
}//namespace gweni
