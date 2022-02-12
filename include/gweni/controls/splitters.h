/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_splitters_h_
#define _gweni_controls_splitters_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT SplitterVertical: public controls::Base
{
    GWENI_CONTROL_INLINE(SplitterVertical, controls::Base)
    {
        m_panels[0]=new controls::Base(this);
        m_panels[1]=new controls::Base(this);
        m_splitter=new controls::SplitterBar(this);
        m_splitterSize=6;
        m_splitter->setPos(0, 100);
        m_splitter->setCursor(gweni::cursor::SizeNS);
        m_splitter->onDragged.add(this, &ThisClass::onSplitterMoved);
        setScaling(false, 100);
    }

public:
    void postLayout(skin::Base *skin) override
    {
        refreshContainers();
    }

    void setPanels(controls::Base *pA, controls::Base *pB)
    {
        if(pA)
            pA->setParent(m_panels[0]);

        if(pB)
            pB->setParent(m_panels[1]);
    }

    void setScaling(bool Right, int iSize)
    {
        m_rightSided=Right;
        m_size=iSize;
    }

    virtual int splitterPos()
    {
        return m_splitter->getY();
    }

protected:

    virtual void refreshContainers()
    {
        const gweni::Rect &inner=getInnerBounds();
        int offset=m_size;

        if(m_rightSided)
            offset=getHeight()-m_size;

        m_splitter->setSize(inner.w, m_splitterSize);
        m_splitter->setPos(0, offset);
        m_panels[0]->setPos(inner.x, inner.y);
        m_panels[0]->setSize(inner.w, offset);
        m_panels[1]->setPos(inner.x, (offset+m_splitterSize));
        m_panels[1]->setSize(inner.w, (inner.y+inner.h)-(offset+m_splitterSize));
    }

    virtual void onSplitterMoved(event::Info)
    {
        if(m_rightSided)
            m_size=getHeight()-m_splitter->getY();
        else
            m_size=m_splitter->getY();

        refreshContainers();
        invalidate();
    }

    controls::Base *m_panels[2];
    bool m_rightSided;
    int m_size;
    controls::SplitterBar *m_splitter;
    unsigned int m_splitterSize;
};


class GWENI_EXPORT SplitterHorizontal: public SplitterVertical
{
    GWENI_CONTROL_INLINE(SplitterHorizontal, SplitterVertical)
    {
        m_splitter->setCursor(gweni::cursor::SizeWE);
        m_splitter->setPos(100, 0);
    }

public:
    void refreshContainers() override
    {
        const gweni::Rect &inner=getInnerBounds();
        int offset=m_size;

        if(m_rightSided)
            offset=getWidth()-m_size;

        m_splitter->setSize(m_splitterSize, inner.h);
        m_splitter->setPos(offset, 0);
        m_panels[0]->setPos(inner.x, inner.y);
        m_panels[0]->setSize(offset, inner.h);
        m_panels[1]->setPos((offset+m_splitterSize), inner.y);
        m_panels[1]->setSize((inner.x+inner.w)-(offset+m_splitterSize), inner.h);
    }

    void onSplitterMoved(event::Info) override
    {
        if(m_rightSided)
            m_size=getWidth()-m_splitter->getX();
        else
            m_size=m_splitter->getX();

        refreshContainers();
        invalidate();
    }

    int splitterPos() override
    {
        return m_splitter->getX();
    }

};


}
}

#endif//_gweni_controls_splitters_h_
