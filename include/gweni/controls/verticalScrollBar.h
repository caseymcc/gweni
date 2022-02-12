/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_verticalscrollbar_h_
#define _gweni_controls_verticalscrollbar_h_
#include <gweni/controls/base.h>
#include <gweni/controls/scrollBar.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{

class GWENI_EXPORT VerticalScrollBar: public BaseScrollBar
{
    GWENI_CONTROL(VerticalScrollBar, BaseScrollBar);

public:
    void layout(skin::Base *skin) override;

    void onMouseClickLeft(int x, int y, bool down) override;
    void onBarMoved(event::Info) override;

    int getBarSize() override
    {
        return m_bar->getHeight();
    }

    int getBarPos() override
    {
        return m_bar->getY()-getWidth();
    }

    void setBarSize(int size) override
    {
        m_bar->setHeight(size);
    }

    int getButtonSize() override
    {
        return getWidth();
    }

    void  scrollToTop() override;
    void  scrollToBottom() override;
    virtual void  nudgeUp(event::Info);
    virtual void  nudgeDown(event::Info);
    float getNudgeAmount() override;

    float calculateScrolledAmount() override;
    bool  setScrolledAmount(float amount, bool forceUpdate) override;
};


}//namespace controls
}//namespace gweni

#endif//_gweni_controls_verticalscrollbar_h_
