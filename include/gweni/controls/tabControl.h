/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_tabcontrol_h_
#define _gweni_controls_tabcontrol_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/controls/tabButton.h>
#include <gweni/controls/tabStrip.h>
#include <gweni/controls/tabTitleBar.h>

namespace gweni
{
namespace controls
{
namespace internal
{
class ScrollBarButton;
}//namespace internal

class GWENI_EXPORT TabControl: public Base
{
    GWENI_CONTROL(TabControl, Base);

public:
    virtual TabButton *addPage(const String text, controls::Base *page=nullptr);

    virtual void addPage(TabButton *button);
    virtual void removePage(TabButton *button);

    virtual int tabCount(void);
    virtual TabButton *getTab(int num);
    virtual TabButton *getCurrentButton()
    {
        return m_currentButton;
    }

    virtual TabStrip *getTabStrip()
    {
        return m_tabStrip;
    }

    virtual void setTabStripPosition(DockPosition dockSide);

    virtual bool doesAllowDrag();

    virtual void setAllowReorder(bool b)
    {
        getTabStrip()->setAllowReorder(b);
    }

    gweni::event::Caller onLoseTabCaller;
    gweni::event::Caller onAddTabCaller;

    virtual void onTabPressed(event::Info info);
    virtual void onLoseTab(TabButton *button);

private:

    void postLayout(skin::Base *skin) override;
    void handleOverflow();

    void onScrollPressLeft(event::Info);
    void onScrollPressRight(event::Info);

    TabStrip *m_tabStrip;
    TabButton *m_currentButton;

    internal::ScrollBarButton *m_scroll[2];
    int m_scrollOffset;


};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_tabcontrol_h_
