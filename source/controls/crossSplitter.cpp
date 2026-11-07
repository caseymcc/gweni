/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/controls/crossSplitter.h>
#include <gweni/controls/button.h>
#include <gweni/platforms/commonPlatform.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(CrossSplitter)
{
    // layout() places the bars from the split fractions, in this control's
    // own coordinates; restrictToParent would clamp them against its margin.
    // They and the panels are this control's to place (Placement::Owner).
    m_verticalSplitter=newChild<SplitterBar>();

    m_verticalSplitter->setPlacement(Placement::Owner);
    m_verticalSplitter->setPos(0, 128);
    m_verticalSplitter->restrictToParent(false);
    m_verticalSplitter->onDragged.add(this, &CrossSplitter::onVerticalMoved);
    m_verticalSplitter->setCursor(gweni::cursor::SizeNS);

    m_horizontalSplitter=newChild<SplitterBar>();

    m_horizontalSplitter->setPlacement(Placement::Owner);
    m_horizontalSplitter->setPos(128, 0);
    m_horizontalSplitter->restrictToParent(false);
    m_horizontalSplitter->onDragged.add(this, &CrossSplitter::onHorizontalMoved);
    m_horizontalSplitter->setCursor(gweni::cursor::SizeWE);

    m_centerSplitter=newChild<SplitterBar>();

    m_centerSplitter->setPlacement(Placement::Owner);
    m_centerSplitter->setPos(128, 128);
    m_centerSplitter->restrictToParent(false);
    m_centerSplitter->onDragged.add(this, &CrossSplitter::onCenterMoved);
    m_centerSplitter->setCursor(gweni::cursor::SizeAll);
    m_horizontal=0.5f;
    m_vertical=0.5f;
    setPanel(0, nullptr);
    setPanel(1, nullptr);
    setPanel(2, nullptr);
    setPanel(3, nullptr);
    setSplitterSize(5);
    m_zoomedSection=-1;
}

namespace
{

//! The position of a bar \p bar wide along an extent \p extent long at
//! \p fraction of the room it has: round((extent-bar)*fraction), never
//! negative.
int barPosition(int extent, int bar, float fraction)
{
    const int room=extent-bar;

    if(room <= 0)
        return 0;
    return std::clamp(static_cast<int>(std::lround(room*fraction)), 0, room);
}

//! The fraction of the room a bar at \p pos has, clamped to [0, 1], or
//! \p current when there is no room (the extent is no bigger than the bar).
float barFraction(int pos, int extent, int bar, float current)
{
    const int room=extent-bar;

    if(room <= 0)
        return current;
    return static_cast<float>(std::clamp(pos, 0, room))/room;
}

}//namespace

int CrossSplitter::barX() const
{
    return barPosition(getWidth(), m_barSize, m_horizontal);
}

int CrossSplitter::barY() const
{
    return barPosition(getHeight(), m_barSize, m_vertical);
}

void CrossSplitter::updateVSplitter()
{
    placeChild(m_verticalSplitter, 0, barY(), getWidth(), m_barSize);
}

void CrossSplitter::updateHSplitter()
{
    placeChild(m_horizontalSplitter, barX(), 0, m_barSize, getHeight());
}

void CrossSplitter::onCenterMoved(event::Info)
{
    // Move the other two bars into position
    calculateValueCenter();
    invalidate();
}

void CrossSplitter::updateCSplitter()
{
    placeChild(m_centerSplitter, barX(), barY(), m_barSize, m_barSize);
}

// The Dragger has moved the bar to the mouse, in both axes and unclamped: the
// fraction is taken from the bar's own axis, clamped to the room it has, and
// layout() puts the bar back on the line the fraction gives.
void CrossSplitter::onHorizontalMoved(event::Info)
{
    m_horizontal=calculateValueHorizontal();
    invalidate();
}

void CrossSplitter::onVerticalMoved(event::Info)
{
    m_vertical=calculateValueVertical();
    invalidate();
}

void CrossSplitter::calculateValueCenter()
{
    m_horizontal=barFraction(m_centerSplitter->getX(), getWidth(), m_barSize, m_horizontal);
    m_vertical=barFraction(m_centerSplitter->getY(), getHeight(), m_barSize, m_vertical);
}

float CrossSplitter::calculateValueHorizontal()
{
    return barFraction(m_horizontalSplitter->getX(), getWidth(), m_barSize, m_horizontal);
}

float CrossSplitter::calculateValueVertical()
{
    return barFraction(m_verticalSplitter->getY(), getHeight(), m_barSize, m_vertical);
}

void CrossSplitter::layout(skin::Base * /*skin*/)
{
    updateVSplitter();
    updateHSplitter();
    updateCSplitter();

    if(m_zoomedSection == -1)
    {
        const int x=barX();
        const int y=barY();
        const int right=x+m_barSize;
        const int bottom=y+m_barSize;
        const int rightWidth=std::max(0, getWidth()-right);
        const int bottomHeight=std::max(0, getHeight()-bottom);

        if(m_sections[0])
            placeChild(m_sections[0], 0, 0, x, y);

        if(m_sections[1])
            placeChild(m_sections[1], right, 0, rightWidth, y);

        if(m_sections[2])
            placeChild(m_sections[2], 0, bottom, x, bottomHeight);

        if(m_sections[3])
            placeChild(m_sections[3], right, bottom, rightWidth, bottomHeight);
    }
    else
    {
        // This should probably use Fill docking instead
        placeChild(m_sections[m_zoomedSection], 0, 0, getWidth(), getHeight());
    }
}

void CrossSplitter::setPanel(int index, controls::Base *panel)
{
    GWENI_ASSERT_MSG(index >= 0 && index <= 3, "CrossSplitter::setPanel out of range");

    // A panel taken out is the generic arrange's again, if it stays here.
    if(m_sections[index] && m_sections[index] != panel)
        m_sections[index]->setPlacement(Placement::Parent);

    m_sections[index]=panel;

    if(panel)
    {
        panel->setDock(DockPosition::None);
        panel->setPlacement(Placement::Owner);
        panel->setParent(this);
    }

    invalidate();
}

controls::Base *CrossSplitter::getPanel(int i)
{
    return m_sections[i];
}

void CrossSplitter::zoomChanged()
{
    onZoomChange.call(this);

    if(m_zoomedSection == -1)
        onUnZoomed.call(this);
    else
        onZoomed.call(this);
}

void CrossSplitter::setBarsHidden(bool hidden)
{
    m_verticalSplitter->setHidden(hidden);
    m_horizontalSplitter->setHidden(hidden);
    m_centerSplitter->setHidden(hidden);
}

void CrossSplitter::zoom(int section)
{
    unZoom();

    if(m_sections[section])
    {
        for(int i=0; i < 4; i++)
        {
            if(i != section && m_sections[i])
                m_sections[i]->setHidden(true);
        }

        // the bars would lie under the zoomed panel and catch its edges
        setBarsHidden(true);
        m_zoomedSection=section;
        invalidate();
    }

    zoomChanged();
}

void CrossSplitter::unZoom()
{
    m_zoomedSection=-1;

    for(int i=0; i < 4; i++)
    {
        if(m_sections[i])
            m_sections[i]->setHidden(false);
    }

    setBarsHidden(false);
    invalidate();
    zoomChanged();
}

}//namespace controls
}//namespace gweni

