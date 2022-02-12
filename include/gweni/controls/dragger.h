/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dragger_h_
#define _gweni_controls_dragger_h_

#include <gweni/controls/base.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{
namespace internal
{

class GWENI_EXPORT Dragger: public controls::Base
{
public:

    GWENI_CONTROL(Dragger, controls::Base);

public:
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;

    void onMouseClickLeft(int x, int y, bool down) override;
    //void render(skin::Base *skin) override;

    virtual void setTarget(controls::Base *base)
    {
        m_target=base;
    }

    virtual bool isDepressed() const
    {
        return m_depressed;
    }

    virtual void setDoMove(bool b)
    {
        m_doMove=b;
    }

    void onMouseDoubleClickLeft(int x, int y) override;

    gweni::event::Caller onDragged;
    gweni::event::Caller onDragStart;

    gweni::event::Caller onDoubleClickLeft;

protected:

    bool                m_depressed;
    gweni::Point          m_holdPos;
    controls::Base *m_target;
    bool                m_doMove;
};


}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_dragger_h_
