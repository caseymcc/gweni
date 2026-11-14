/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/splitButton.h>
#include <gweni/controls/menuItem.h>
#include <gweni/inputHandler.h>

#include <algorithm>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(SplitButtonMain)
{
    // the SplitButton takes the focus for both parts (a press finds it as the
    // nearest ancestor taking keyboard input)
    setKeyboardInputEnabled(false);
    setTabable(false);
}

SplitButton *SplitButtonMain::getSplitButton() const
{
    return gweni_cast<SplitButton>(getParent());
}

GWENI_CONTROL_CONSTRUCTOR(SplitButtonArrow)
{
    setKeyboardInputEnabled(false);
    setTabable(false);
    setTextPadding(Padding(0, 0, 0, 0));
}

SplitButton *SplitButtonArrow::getSplitButton() const
{
    return gweni_cast<SplitButton>(getParent());
}

Base *SplitButtonArrow::menuAnchor()
{
    if(SplitButton *split=getSplitButton())
        return split;
    return this;
}

}//namespace internal

GWENI_CONTROL_CONSTRUCTOR(SplitButton)
{
    setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    setKeyboardInputEnabled(true);
    setTabable(true);

    m_main=newChild<internal::SplitButtonMain>();
    m_main->setPlacement(Placement::Owner);
    m_main->onPressCaller.add(this, &SplitButton::onMainPressed);
    m_main->onToggleCaller.add(this, &SplitButton::onMainToggled);

    m_arrow=newChild<internal::SplitButtonArrow>();
    m_arrow->setPlacement(Placement::Owner);
}

// As wide as the main part's content and the arrow part with the divider
// between them, as high as the higher part.
void SplitButton::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    const Padding &padding=getPadding();

    if(dim == Dim::X)
        m_preferredSize.width=preferredFromContent(m_main->getPreferredSize().width+Divider+ArrowWidth
            +padding.left+padding.right, Dim::X);
    else
        m_preferredSize.height=preferredFromContent(std::max(m_main->getPreferredSize().height,
            m_arrow->getPreferredSize().height)+padding.top+padding.bottom, Dim::Y);
}

// The parts are placed along each axis before the control arranges its
// children (a Chip's remove square's pattern): the arrow part at the right
// edge, the main part up to the divider left of it, both the full height.
void SplitButton::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        const int right=getWidth()-padding.right;
        const int arrowX=std::max(padding.left, right-ArrowWidth);
        const int mainWidth=std::max(0, arrowX-Divider-padding.left);

        placeChild(m_main, Rect(padding.left, m_main->getY(), mainWidth, m_main->getHeight()));
        placeChild(m_arrow, Rect(arrowX, m_arrow->getY(), right-arrowX, m_arrow->getHeight()));
    }
    else
    {
        const int height=std::max(0, getHeight()-padding.top-padding.bottom);

        placeChild(m_main, Rect(m_main->getX(), padding.top, m_main->getWidth(), height));
        placeChild(m_arrow, Rect(m_arrow->getX(), padding.top, m_arrow->getWidth(), height));
    }

    ParentClass::arrange(skin, dim);
}

void SplitButton::setText(const String &text)
{
    m_main->setText(text);
}

const String &SplitButton::getText()
{
    return m_main->getText();
}

void SplitButton::setImage(const String &name, bool center)
{
    m_main->setImage(name, center);
}

Menu *SplitButton::getMenu()
{
    return m_arrow->getMenu();
}

bool SplitButton::isMenuOpen() const
{
    return m_arrow->isMenuOpen();
}

void SplitButton::openMenu(MenuFocus focus)
{
    if(isDisabled())
        return;
    m_arrow->openMenu(focus);
}

void SplitButton::closeMenu()
{
    m_arrow->closeMenu();
}

void SplitButton::setIsToggle(bool toggle)
{
    m_main->setIsToggle(toggle);
}

bool SplitButton::isToggle() const
{
    return m_main->isToggle();
}

bool SplitButton::getToggleState() const
{
    return m_main->getToggleState();
}

void SplitButton::setToggleState(bool on)
{
    m_main->setToggleState(on);
}

void SplitButton::setMainItem(MenuItem *item)
{
    m_mainItem=item ? item->getControlId() : 0;

    if(item)
        m_main->setText(item->getText());
}

MenuItem *SplitButton::getMainItem() const
{
    return m_mainItem ? gweni_cast<MenuItem>(getControlById(m_mainItem)) : nullptr;
}

// Switch's: no keyboard input, Tab stop or focus while disabled, so a click
// on either part focuses nothing.
void SplitButton::setDisabled(bool disabled)
{
    setKeyboardInputEnabled(!disabled);
    setTabable(!disabled);
    if(disabled)
        closeMenu();

    ParentClass::setDisabled(disabled);
    m_main->setDisabled(disabled);
    m_arrow->setDisabled(disabled);
    if(disabled && (KeyboardFocus == this))
        unfocus();
}

// Both parts draw the control's focus in their borders (Base::focus() and
// unfocus() redraw the control itself, so its divider too).
void SplitButton::onKeyboardFocus()
{
    m_main->redraw();
    m_arrow->redraw();
}

void SplitButton::onLostKeyboardFocus()
{
    m_main->redraw();
    m_arrow->redraw();
}

void SplitButton::onMainPressed(event::Info)
{
    onPress.call(this);

    if(MenuItem *item=getMainItem())
        item->onMenuItemSelected.call(item);
}

void SplitButton::onMainToggled(event::Info)
{
    onToggle.call(this);
}

// The main part's action; a menu left open (a press on the arrow let go off
// it) closes first, as a click on the main part closes it.
void SplitButton::pressMainFromKey()
{
    if(isDisabled())
        return;

    closeMenu();
    m_main->doAction();
}

bool SplitButton::onKeySpace(bool down)
{
    if(down)
        pressMainFromKey();
    return true;
}

bool SplitButton::onKeyReturn(bool down)
{
    if(down)
        pressMainFromKey();
    return true;
}

bool SplitButton::onKeyDown(bool down)
{
    if(!input::isKeyDown(keys::Alt))
        return ParentClass::onKeyDown(down);

    if(down)
        toggleMenuFromKey();
    return true;
}

bool SplitButton::onKeyF4(bool down)
{
    if(down)
        toggleMenuFromKey();
    return true;
}

// The menu open while the control has the focus (a press on the open arrow
// let go off it): Alt+Down and F4 close it, as a second press would.
void SplitButton::toggleMenuFromKey()
{
    if(isMenuOpen())
        closeMenu();
    else
        openMenu(MenuFocus::FirstItem);
}

bool SplitButton::onKeyEscape(bool down)
{
    if(!isMenuOpen())
        return ParentClass::onKeyEscape(down);

    if(down)
        closeMenu();
    return true;
}

}//namespace controls
}//namespace gweni
