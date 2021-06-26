/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/imagePanel.h>
#include <gweni/controls/label.h>
#include <gweni/controls/resizableControl.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(ResizableControl)
{
    m_resizable=true;
    m_minimumSize=Size(5, 5);
    m_clampMovement=false;
    m_sizeFlags={SizeFlag::Fixed, SizeFlag::Fixed};

    // bottom
    m_resizers[0]=new internal::Resizer(this);
    m_resizers[0]->dock(Position::Bottom);
    m_resizers[0]->setResizeDirection(Position::Bottom);
    m_resizers[0]->setTarget(this);
    m_resizers[0]->onResize.add(this, &ResizableControl::onResizedInternal);

    // bottom left
    m_resizers[1]=new internal::Resizer(m_resizers[0]);
    m_resizers[1]->dock(Position::Left);
    m_resizers[1]->setResizeDirection(Position::Bottom|Position::Left);
    m_resizers[1]->setTarget(this);
    m_resizers[1]->onResize.add(this, &ResizableControl::onResizedInternal);

    // bottom right
    m_resizers[2]=new internal::Resizer(m_resizers[0]);
    m_resizers[2]->dock(Position::Right);
    m_resizers[2]->setResizeDirection(Position::Bottom|Position::Right);
    m_resizers[2]->setTarget(this);
    m_resizers[2]->onResize.add(this, &ResizableControl::onResizedInternal);

    // top
    m_resizers[3]=new internal::Resizer(this);
    m_resizers[3]->dock(Position::Top);
    m_resizers[3]->setResizeDirection(Position::Top);
    m_resizers[3]->setTarget(this);
    m_resizers[3]->onResize.add(this, &ResizableControl::onResizedInternal);

    // top left
    m_resizers[4]=new internal::Resizer(m_resizers[3]);
    m_resizers[4]->dock(Position::Left);
    m_resizers[4]->setResizeDirection(Position::Top|Position::Left);
    m_resizers[4]->setTarget(this);
    m_resizers[4]->onResize.add(this, &ResizableControl::onResizedInternal);

    // top right
    m_resizers[5]=new internal::Resizer(m_resizers[3]);
    m_resizers[5]->dock(Position::Right);
    m_resizers[5]->setResizeDirection(Position::Top|Position::Right);
    m_resizers[5]->setTarget(this);
    m_resizers[5]->onResize.add(this, &ResizableControl::onResizedInternal);

    // left
    m_resizers[6]=new internal::Resizer(this);
    m_resizers[6]->dock(Position::Left);
    m_resizers[6]->setResizeDirection(Position::Left);
    m_resizers[6]->setTarget(this);
    m_resizers[6]->onResize.add(this, &ResizableControl::onResizedInternal);

    // right
    m_resizers[7]=new internal::Resizer(this);
    m_resizers[7]->dock(Position::Right);
    m_resizers[7]->setResizeDirection(Position::Right);
    m_resizers[7]->setTarget(this);
    m_resizers[7]->onResize.add(this, &ResizableControl::onResizedInternal);
}

void ResizableControl::disableResizing()
{
    for(auto &&resizer : m_resizers)
    {
        resizer->setDisabled(true);
    }
}

bool ResizableControl::setBounds(int x, int y, int w, int h)
{
    Size minSize=getMinimumSize();

    // Clamp Minimum Size
    w=std::max(w, minSize.width);
    h=std::max(h, minSize.height);

    // Clamp to parent's window
    Base *parent=getParent();

    if(parent && m_clampMovement)
    {
        if(x+w > parent->getWidth())
            x=parent->getWidth()-w;

        if(x < 0)
            x=0;

        if(y+h > parent->getHeight())
            y=parent->getHeight()-h;

        if(y < 0)
            y=0;
    }

    return ParentClass::setBounds(x, y, w, h);
}

void ResizableControl::onResizedInternal(event::Info)
{
    if(!isDisabled())
    {
        onResize.call(this);
        onResized();
    }
}

}//namespace controls
}//namespace gweni
