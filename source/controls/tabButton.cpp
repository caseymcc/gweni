/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/tabButton.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/highlight.h>
#include <gweni/dragAndDrop.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(TabButton)
{
    m_page=nullptr;
    m_control=nullptr;
    m_closeButton=nullptr;
    dragAndDrop_setPackage(true, "TabButtonMove");
    setTextPadding(Padding(2, 2, 2, 2));
    // The text reads from the top left of the padding, where sizeToContents()
    // puts it, in a tab stretched to fill a Left/Right strip too.
    setTextAlignment(Alignment::Left|Alignment::Top);
}

// The padding follows the side the strip is docked on, and the text sits
// inside it. Set before this button is measured, which counts them: the strip
// docks the button at the size it measures.
void TabButton::calculateSize(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X)
    {
        PlacementScope placement;
        DockPosition parentDock=m_control->getTabStrip()->getDock();

        if(parentDock == DockPosition::Bottom)
            setPadding(Padding(3, 1, 5, 4));
        else if(parentDock == DockPosition::Top)
            setPadding(Padding(3, 3, 5, 2));
        else
            setPadding(Padding(3, 2, 5, 2));

        // a closable tab's close square sits between its text and its right
        // padding, so the text is laid out clear of it on every side
        if(m_closeButton)
        {
            Padding padding=getPadding();
            padding.right+=CloseButtonSize;
            setPadding(padding);
        }
    }

    ParentClass::calculateSize(skin, dim);
}

void TabButton::setClosable(bool closable)
{
    if(closable == isClosable())
        return;

    if(closable)
    {
        m_closeButton=newChild<TabCloseButton>();
        m_closeButton->setPlacement(Placement::Owner);
        m_closeButton->setSize(CloseButtonSize, CloseButtonSize);
        m_closeButton->onPressCaller.add(this, &TabButton::onCloseButtonPressed);
        updateCloseButton();
    }
    else
    {
        delete m_closeButton;
        m_closeButton=nullptr;
    }

    invalidate();
}

bool TabButton::isTabHovered() const
{
    return isHovered() || (m_closeButton && m_closeButton->isHovered());
}

// The close button is a CloseButtonSize square along each axis, given before
// this tab arranges it (the TreeNode toggle's pattern).
void TabButton::arrange(skin::Base *skin, Dim dim)
{
    if(m_closeButton)
    {
        if(dim == Dim::X)
            placeChildSize(m_closeButton, CloseButtonSize, m_closeButton->getHeight());
        else
            placeChildSize(m_closeButton, m_closeButton->getWidth(), CloseButtonSize);
    }

    ParentClass::arrange(skin, dim);
}

// Placed once the tab has both its width and height: at the trailing end of
// the content, in the right padding calculateSize() added, centred between
// the top and bottom padding. Only its position changes, which nothing laid
// out in this pass has read.
void TabButton::postLayout(skin::Base *skin)
{
    ParentClass::postLayout(skin);

    if(!m_closeButton)
        return;

    const Padding &padding=getPadding();
    // the square is the first CloseButtonSize units of the right padding
    const int x=getWidth()-padding.right;
    const int y=padding.top+(getHeight()-padding.top-padding.bottom-CloseButtonSize)/2;

    placeChildPos(m_closeButton, x, y);
}

// The mouse moving onto or off the close button keeps or ends this tab's
// hover: the text colour and the button's own visibility follow it.
void TabButton::onDescendantInteraction(Base *control)
{
    ParentClass::onDescendantInteraction(control);

    if(m_closeButton && control == m_closeButton)
        redraw();
}

// Shown on the selected tab and on the hovered one, as imgui does; a disabled
// tab shows none. Run from updateColors(), so every hover, press, focus and
// selection change (each a redraw()) settles it.
void TabButton::updateCloseButton()
{
    if(!m_closeButton)
        return;

    const bool show=!isDisabled() && (isActive() || isTabHovered());

    m_closeButton->setHidden(!show);
}

void TabButton::onCloseButtonPressed(event::Info /*info*/)
{
    if(m_control)
        m_control->requestClose(this);
}


void TabButton::setTabControl(TabControl *ctrl)
{
    if(m_control == ctrl)
        return;

    if(m_control)
        m_control->onLoseTab(this);

    m_control=ctrl;
}

bool TabButton::dragAndDrop_shouldStartDrag()
{
    return m_control->doesAllowDrag();
}

bool TabButton::onKeyUp(bool down)
{
    onKeyLeft(down);
    return true;
}

bool TabButton::onKeyDown(bool down)
{
    onKeyRight(down);
    return true;
}

bool TabButton::onKeyLeft(bool down)
{
    if(down)
    {
        Base::List::reverse_iterator it=std::find(
            m_parent->getChildren().rbegin(), m_parent->getChildren().rend(), this);

        if(it != m_parent->getChildren().rend() && (++it != m_parent->getChildren().rend()))
        {
            Base *nextTab=*it;
            getTabControl()->onTabPressed(nextTab);
            gweni::KeyboardFocus=nextTab;
        }
    }

    return true;
}

bool TabButton::onKeyRight(bool down)
{
    if(down)
    {
        Base::List::iterator it=std::find(m_parent->getChildren().begin(),
            m_parent->getChildren().end(), this);

        if(it != m_parent->getChildren().end() && (++it != m_parent->getChildren().end()))
        {
            Base *nextTab=*it;
            getTabControl()->onTabPressed(nextTab);
            gweni::KeyboardFocus=nextTab;
        }
    }

    return true;
}

void TabButton::updateColors()
{
    updateCloseButton();

    const bool hovered=isTabHovered();

    if(!isActive())
    {
        setImageAlpha(0.5);

        if(isDisabled())
            return setTextColor(getSkin()->Colors.Tab.Inactive.Disabled);

        if(isDepressed())
            return setTextColor(getSkin()->Colors.Tab.Inactive.Down);

        if(hovered)
            return setTextColor(getSkin()->Colors.Tab.Inactive.Hover);

        return setTextColor(getSkin()->Colors.Tab.Inactive.Normal);
    }

    setImageAlpha(1.0);

    if(isDisabled())
        return setTextColor(getSkin()->Colors.Tab.Active.Disabled);

    if(isDepressed())
        return setTextColor(getSkin()->Colors.Tab.Active.Down);

    if(hovered)
        return setTextColor(getSkin()->Colors.Tab.Active.Hover);

    setTextColor(getSkin()->Colors.Tab.Active.Normal);
}

}//namespace controls
}//namespace gweni
