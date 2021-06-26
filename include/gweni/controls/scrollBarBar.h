/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_scrollbarbar_h_
#define _gweni_controls_scrollbarbar_h_

#include <gweni/controls/dragger.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{
namespace internal
{

class GWENI_EXPORT ScrollBarBar: public internal::Dragger
{
public:

    GWENI_CONTROL(ScrollBarBar, internal::Dragger);

    void render(skin::Base *skin) override;
    void layout(skin::Base *skin) override;

    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseClickLeft(int x, int y, bool down) override;

    void moveTo(int x, int y) override;

    virtual void setHorizontal()
    {
        m_horizontal=true;
    }

    virtual void setVertical()
    {
        m_horizontal=false;
    }

    virtual bool isVertical()
    {
        return !m_horizontal;
    }

    virtual bool isHorizontal()
    {
        return m_horizontal;
    }

    bool isDepressed() const override
    {
        return m_depressed;
    }

protected:

    bool m_horizontal;

};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_scrollbarbar_h_
