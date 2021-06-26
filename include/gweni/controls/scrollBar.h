/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_scrollbar_h_
#define _gweni_controls_scrollbar_h_

#include <gweni/controls/base.h>
#include <gweni/controls/scrollBarBar.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/gweni.h>


#define SCROLL_BUTTON_UP 0
#define SCROLL_BUTTON_LEFT 0
#define SCROLL_BUTTON_DOWN 1
#define SCROLL_BUTTON_RIGHT 1
#define NUDGE_DIST 10

namespace gweni
{
namespace controls
{

class GWENI_EXPORT BaseScrollBar: public Base
{
public:

    GWENI_CONTROL(BaseScrollBar, Base);

    void render(skin::Base *skin) override;

    virtual void setBarSize(int size)=0;
    virtual int  getBarSize()=0;
    virtual int  getBarPos()=0;

    virtual void onBarMoved(event::Info);
    void onMouseClickLeft(int /*x*/, int /*y*/, bool /*down*/) override {}

    virtual void scrollToLeft() {}
    virtual void scrollToRight() {}
    virtual void scrollToTop() {}
    virtual void scrollToBottom() {}

    virtual float getNudgeAmount()
    {
        return m_nudgeAmount/m_contentSize;
    }

    virtual void setNudgeAmount(float nudge)
    {
        m_nudgeAmount=nudge;
    }

    virtual void barMovedNotification();

    virtual float calculateScrolledAmount()
    {
        return 0;
    }

    virtual int calculateBarSize()
    {
        return 0;
    }

    virtual bool setScrolledAmount(float amount, bool forceUpdate);

    virtual void setContentSize(float size);
    virtual void setViewableContentSize(float size);

    virtual int getButtonSize()
    {
        return 0;
    }

    virtual float getScrolledAmount()
    {
        return m_scrolledAmount;
    }

    virtual bool isHorizontal()
    {
        return false;
    }

    event::Caller onBarMovedCaller;

protected:

    internal::ScrollBarButton *m_scrollButton[2];
    internal::ScrollBarBar *m_bar;

    bool m_depressed;
    float m_scrolledAmount;
    float m_contentSize;
    float m_viewableContentSize;
    float m_nudgeAmount;
};


}//namespace controls
}//namespace gweni

#endif//_gweni_controls_scrollbar_h_
