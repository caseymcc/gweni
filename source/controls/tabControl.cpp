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
#include <gweni/controls/canvas.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/selectable.h>


namespace gweni
{
namespace controls
{

class TabControlInner: public Base
{
public:

    GWENI_CONTROL_INLINE(TabControlInner, Base)
    {}
};


GWENI_CONTROL_CONSTRUCTOR(TabControl)
{
    m_scrollOffset=0;
    m_currentButton=nullptr;
    m_closeVetoed=false;
    m_tabStrip=newChild<TabStrip>();
    m_tabStrip->setTabPosition(DockPosition::Top);
    // The overflow buttons and the tab-list button, after the strip so they
    // are numbered (and drawn) over it, hidden until they are needed. Placed
    // by handleOverflow() and arrange(Y), at the size set here.
    m_scroll[0]=newChild<internal::ScrollBarButton>();
    m_scroll[0]->setDirectionLeft();
    m_scroll[0]->onPressCaller.add(this, &TabControl::onScrollPressLeft);

    m_scroll[1]=newChild<internal::ScrollBarButton>();
    m_scroll[1]->setDirectionRight();
    m_scroll[1]->onPressCaller.add(this, &TabControl::onScrollPressRight);

    m_list=newChild<internal::ScrollBarButton>();
    m_list->setDirectionDown();
    m_list->onPressCaller.add(this, &TabControl::onTabListPressed);

    for(internal::ScrollBarButton *button : {m_scroll[0], m_scroll[1], m_list})
    {
        button->setSize(ButtonSize, ButtonSize);
        button->setPlacement(Placement::Owner);
        button->setTabable(false);
        button->setHidden(true);
    }

    m_innerPanel=newChild<TabControlInner>();
    m_innerPanel->setDock(DockPosition::Center);
    m_innerPanel->sendToBack();
    setTabable(false);
}

TabControl::~TabControl()
{
    if(!m_listPopup)
        return;

    // Made on the canvas, which deletes it with its other children when it is
    // tearing down, before or after this control, so it is not touched then
    // (as ColorEdit's picker); otherwise it goes delayed, as a Popup's creator
    // does.
    Canvas *canvas=getCanvas();

    if(!canvas)
        canvas=m_listPopup->getCanvas();
    if(canvas && canvas->releasingChildren())
        return;
    m_listPopup->delayedDelete();
}

TabButton *TabControl::addPage(String text, controls::Base *page)
{
    if(!page)
        page=newChild<Base>();
    else
        page->setParent(this);

    TabButton *button=m_tabStrip->newChild<TabButton>();
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
    page->setDock(DockPosition::Center);
    button->setParent(m_tabStrip);
    button->setDock(DockPosition::Left);
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
    // its colours and close button follow the selection, also when it was
    // selected by a key or a close rather than its own click
    button->redraw();
    m_tabStrip->invalidate();
    invalidate();
    scrollToTab(button);
}

bool TabControl::requestClose(TabButton *button)
{
    if(!button || button->getTabControl() != this)
        return false;

    // a handler may close another tab itself, which asks again
    const bool outer=m_closeVetoed;
    m_closeVetoed=false;

    event::Info info(button);
    onClose.call(this, info);

    const bool vetoed=m_closeVetoed;
    m_closeVetoed=outer;

    if(vetoed)
        return false;

    closeTab(button);
    return true;
}

void TabControl::closeTab(TabButton *button)
{
    if(!button || button->getTabControl() != this)
        return;

    if(m_currentButton == button)
    {
        TabButton *neighbour=nullptr;
        const int count=tabCount();

        for(int i=0; i < count; ++i)
        {
            if(getTab(i) != button)
                continue;

            neighbour=(i+1 < count)?getTab(i+1):nullptr;

            if(!neighbour && i > 0)
                neighbour=getTab(i-1);
            break;
        }

        if(neighbour)
        {
            event::Info info(neighbour);
            info.controlCaller=neighbour;
            onTabPressed(info);
        }
    }

    Base *page=button->getPage();

    button->setHidden(true);
    removePage(button);

    if(page)
    {
        page->setHidden(true);
        page->delayedDelete();
    }

    button->delayedDelete();
}

void TabControl::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    // owner-placed, so not counted: measured for their own subtrees only
    for(internal::ScrollBarButton *button : {m_scroll[0], m_scroll[1], m_list})
    {
        if(button->hidden())
            button->doCalculateSize(skin, dim);
    }
}

// Whether the tabs overflow needs only this control's width, final in the X
// arrange, and the strip's preferred width, so it is decided before the
// children are placed. The buttons are owner-placed and the scroll offset only
// moves the tabs (TabStrip::setScrollOffset), so neither changes the size this
// control was measured at. Their y waits for the strip's height, final in the
// Y pass.
void TabControl::arrange(skin::Base *skin, Dim dim)
{
    {
        PlacementScope placement;

        if(dim == Dim::X)
            handleOverflow();
        else
        {
            const int stripHeight=m_tabStrip->getPreferredSize().height;
            const Margin &margin=m_tabStrip->getMargin();
            int stripY=getPadding().top+margin.top;

            if(m_tabStrip->getDock() == DockPosition::Bottom)
                stripY=getHeight()-getPadding().bottom-margin.bottom-stripHeight;

            const int y=stripY+(stripHeight-ButtonSize)/2;

            for(internal::ScrollBarButton *button : {m_scroll[0], m_scroll[1], m_list})
            {
                if(!button->hidden())
                    placeChildPos(button, button->getX(), y);
            }
        }
    }

    ParentClass::arrange(skin, dim);
}

void TabControl::onLoseTab(TabButton *button)
{
    if(m_currentButton == button)
        m_currentButton=nullptr;
    if(m_scrollTarget == button)
        m_scrollTarget=nullptr;

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

void TabControl::setTabStripPosition(DockPosition dock)
{
    m_tabStrip->setTabPosition(dock);
}

bool TabControl::doesAllowDrag()
{
    return m_tabStrip->allowTabReorder();
}

bool TabControl::isHorizontalStrip()
{
    const DockPosition dock=m_tabStrip->getDock();

    return dock == DockPosition::Top || dock == DockPosition::Bottom;
}

void TabControl::tabExtents(std::vector<std::pair<TabButton *, Rect>> &extents)
{
    extents.clear();

    // the row as the strip docks it, from its origin (its left padding):
    // each tab at its margin's left, the next after its margin's right
    int x=0;

    for(auto &&control : m_tabStrip->getChildren())
    {
        TabButton *button=gweni_cast<TabButton>(control);

        if(!button || button->hidden())
            continue;

        const Margin &margin=button->getMargin();
        const int width=button->getPreferredSize().width;

        extents.emplace_back(button, Rect(x+margin.left, 0, width, 0));
        x+=margin.left+width+margin.right;
    }
}

// The column holds, from the right, the list button (if it is on) and the two
// scroll buttons (while the tabs overflow). The tabs overflow when the row runs
// past where the column would start with the list button alone; then the
// column takes the scroll buttons too, and the row may scroll until its last
// tab's right edge is where the visible tabs end.
void TabControl::handleOverflow()
{
    const bool horizontal=isHorizontalStrip();
    const bool list=m_tabListButton && horizontal;
    const int right=getWidth()-getPadding().right;
    const auto tabsRight=[&](int buttons)
        {
            if(buttons == 0)
                return right;
            return right-ColumnInset-buttons*ButtonSize-(buttons-1)*ButtonSpacing-ColumnGap;
        };

    const int padLeft=m_tabStrip->getPadding().left;
    const int rowOrigin=getPadding().left+m_tabStrip->getMargin().left+padLeft;
    const int rowEnd=rowOrigin-padLeft+m_tabStrip->getPreferredSize().width;
    const bool overflow=horizontal && rowEnd > tabsRight(list?1:0);
    const int buttons=(overflow?2:0)+(list?1:0);

    m_overflowing=overflow;
    m_tabsRight=tabsRight(buttons);
    m_maxScrollOffset=overflow?std::max(0, rowEnd-m_tabsRight):0;

    if(m_scrollTarget)
    {
        std::vector<std::pair<TabButton *, Rect>> extents;

        tabExtents(extents);
        for(const auto &extent : extents)
        {
            if(extent.first != m_scrollTarget)
                continue;

            // the least scroll that shows the whole tab
            const int visible=m_tabsRight-rowOrigin;
            const Rect &r=extent.second;

            if(r.x < m_scrollOffset)
                m_scrollOffset=r.x;
            else if(r.x+r.w > m_scrollOffset+visible)
                m_scrollOffset=r.x+r.w-visible;
            break;
        }
        m_scrollTarget=nullptr;
    }

    m_scrollOffset=gweni::clamp(m_scrollOffset, 0, m_maxScrollOffset);
    m_tabStrip->setScrollOffset(m_scrollOffset);

    m_scroll[0]->setHidden(!overflow);
    m_scroll[1]->setHidden(!overflow);
    m_list->setHidden(!list);

    // right to left: the list button, then the right and left arrows
    int x=right-ColumnInset-ButtonSize;

    if(list)
    {
        placeChildPos(m_list, x, m_list->getY());
        x-=ButtonSize+ButtonSpacing;
    }
    if(overflow)
    {
        placeChildPos(m_scroll[1], x, m_scroll[1]->getY());
        x-=ButtonSize+ButtonSpacing;
        placeChildPos(m_scroll[0], x, m_scroll[0]->getY());
    }

    // While a button shows, the strip (and its tabs, which do not clip
    // themselves) is cut to the row between its origin and where the visible
    // tabs end, so no tab draws or takes the mouse under the column, nor in
    // the strip's padding once scrolled. Its height is the control's (it clips
    // the children to that anyway), so the rect depends on the width alone.
    const bool clip=buttons > 0;
    const Rect stripClip(rowOrigin, 0, m_tabsRight-rowOrigin, 0x3fff);

    if(clip != m_clipStrip || (clip && !(stripClip == m_stripClip)))
    {
        m_clipStrip=clip;
        m_stripClip=stripClip;
        // the new region reaches slots already drawn
        m_tabStrip->addStateChangeRecursive(StateChange_Geometry);
    }
}

const Rect *TabControl::getChildClipRect(const Base *child) const
{
    return (m_clipStrip && child == m_tabStrip)?&m_stripClip:nullptr;
}

void TabControl::scrollTabs(bool forward)
{
    std::vector<std::pair<TabButton *, Rect>> extents;

    tabExtents(extents);

    // the next (or previous) tab's start, so a press puts a whole tab at the
    // row's origin; the last step stops at the end of the row
    int target=forward?m_maxScrollOffset:0;

    for(const auto &extent : extents)
    {
        const int start=std::max(0, extent.second.x);

        if(forward && start > m_scrollOffset)
        {
            target=std::min(start, m_maxScrollOffset);
            break;
        }
        if(!forward && start < m_scrollOffset)
            target=start;
    }

    if(target == m_scrollOffset)
        return;

    m_scrollOffset=target;
    invalidate();
}

void TabControl::scrollToTab(TabButton *button)
{
    if(!button || button->getTabControl() != this)
        return;

    m_scrollTarget=button;
    invalidate();
}

void TabControl::onScrollPressLeft(event::Info)
{
    scrollTabs(false);
}

void TabControl::onScrollPressRight(event::Info)
{
    scrollTabs(true);
}

void TabControl::setTabListButton(bool show)
{
    if(m_tabListButton == show)
        return;

    m_tabListButton=show;
    invalidate();
}

void TabControl::onTabListPressed(event::Info)
{
    toggleTabList();
}

void TabControl::toggleTabList()
{
    if(m_listPopup && m_listPopup->isOpen())
    {
        m_listPopup->close();
        return;
    }

    Canvas *canvas=getCanvas();

    if(!canvas || m_list->hidden())
        return;

    if(!m_listPopup)
        m_listPopup=canvas->newChild<Popup>();

    // One row per tab, in strip order, made again only here: the rows of an
    // earlier opening are reused, the surplus deleted.
    m_listTabs.clear();
    for(auto &&control : m_tabStrip->getChildren())
    {
        TabButton *button=gweni_cast<TabButton>(control);

        if(button && !button->hidden())
            m_listTabs.push_back(button);
    }

    while(m_listRows.size() > m_listTabs.size())
    {
        delete m_listRows.back();
        m_listRows.pop_back();
    }
    while(m_listRows.size() < m_listTabs.size())
    {
        Selectable *row=m_listPopup->newChild<Selectable>();

        row->setToggleable(false);
        row->setTabable(false);
        row->onPressCaller.add(this, &TabControl::onTabListChosen);
        m_listRows.push_back(row);
    }

    for(size_t i=0; i < m_listTabs.size(); ++i)
    {
        m_listRows[i]->setText(m_listTabs[i]->getText());
        m_listRows[i]->setSelected(m_listTabs[i] == m_currentButton, false);
    }

    m_listPopup->open(m_list, PopupPlacement::Below);
}

void TabControl::onTabListChosen(event::Info info)
{
    TabButton *chosen=nullptr;

    for(size_t i=0; i < m_listRows.size(); ++i)
    {
        if(m_listRows[i] == info.controlCaller)
        {
            chosen=m_listTabs[i];
            break;
        }
    }

    if(m_listPopup)
        m_listPopup->close();

    // the tab may have left since the list opened: only one still here is
    // selected (the pointer is compared, never followed, until then)
    bool present=false;

    for(auto &&control : m_tabStrip->getChildren())
    {
        if(control == chosen)
            present=true;
    }

    if(!chosen || !present)
        return;

    event::Info press(chosen);

    press.controlCaller=chosen;
    onTabPressed(press);
    scrollToTab(chosen);
}

}//namespace controls
}//namespace gweni
