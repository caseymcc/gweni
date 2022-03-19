/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/controls/crossSplitter.h>
#include <gweni/controls/button.h>
#include <gweni/platforms/commonPlatform.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(CrossSplitter)
{
    m_verticalSplitter=newChild<SplitterBar>();

    m_verticalSplitter->setPos(0, 128);
    m_verticalSplitter->onDragged.add(this, &CrossSplitter::onVerticalMoved);
    m_verticalSplitter->setCursor(gweni::cursor::SizeNS);

    m_horizontalSplitter=newChild<SplitterBar>();

    m_horizontalSplitter->setPos(128, 0);
    m_horizontalSplitter->onDragged.add(this, &CrossSplitter::onHorizontalMoved);
    m_horizontalSplitter->setCursor(gweni::cursor::SizeWE);

    m_centerSplitter=newChild<SplitterBar>();

    m_centerSplitter->setPos(128, 128);
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

void CrossSplitter::updateVSplitter()
{
    m_verticalSplitter->moveTo(m_verticalSplitter->getX(), (getHeight()-m_verticalSplitter->getHeight())*(m_vertical));
}

void CrossSplitter::updateHSplitter()
{
    m_horizontalSplitter->moveTo((getWidth()-m_horizontalSplitter->getWidth())*(m_horizontal), m_horizontalSplitter->getY());
}

void CrossSplitter::onCenterMoved(event::Info)
{
    // Move the other two bars into position
    calculateValueCenter();
    invalidate();
}

void CrossSplitter::updateCSplitter()
{
    m_centerSplitter->moveTo((getWidth()-m_centerSplitter->getWidth())*(m_horizontal),
        (getHeight()-m_centerSplitter->getHeight())*(m_vertical));
}

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
    m_horizontal=static_cast<float>(m_centerSplitter->getX()) / (getWidth() - m_centerSplitter->getWidth());
    m_vertical=static_cast<float>(m_centerSplitter->getY()) / (getHeight() - m_centerSplitter->getHeight());
}

float CrossSplitter::calculateValueHorizontal()
{
    return static_cast<float>(m_horizontalSplitter->getX()) / (getWidth() - m_horizontalSplitter->getWidth());
}

float CrossSplitter::calculateValueVertical()
{
    return static_cast<float>(m_verticalSplitter->getY()) / (getHeight() - m_verticalSplitter->getHeight());
}

void CrossSplitter::layout(skin::Base * /*skin*/)
{
    m_verticalSplitter->setSize(getWidth(), m_barSize);
    m_horizontalSplitter->setSize(m_barSize, getHeight());
    m_centerSplitter->setSize(m_barSize, m_barSize);
    updateVSplitter();
    updateHSplitter();
    updateCSplitter();

    if(m_zoomedSection == -1)
    {
        if(m_sections[0])
        {
            m_sections[0]->setBounds(0,
                0,
                m_horizontalSplitter->getX(),
                m_verticalSplitter->getY());
        }

        if(m_sections[1])
        {
            m_sections[1]->setBounds(m_horizontalSplitter->getX()+m_barSize,
                0,
                getWidth()-(m_horizontalSplitter->getX()+m_barSize),
                m_verticalSplitter->getY());
        }

        if(m_sections[2])
        {
            m_sections[2]->setBounds(0,
                m_verticalSplitter->getY()+m_barSize,
                m_horizontalSplitter->getX(),
                getHeight()-(m_verticalSplitter->getY()+m_barSize));
        }

        if(m_sections[3])
        {
            m_sections[3]->setBounds(m_horizontalSplitter->getX()+m_barSize,
                m_verticalSplitter->getY()+m_barSize,
                getWidth()-(m_horizontalSplitter->getX()+m_barSize),
                getHeight()-(m_verticalSplitter->getY()+m_barSize));
        }
    }
    else
    {
        // This should probably use Fill docking instead
        m_sections[m_zoomedSection]->setBounds(0, 0, getWidth(), getHeight());
    }
}

void CrossSplitter::setPanel(int index, controls::Base *panel)
{
    GWENI_ASSERT_MSG(index >= 0 && index <= 3, "CrossSplitter::setPanel out of range");
    m_sections[index]=panel;

    if(panel)
    {
        panel->setDock(DockPosition::None);
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

    invalidate();
    zoomChanged();
}

}//namespace controls
}//namespace gweni

