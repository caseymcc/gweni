/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_horizontalscrollbar_h_
#define _gweni_controls_horizontalscrollbar_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/dragger.h>
#include <gweni/controls/scrollBar.h>

namespace gweni
{
namespace controls
{

class GWENI_EXPORT HorizontalScrollBar: public BaseScrollBar
{
public:

    GWENI_CONTROL(HorizontalScrollBar, BaseScrollBar);

    void layout(skin::Base *skin) override;

    void onMouseClickLeft(int x, int y, bool down) override;
    void onBarMoved(event::Info) override;

    int getBarSize() override
    {
        return m_bar->getWidth();
    }

    int getBarPos() override
    {
        return m_bar->getX()-getHeight();
    }

    void setBarSize(int size) override
    {
        m_bar->setWidth(size);
    }

    int getButtonSize() override
    {
        return getHeight();
    }

    void  scrollToLeft() override;
    void  scrollToRight() override;
    virtual void  nudgeLeft(event::Info);
    virtual void  nudgeRight(event::Info);
    float getNudgeAmount() override;

    float calculateScrolledAmount() override;
    bool  setScrolledAmount(float amount, bool forceUpdate) override;

    bool isHorizontal() override
    {
        return true;
    }
};


}//namespace controls
}//namespace gweni

#endif//_gweni_controls_horizontalscrollbar_h_
