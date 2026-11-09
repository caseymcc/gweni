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

#include <utility>
#include <vector>

namespace gweni
{
namespace controls
{
namespace internal
{
class ScrollBarButton;
}//namespace internal

class Popup;
class Selectable;

//! Pages selected by a strip of tabs. A Top or Bottom strip whose tabs do not
//! fit the control's width overflows: two scroll buttons show in a column at
//! the strip's trailing end, the tabs end before it (clipped there), and each
//! press scrolls the row by one tab, stopping once the last tab's right edge
//! reaches the column. The row is clipped to the strip's padding on the left
//! and the column on the right. Selecting a tab scrolls it fully into view. The tabs
//! overflow only where the control is narrower than they are: like any
//! control it is as wide as its content, the whole row, unless its size flags
//! (Fixed, Shrink), a dock or a layout cell make it narrower. A strip on the
//! left or right never overflows.
//!
//! setTabListButton(true) adds a chevron button to that column (for a Top or
//! Bottom strip), shown whether or not the tabs overflow: it opens a Popup
//! below it listing every tab, the selected one marked, and choosing one
//! selects it and scrolls it into view (imgui's tab list popup button).
class GWENI_EXPORT TabControl: public Base
{
    GWENI_CONTROL(TabControl, Base);

public:
    //! The side of each overflow and list button.
    static constexpr int ButtonSize=14;
    //! The units between two buttons of the column.
    static constexpr int ButtonSpacing=2;
    //! The units between the last visible tab and the column.
    static constexpr int ColumnGap=4;
    //! The units between the column and the control's right edge.
    static constexpr int ColumnInset=2;

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

    //! A closable tab's close button was clicked (TabButton::setClosable), before
    //! the tab closes: controlCaller is this control, `control` the TabButton.
    //! A handler that calls vetoClose() keeps the tab (an "unsaved changes"
    //! prompt, say); otherwise closeTab() closes it once every handler has run.
    gweni::event::Caller onClose;

    //! From an onClose handler: the tab being closed stays.
    void vetoClose()
    {
        m_closeVetoed=true;
    }

    //! What a tab's close button does: fires onClose for \p button, then closes
    //! it with closeTab() unless a handler vetoed. Returns whether it closed.
    virtual bool requestClose(TabButton *button);

    //! Closes \p button without asking. If it was the selected tab, the next
    //! tab is selected, or the previous one when it was the last; closing
    //! another tab leaves the selection alone. The tab leaves through
    //! removePage() (so onLoseTabCaller fires), and the button and its page are
    //! hidden and deleted at the start of the next frame (delayedDelete), so a
    //! close from the button's own click handler is safe.
    virtual void closeTab(TabButton *button);

    virtual void onTabPressed(event::Info info);
    virtual void onLoseTab(TabButton *button);

    //! Shows or hides the tab-list button (off by default).
    void setTabListButton(bool show);
    bool hasTabListButton() const { return m_tabListButton; }

    //! Whether the tabs overflowed the strip in the last layout pass, so the
    //! scroll buttons show.
    bool isOverflowing() const { return m_overflowing; }
    //! Units the tabs are scrolled by, as the last layout pass placed them.
    int getScrollOffset() const { return m_scrollOffset; }
    //! The largest scroll offset in the last layout pass: the one that puts the
    //! last tab's right edge at the column; 0 when the tabs fit.
    int getMaxScrollOffset() const { return m_maxScrollOffset; }
    //! The x, in this control's coordinates, where the visible tabs end: the
    //! button column's left edge less ColumnGap, or the right padding edge when
    //! no button shows, as the last layout pass placed it.
    int getTabsRight() const { return m_tabsRight; }

    //! Scrolls the row one tab back or on, as a scroll button press does; it
    //! takes effect in the next layout pass.
    void scrollTabs(bool forward);
    //! Scrolls \p button fully into view in the next layout pass (selecting a
    //! tab does this).
    void scrollToTab(TabButton *button);

    //! The left and right scroll buttons, and the tab-list button.
    internal::ScrollBarButton *getScrollButton(int i) { return m_scroll[i]; }
    internal::ScrollBarButton *getTabListButton() { return m_list; }
    //! The tab list's popup, made the first time it opens; null before.
    Popup *getTabListPopup() { return m_listPopup; }
    //! Opens the tab list below its button, or closes it if it is open.
    void toggleTabList();

    //! While a button shows, the strip is clipped to the row from its origin
    //! (the strip's left padding) to where the visible tabs end.
    const Rect *getChildClipRect(const Base *child) const override;
    bool hasChildClipRects() const override { return true; }

protected:

    //! Also measures the buttons that are hidden: the X arrange may show one,
    //! and a hidden control is not measured with the others.
    void calculateSize(skin::Base *skin, Dim dim) override;

    //! X: decides the overflow from this control's width and the strip's
    //! preferred width, sets the strip's scroll offset and shows and places the
    //! buttons along x, before the children are arranged. Y: centres the
    //! buttons on the strip's row, once its height is final.
    void arrange(skin::Base *skin, Dim dim) override;

private:

    void handleOverflow();
    //! Where each shown tab starts along the row, from the row's origin, and
    //! how wide it is, from the preferred sizes the pass measured.
    void tabExtents(std::vector<std::pair<TabButton *, Rect>> &extents);
    bool isHorizontalStrip();

    void onScrollPressLeft(event::Info);
    void onScrollPressRight(event::Info);
    void onTabListPressed(event::Info);
    void onTabListChosen(event::Info);

    TabStrip *m_tabStrip;
    TabButton *m_currentButton;
    bool m_closeVetoed;

    internal::ScrollBarButton *m_scroll[2];
    internal::ScrollBarButton *m_list;
    int m_scrollOffset;
    int m_maxScrollOffset=0;
    int m_tabsRight=0;
    bool m_overflowing=false;
    bool m_tabListButton=false;
    //! The tab scrollToTab() asked for, resolved in the next X arrange.
    TabButton *m_scrollTarget=nullptr;

    //! The strip's clip while a button shows (getChildClipRect).
    Rect m_stripClip;
    bool m_clipStrip=false;

    Popup *m_listPopup=nullptr;
    //! The rows of the list's popup and the tab each one selects, in strip
    //! order, as it was last opened.
    std::vector<Selectable *> m_listRows;
    std::vector<TabButton *> m_listTabs;


};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_tabcontrol_h_
