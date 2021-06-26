/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/highlight.h>
#include <gweni/dragAndDrop.h>
#include <gweni/controls/windowControl.h>
#include <gweni/controls/scrollBarButton.h>


namespace gweni
{
namespace controls
{

template <typename T, typename T2>
inline T Approach(T current, T fTarget, T2 delta)
{
    if(current < fTarget)
    {
        current+=delta;

        if(current > fTarget)
            return fTarget;
    }
    else if(current > fTarget)
    {
        current-=delta;

        if(current < fTarget)
            return fTarget;
    }

    return current;
}

class TabControlInner: public Base
{
public:

    GWENI_CONTROL_INLINE(TabControlInner, Base)
    {}

    void render(skin::Base *skin) override
    {
//        skin->drawTabControl(this, skin::Generate);
        skin->drawControl(this);
    }

};


GWENI_CONTROL_CONSTRUCTOR(TabControl)
{
    m_scrollOffset=0;
    m_currentButton=nullptr;
    m_tabStrip=new TabStrip(this);
    m_tabStrip->setTabPosition(Position::Top);
    // Make this some special control?
    m_scroll[0]=new internal::ScrollBarButton(this);
    m_scroll[0]->setDirectionLeft();
    m_scroll[0]->onPressCaller.add(this, &TabControl::onScrollPressLeft);
    m_scroll[0]->setSize(14, 14);
    m_scroll[1]=new internal::ScrollBarButton(this);
    m_scroll[1]->setDirectionRight();
    m_scroll[1]->onPressCaller.add(this, &TabControl::onScrollPressRight);
    m_scroll[1]->setSize(14, 14);
    m_innerPanel=new TabControlInner(this);
    m_innerPanel->dock(Position::Fill);
    m_innerPanel->sendToBack();
    setTabable(false);
}

TabButton *TabControl::addPage(String text, controls::Base *page)
{
    if(!page)
        page=new Base(this);
    else
        page->setParent(this);

    TabButton *button=new TabButton(m_tabStrip);
    button->setText(text);
    button->setPage(page);
    button->setTabable(false);
    addPage(button);
    return button;
}

void TabControl::removePage(TabButton *button)
{
    button->setParent(getCanvas());
    onLoseTab(button);
}

void TabControl::addPage(TabButton *button)
{
    Base *page=button->getPage();
    page->setParent(this);
    page->setHidden(true);
    page->setMargin(Margin(6, 6, 6, 6));
    page->dock(Position::Fill);
    button->setParent(m_tabStrip);
    button->dock(Position::Left);
    button->sizeToContents();

    if(button->getTabControl())
        button->onPressCaller.removeHandler(button->getTabControl());

    button->setTabControl(this);
    button->onPressCaller.add(this, &TabControl::onTabPressed);

    if(!m_currentButton)
        button->onPress(event::Info(this));

    onAddTabCaller.call(this);
    invalidate();
}

void TabControl::onTabPressed(event::Info info)
{
    TabButton *button=gweni_cast<TabButton>(info.controlCaller);

    if(!button)
        return;

    Base *page=button->getPage();

    if(!page)
        return;

    if(m_currentButton == button)
        return;

    if(m_currentButton)
    {
        Base *subpage=m_currentButton->getPage();

        if(subpage)
            subpage->setHidden(true);

        m_currentButton->redraw();
        m_currentButton=nullptr;
    }

    m_currentButton=button;
    page->setHidden(false);
    m_tabStrip->invalidate();
    invalidate();
}

void TabControl::postLayout(skin::Base *skin)
{
    ParentClass::postLayout(skin);
    handleOverflow();
}

void TabControl::onLoseTab(TabButton *button)
{
    if(m_currentButton == button)
        m_currentButton=nullptr;

    // TODO: select a tab if any exist.
    onLoseTabCaller.call(this);
    invalidate();
}

int TabControl::tabCount(void)
{
    return m_tabStrip->numChildren();
}

TabButton *TabControl::getTab(int num)
{
    return gweni_cast<TabButton>(m_tabStrip->getChild(num));
}

void TabControl::setTabStripPosition(Position dock)
{
    m_tabStrip->setTabPosition(dock);
}

bool TabControl::doesAllowDrag()
{
    return m_tabStrip->allowTabReorder();
}

void TabControl::handleOverflow()
{
    gweni::Point TabsSize=m_tabStrip->childrenSize();
    // Only enable the scrollers if the tabs are at the top.
    // This is a limitation we should explore.
    // Really TabControl should have derivitives for tabs placed elsewhere where
    // we could specialize
    // some functions like this for each direction.
    bool bNeeded=TabsSize.x > getWidth() && m_tabStrip->getDock() == Position::Top;
    m_scroll[0]->setHidden(!bNeeded);
    m_scroll[1]->setHidden(!bNeeded);

    if(!bNeeded)
        return;

    m_scrollOffset=gweni::clamp(m_scrollOffset, 0, TabsSize.x-getWidth()+32);
#if 0
    //
    // This isn't frame rate independent.
    // Could be better. Get rid of m_scrollOffset and just use
    // m_tabStrip->getMargin().left ?
    // Then get a margin animation type and do it properly!
    // TODO!
    //
    m_tabStrip->setMargin(Margin(Approach(m_tabStrip->getMargin().left, m_scrollOffset *-1,
        2), 0, 0, 0));
    invalidateParent();
#else // if 0
    m_tabStrip->setMargin(Margin(m_scrollOffset *-1, 0, 0, 0));
#endif // if 0
    m_scroll[0]->setPos(getWidth()-30, 5);
    m_scroll[1]->setPos(m_scroll[0]->right(), 5);
}

void TabControl::onScrollPressLeft(event::Info)
{
    m_scrollOffset-=120;
}

void TabControl::onScrollPressRight(event::Info)
{
    m_scrollOffset+=120;
}

}//namespace controls
}//namespace gweni
