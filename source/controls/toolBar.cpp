/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/toolBar.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/imagePanel.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/separator.h>
#include <gweni/controls/splitButton.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

///////////////////////////////////////////////////////////////////////////
//ToolButton

// Owner-placed by its toolbar, as wide and high as its content: its Text has
// no padding of its own, the button keeps the room around it.
GWENI_CONTROL_CONSTRUCTOR(ToolButton)
{
    setDock(DockPosition::None);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setTextPadding(Padding(0, 0, 0, 0));
    setDefaultTextAlignment(Alignment::Left|Alignment::CenterV);
    m_text->setPlacement(Placement::Owner);
    setKeyboardInputEnabled(true);
    setTabable(true);
}

namespace
{
// The toolbar a tool item is an item (or the overflow button) of.
ToolBar *toolBarOf(const Base *item)
{
    return item?gweni_cast<ToolBar>(item->getParent()):nullptr;
}
}

bool ToolButton::isTabable()
{
    ToolBar *bar=toolBarOf(this);

    return ParentClass::isTabable() && (!bar || bar->getTabStop() == this);
}

void ToolButton::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();
    if(ToolBar *bar=toolBarOf(this))
        bar->itemFocused(this);
}

bool ToolButton::onKeySpace(bool down)
{
    if(isDisabled())
        return false;
    return ParentClass::onKeySpace(down);
}

bool ToolButton::onKeyReturn(bool down)
{
    return onKeySpace(down);
}

void ToolButton::setDisabled(bool disabled)
{
    setKeyboardInputEnabled(!disabled);
    setTabable(!disabled);
    ParentClass::setDisabled(disabled);
    if(disabled && (KeyboardFocus == this))
        unfocus();
}

void ToolButton::setText(const String &str, bool doEvents)
{
    ParentClass::setText(str, doEvents);
    m_text->setHidden(!textShows());
    // TextOnly shows the icon only while there is no text
    if(m_image)
        m_image->setHidden(!imageShows());
    updateToolTip();
}

void ToolButton::setImage(const String &name, bool center)
{
    const bool had=(m_image != nullptr);

    ParentClass::setImage(name, center);
    // Button's padding for its image is the tool button's to keep
    setTextPadding(Padding(0, 0, 0, 0));
    if(m_image)
    {
        m_image->setPlacement(Placement::Owner);
        m_image->setMargin(Margin(0, 0, 0, 0));
        m_image->setMouseInputEnabled(false);
        m_image->setHidden(!imageShows());
    }
    m_text->setHidden(!textShows());
    updateToolTip();
    if(had != (m_image != nullptr))
        invalidate();
}

void ToolButton::setDisplay(ToolBarDisplay display)
{
    if(display == m_display)
        return;

    m_display=display;
    m_text->setHidden(!textShows());
    if(m_image)
        m_image->setHidden(!imageShows());
    updateToolTip();
    invalidate();
}

bool ToolButton::imageShows() const
{
    if(!m_image)
        return false;
    return m_display != ToolBarDisplay::TextOnly || m_text->getText().empty();
}

bool ToolButton::textShows() const
{
    if(m_text->getText().empty())
        return false;
    return m_display != ToolBarDisplay::IconOnly || !m_image;
}

String ToolButton::getMenuText() const
{
    const String &text=m_text->getText();

    return text.empty()?getToolTipText():text;
}

void ToolButton::updateToolTip()
{
    const bool iconOnly=imageShows() && !textShows() && !m_text->getText().empty();

    // the app's own tooltip is put aside while the text is the tooltip, and
    // given back after
    if(iconOnly)
    {
        if(!m_textToolTip)
            m_appToolTip=getToolTipText();
        Button::setToolTipText(m_text->getText());
        m_textToolTip=true;
    }
    else if(m_textToolTip)
    {
        Button::setToolTipText(m_appToolTip);
        m_appToolTip.clear();
        m_textToolTip=false;
    }
}

void ToolButton::setToolTipText(const String &text)
{
    // while the text is the tooltip, the app's tooltip is the one put aside
    if(m_textToolTip)
    {
        m_appToolTip=text;
        return;
    }
    Button::setToolTipText(text);
}

int ToolButton::imageWidth() const
{
    return imageShows()?m_image->textureWidth():0;
}

int ToolButton::imageHeight() const
{
    return imageShows()?m_image->textureHeight():0;
}

int ToolButton::contentWidth() const
{
    int width=imageWidth();

    if(textShows())
        width+=(imageShows()?ImageGap:0)+m_text->getPreferredSize().width;
    return width;
}

void ToolButton::calculateSize(skin::Base *skin, Dim dim)
{
    // the Text and the image are measured for themselves; neither counts
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::X)
    {
        m_preferredSize.width=preferredFromContent(SidePadding*2+contentWidth(), Dim::X);
        return;
    }

    int height=imageHeight();

    if(textShows())
        height=std::max(height, m_text->getPreferredSize().height);
    m_preferredSize.height=preferredFromContent(PaddingY*2+height, Dim::Y);
}

// The image and the text as one group centred in the button.
void ToolButton::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    int x=std::max(SidePadding, (getWidth()-contentWidth())/2);

    if(imageShows())
    {
        placeChild(m_image, x, m_image->getY(), imageWidth(), imageHeight());
        x+=imageWidth()+ImageGap;
    }
    placeChild(m_text, x, m_text->getY(), textShows()?m_text->getPreferredSize().width:0, m_text->getHeight());
}

// Each part centred on the button's height.
void ToolButton::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    const int height=getHeight();

    if(imageShows())
        placeChild(m_image, m_image->getX(), (height-imageHeight())/2, m_image->getWidth(), imageHeight());

    const int textHeight=m_text->getPreferredSize().height;

    placeChild(m_text, m_text->getX(), (height-textHeight)/2, m_text->getWidth(), textHeight);
}

///////////////////////////////////////////////////////////////////////////
//internal::ToolBarOverflowButton

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(ToolBarOverflowButton)
{
    setDock(DockPosition::None);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setTextPadding(Padding(0, 0, 0, 0));
    setToolTipText("More");
}

ToolBar *ToolBarOverflowButton::getToolBar() const
{
    return gweni_cast<ToolBar>(getParent());
}

// The chevron alone: Width wide, as high as a tool button showing text.
void ToolBarOverflowButton::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::X)
        m_preferredSize.width=preferredFromContent(Width, Dim::X);
    else
        m_preferredSize.height=preferredFromContent(ChevronSize+2*ToolButton::PaddingY, Dim::Y);
}

bool ToolBarOverflowButton::isTabable()
{
    ToolBar *bar=getToolBar();

    return ParentClass::isTabable() && (!bar || bar->getTabStop() == this);
}

void ToolBarOverflowButton::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();
    if(ToolBar *bar=getToolBar())
        bar->itemFocused(this);
}

void ToolBarOverflowButton::aboutToOpenMenu(Menu *menu)
{
    if(ToolBar *bar=getToolBar())
        bar->prepareOverflowMenu(menu);
}

GWENI_CONTROL_CONSTRUCTOR(ToolBarDropDown)
{
}

bool ToolBarDropDown::isTabable()
{
    ToolBar *bar=toolBarOf(this);

    return ParentClass::isTabable() && (!bar || bar->getTabStop() == this);
}

void ToolBarDropDown::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();
    if(ToolBar *bar=toolBarOf(this))
        bar->itemFocused(this);
}

GWENI_CONTROL_CONSTRUCTOR(ToolBarSplitButton)
{
}

bool ToolBarSplitButton::isTabable()
{
    ToolBar *bar=toolBarOf(this);

    return ParentClass::isTabable() && (!bar || bar->getTabStop() == this);
}

void ToolBarSplitButton::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();
    if(ToolBar *bar=toolBarOf(this))
        bar->itemFocused(this);
}

}//namespace internal

///////////////////////////////////////////////////////////////////////////
//ToolBar

GWENI_CONTROL_CONSTRUCTOR(ToolBar)
{
    setPadding(gweni::Padding(Padding, Padding, Padding, Padding));
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setDock(DockPosition::Top);

    // hidden until an item overflows, placed by handleOverflow() and
    // arrange(Y); always the last child, so it is numbered after the items
    m_overflowButton=newChild<internal::ToolBarOverflowButton>();
    m_overflowButton->setPlacement(Placement::Owner);
    m_overflowButton->setHidden(true);
}

void ToolBar::addItem(Base *item, bool separator, bool control)
{
    ItemInfo info;

    info.separator=separator;
    info.control=control;
    item->setPlacement(Placement::Owner);
    // in item order, before the overflow button
    item->bringNextToControl(m_overflowButton, false);
    m_items.push_back(item);
    m_info.push_back(info);
    invalidate();
}

ToolButton *ToolBar::addButton(const String &text, const String &icon)
{
    ToolButton *button=newChild<ToolButton>();

    button->setDisplay(m_display);
    button->setText(text);
    if(!icon.empty())
        button->setImage(icon);
    addItem(button, false, false);
    return button;
}

ToolButton *ToolBar::addToggle(const String &text, const String &icon, bool on)
{
    ToolButton *button=addButton(text, icon);

    button->setIsToggle(true);
    button->setToggleState(on);
    return button;
}

Separator *ToolBar::addSeparator()
{
    Separator *separator=newChild<Separator>();

    separator->setVertical();
    addItem(separator, true, false);
    return separator;
}

DropDownButton *ToolBar::addDropDown(const String &text, const String &icon)
{
    DropDownButton *button=newChild<internal::ToolBarDropDown>();

    button->setText(text);
    // a tool button's room, the chevron's kept on the right
    button->setTextPadding(gweni::Padding(ToolButton::SidePadding, 0,
        ToolButton::SidePadding+DropDownButton::ChevronRoom, 0));
    if(!icon.empty())
        button->setImage(icon);
    button->setDock(DockPosition::None);
    addItem(button, false, false);
    return button;
}

SplitButton *ToolBar::addSplitButton(const String &text, const String &icon)
{
    SplitButton *button=newChild<internal::ToolBarSplitButton>();

    button->setText(text);
    button->getMainPart()->setTextPadding(gweni::Padding(ToolButton::SidePadding, 0, ToolButton::SidePadding, 0));
    if(!icon.empty())
        button->setImage(icon);
    button->setDock(DockPosition::None);
    addItem(button, false, false);
    return button;
}

Base *ToolBar::addControl(Base *control, const String &overflowText)
{
    if(!control)
        return nullptr;
    // already an item, or the overflow button: one entry per control, so a
    // deleted control leaves none behind in the list
    if((indexOf(control) >= 0) || (control == m_overflowButton))
        return control;

    control->setParent(this);
    control->setDock(DockPosition::None);
    addItem(control, false, true);
    m_info.back().overflowText=overflowText;
    return control;
}

int ToolBar::indexOf(const Base *item) const
{
    for(size_t i=0; i < m_items.size(); ++i)
    {
        if(m_items[i] == item)
            return static_cast<int>(i);
    }
    return -1;
}

void ToolBar::setOverflowPriority(Base *item, OverflowPriority priority)
{
    const int i=indexOf(item);

    if(i < 0 || m_info[i].priority == priority)
        return;

    m_info[i].priority=priority;
    invalidate();
}

OverflowPriority ToolBar::getOverflowPriority(const Base *item) const
{
    const int i=indexOf(item);

    return (i < 0)?OverflowPriority::Normal:m_info[i].priority;
}

void ToolBar::setOverflowText(Base *item, const String &text)
{
    const int i=indexOf(item);

    if(i < 0 || m_info[i].overflowText == text)
        return;

    m_info[i].overflowText=text;
    invalidate();
}

String ToolBar::getOverflowText(const Base *item) const
{
    const int i=indexOf(item);

    return (i < 0)?String():m_info[i].overflowText;
}

bool ToolBar::canOverflow(const Base *item) const
{
    const int i=indexOf(item);

    if(i < 0 || m_info[i].priority == OverflowPriority::AlwaysShow)
        return false;
    return !m_info[i].control || !m_info[i].overflowText.empty();
}

bool ToolBar::isOverflowed(const Base *item) const
{
    const int i=indexOf(item);

    return i >= 0 && m_info[i].overflowed;
}

std::vector<Base *> ToolBar::getOverflowedItems() const
{
    std::vector<Base *> items;

    for(size_t i=0; i < m_items.size(); ++i)
    {
        if(m_info[i].overflowed)
            items.push_back(m_items[i]);
    }
    return items;
}

void ToolBar::setDisplay(ToolBarDisplay display)
{
    if(display == m_display)
        return;

    m_display=display;
    for(Base *item : m_items)
        applyDisplay(item);
    invalidate();
}

void ToolBar::applyDisplay(Base *item)
{
    if(ToolButton *button=gweni_cast<ToolButton>(item))
        button->setDisplay(m_display);
}

void ToolBar::onChildRemoved(Base *child)
{
    const int i=indexOf(child);

    if(i >= 0)
    {
        m_items.erase(m_items.begin()+i);
        m_info.erase(m_info.begin()+i);
        ++m_overflowSerial;
        invalidate();
    }
    if(child == m_overflowButton)
        m_overflowButton=nullptr;

    ParentClass::onChildRemoved(child);
}

int ToolBar::itemWidth(size_t i) const
{
    if(m_info[i].separator)
        return SeparatorSlot;
    return m_items[i]->getPreferredSize().width;
}

bool ToolBar::fillsHeight(size_t i) const
{
    return !m_info[i].control;
}

int ToolBar::rowWidth(std::vector<bool> &shown) const
{
    // a separator shows only with a shown item before it (since the last
    // shown separator) and one after it
    bool itemBefore=false;
    int pending=-1;

    for(size_t i=0; i < m_items.size(); ++i)
    {
        if(!m_info[i].separator)
        {
            if(!shown[i])
                continue;
            if(pending >= 0)
            {
                shown[pending]=true;
                pending=-1;
            }
            itemBefore=true;
            continue;
        }

        shown[i]=false;
        if(itemBefore && pending < 0)
        {
            pending=static_cast<int>(i);
            itemBefore=false;
        }
    }

    int width=0;
    bool first=true;

    for(size_t i=0; i < m_items.size(); ++i)
    {
        if(!shown[i])
            continue;
        width+=(first?0:Spacing)+itemWidth(i);
        first=false;
    }
    return width;
}

int ToolBar::fullWidth() const
{
    std::vector<bool> shown(m_items.size(), true);
    bool forced=false;

    // an AlwaysOverflow item is never in the row, but the button it forces is
    for(size_t i=0; i < m_items.size(); ++i)
    {
        if(m_info[i].priority == OverflowPriority::AlwaysOverflow && canOverflow(m_items[i]))
        {
            shown[i]=false;
            forced=forced || !m_info[i].separator;
        }
    }

    int width=rowWidth(shown);

    if(forced && m_overflowButton)
        width+=(width > 0?Spacing:0)+m_overflowButton->getPreferredSize().width;
    return width+getPadding().left+getPadding().right;
}

int ToolBar::minimumWidth() const
{
    std::vector<bool> shown(m_items.size());
    bool overflows=false;

    for(size_t i=0; i < m_items.size(); ++i)
    {
        shown[i]=!m_info[i].separator && !canOverflow(m_items[i]);
        overflows=overflows || (!m_info[i].separator && canOverflow(m_items[i]));
    }

    int width=rowWidth(shown);

    if(overflows && m_overflowButton)
        width+=(width > 0?Spacing:0)+m_overflowButton->getPreferredSize().width;
    return width+getPadding().left+getPadding().right;
}

void ToolBar::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    // owner-placed, so not counted: measured for their own subtrees, the
    // hidden ones too, so an item shown by the overflow has its size
    for(Base *item : m_items)
    {
        if(item->hidden())
            item->doCalculateSize(skin, dim);
    }
    if(m_overflowButton && m_overflowButton->hidden())
        m_overflowButton->doCalculateSize(skin, dim);

    const gweni::Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        m_preferredSize.width=preferredFromContent(fullWidth(), Dim::X);
        return;
    }

    int height=m_overflowButton?m_overflowButton->getPreferredSize().height:0;

    for(size_t i=0; i < m_items.size(); ++i)
    {
        if(!m_info[i].separator)
            height=std::max(height, m_items[i]->getPreferredSize().height);
    }
    m_preferredSize.height=preferredFromContent(height+padding.top+padding.bottom, Dim::Y);
}

// Which items show needs only this control's width, final in the X arrange,
// and the items' preferred widths, so it is decided before the items are
// placed, under a PlacementScope (the items are owner-placed and none of it
// changes a preferred size). Their y and heights wait for the Y pass.
void ToolBar::arrange(skin::Base *skin, Dim dim)
{
    Base *focused=(dim == Dim::X)?focusedItem():nullptr;
    // the overflow button's open menu (or a submenu of it) holds the focus
    // for it
    const bool menuFocused=(dim == Dim::X) && !focused && overflowMenuFocused();
    std::vector<bool> wasHidden;

    if(focused || menuFocused)
    {
        for(Base *item : m_items)
            wasHidden.push_back(item->hidden());
    }

    {
        PlacementScope placement;

        if(dim == Dim::X)
            handleOverflow();
        else
        {
            const gweni::Padding &padding=getPadding();
            const int content=std::max(0, getHeight()-padding.top-padding.bottom);

            for(size_t i=0; i < m_items.size(); ++i)
            {
                Base *item=m_items[i];

                if(item->hidden())
                    continue;
                if(m_info[i].separator)
                    placeChild(item, item->getX(), padding.top+SeparatorInset, item->getWidth(),
                        std::max(0, content-2*SeparatorInset));
                else if(fillsHeight(i))
                    placeChild(item, item->getX(), padding.top, item->getWidth(), content);
                else
                {
                    const int height=std::min(content, item->getPreferredSize().height);

                    placeChild(item, item->getX(), padding.top+(content-height)/2, item->getWidth(), height);
                }
            }
            if(m_overflowButton && !m_overflowButton->hidden())
                placeChild(m_overflowButton, m_overflowButton->getX(), padding.top,
                    m_overflowButton->getWidth(), content);
        }
    }
    // the focus moves outside the placement, which it changes nothing of
    if(focused || menuFocused)
        passFocusOn(focused, menuFocused, wasHidden);
    if(dim == Dim::X)
        postMenuCheck();

    ParentClass::arrange(skin, dim);
}

// While the overflow menu is open every other menu is closed (opening it
// closed them), so any focused Menu is it or one of its open submenus.
bool ToolBar::overflowMenuFocused() const
{
    return m_overflowButton && m_overflowButton->isMenuOpen() && gweni_cast<Menu>(KeyboardFocus);
}

// The open menu lists the overflowed set it was built from; one the layout
// changed since, while the overflow button still shows, is stale (a hidden
// button's menu the canvas closes itself).
bool ToolBar::menuStale() const
{
    return m_overflowButton && m_overflowButton->isMenuOpen() && !m_overflowButton->hidden()
        && (m_menuSerial != m_overflowSerial);
}

// Closed, not refilled: its rows and highlight would change under the user.
// A focus it holds goes back to the overflow button (Menu's hand-back).
void ToolBar::closeStaleMenu()
{
    if(menuStale())
        m_overflowButton->closeMenu();
}

// After an X arrange, a stale menu is closed in the next update (a close in
// the pass would re-dirty it).
void ToolBar::postMenuCheck()
{
    if(!menuStale())
        return;

    Canvas *canvas=getCanvas();

    if(!canvas)
        return;

    const size_t id=getControlId();

    canvas->post([id]()
        {
            if(auto *bar=gweni_cast<ToolBar>(Base::getControlById(id)))
                bar->closeStaleMenu();
        });
}

Base *ToolBar::focusedItem() const
{
    Base *from=KeyboardFocus;

    while(from && from->getParent() != this)
        from=from->getParent();
    if(!from || ((from != m_overflowButton) && (indexOf(from) < 0)))
        return nullptr;
    return from;
}

// A hidden item keeps the focus until the layout ends, when the input code
// drops it; the keyboard stays in the toolbar instead, on the control that
// now shows what the hidden one did.
void ToolBar::passFocusOn(Base *focused, bool menuFocused, const std::vector<bool> &wasHidden)
{
    // A hidden overflow button's menu is closed by the canvas in the next
    // update (a close here would re-dirty the pass), handing back the focus
    // it holds only if it still does: given on here, it stays in the bar.
    if(menuFocused)
    {
        if(!overflowMenuFocused())
            return;
        focused=m_overflowButton;
    }
    else if(focusedItem() != focused)
        return;
    if(!focused || !focused->hidden())
        return;

    Base *target=nullptr;

    if(focused != m_overflowButton)
    {
        // gone into the menu, which the overflow button opens
        if(canFocus(m_overflowButton))
            target=m_overflowButton;
    }
    else
    {
        for(size_t i=0; i < m_items.size() && !target; ++i)
        {
            if((i < wasHidden.size()) && wasHidden[i] && canFocus(m_items[i]))
                target=m_items[i];
        }
    }
    if(!target)
    {
        const std::vector<Base *> items=toolItems();

        if(!items.empty())
            target=items.front();
    }
    if(target)
        target->focus();
}

void ToolBar::handleOverflow()
{
    const gweni::Padding &padding=getPadding();
    const int available=getWidth()-padding.left-padding.right;
    const size_t count=m_items.size();
    std::vector<bool> shown(count);
    bool forced=false;

    for(size_t i=0; i < count; ++i)
    {
        const bool always=(m_info[i].priority == OverflowPriority::AlwaysOverflow) && canOverflow(m_items[i]);

        shown[i]=!always;
        forced=forced || (always && !m_info[i].separator);
    }

    int width=rowWidth(shown);
    bool overflow=forced || width > available;

    if(overflow)
    {
        const int button=m_overflowButton?m_overflowButton->getPreferredSize().width:0;

        // the Normal items go from the end until the rest and the button fit
        for(size_t n=count; n > 0 && (width+(width > 0?Spacing:0)+button) > available; --n)
        {
            const size_t i=n-1;

            if(!shown[i] || m_info[i].separator || !canOverflow(m_items[i]))
                continue;
            shown[i]=false;
            width=rowWidth(shown);
        }

        // nothing went (every item stays): no button
        bool any=false;

        for(size_t i=0; i < count && !any; ++i)
            any=!shown[i] && !m_info[i].separator;
        overflow=any;
    }

    bool changed=false;
    int x=padding.left;

    for(size_t i=0; i < count; ++i)
    {
        Base *item=m_items[i];
        const bool itemShown=shown[i];
        const bool overflowed=!itemShown && (!m_info[i].separator || overflow);

        if(overflowed != m_info[i].overflowed)
        {
            m_info[i].overflowed=overflowed;
            changed=true;
        }
        item->setHidden(!itemShown);
        if(!itemShown)
            continue;

        const int slot=itemWidth(i);

        if(m_info[i].separator)
            placeChild(item, x+(slot-1)/2, item->getY(), 1, item->getHeight());
        else
            placeChild(item, x, item->getY(), slot, item->getHeight());
        x+=slot+Spacing;
    }

    if(m_overflowButton)
    {
        m_overflowButton->setHidden(!overflow);
        if(overflow)
        {
            const int button=m_overflowButton->getPreferredSize().width;

            // at the right edge, inside the bar even when the AlwaysShow
            // items do not fit (they run on under it, so it stays in reach)
            placeChild(m_overflowButton, std::max(padding.left, padding.left+available-button),
                m_overflowButton->getY(), button, m_overflowButton->getHeight());
        }
    }

    if(changed)
    {
        ++m_overflowSerial;
        // an item may have gone from under a resting mouse, or the overflow
        // button come under it
        if(Canvas *canvas=getCanvas())
            canvas->hoverMayHaveMoved();
    }
}

///////////////////////////////////////////////////////////////////////////
//ToolBar: the overflow menu

size_t ToolBar::idOf(const Base *control)
{
    return control?const_cast<Base *>(control)->getControlId():0;
}

String ToolBar::imageNameOf(const Button *button)
{
    return button?button->getImageName():String();
}

void ToolBar::addMenuEntry(MenuItem *entry, Base *source, Base *item, bool itemText)
{
    MenuEntry e;

    e.itemText=itemText;
    e.entry=idOf(entry);
    e.source=idOf(source);
    e.item=idOf(item);
    m_menuEntries.push_back(e);
    entry->onMenuItemSelected.add(this, &ToolBar::onMenuEntryChosen);
}

void ToolBar::copyMenu(Menu *from, Menu *to, Base *item, bool dividerFirst)
{
    if(!from)
        return;

    bool pendingDivider=dividerFirst;
    bool any=dividerFirst;

    for(Base *child : from->getChildren())
    {
        if(!child || child->hidden())
            continue;
        if(gweni_cast<MenuDivider>(child))
        {
            pendingDivider=any;
            continue;
        }

        MenuItem *source=gweni_cast<MenuItem>(child);

        if(!source)
            continue;
        if(pendingDivider)
            to->addDivider();
        pendingDivider=false;
        any=true;

        MenuItem *entry=to->addItem(source->getText(), source->getImageName());

        // a submenu is copied whole, its items chosen as themselves
        if(source->hasMenu())
        {
            copyMenu(source->getMenu(), entry->getMenu(), item, false);
            // kept in step with its original as a leaf is (choosing it only
            // opens its submenu, so its source is never pressed)
            addMenuEntry(entry, source, item);
            continue;
        }
        entry->setCheckable(source->isCheckable());
        addMenuEntry(entry, source, item);
    }
}

void ToolBar::buildMenu(Menu *menu)
{
    // the old entries go (hidden at once, deleted with the delayed deletes)
    for(Base *child : menu->getChildren())
    {
        if(child)
            child->setHidden(true);
    }
    menu->clearItems();
    // as wide as a fresh Menu, which its items widen (Menu::onAddItem only
    // grows it, so the widest set it ever held would stay)
    menu->setWidth(FreshMenuWidth);
    m_menuEntries.clear();

    bool pendingDivider=false;
    bool any=false;

    for(size_t i=0; i < m_items.size(); ++i)
    {
        if(!m_info[i].overflowed)
            continue;

        Base *item=m_items[i];

        if(m_info[i].separator)
        {
            pendingDivider=any;
            continue;
        }
        if(pendingDivider)
            menu->addDivider();
        pendingDivider=false;
        any=true;

        // an addControl item, whatever control it is (a Button or a
        // DropDownButton among them too): its overflow text, nothing pressed
        if(m_info[i].control)
        {
            MenuItem *entry=menu->addItem(m_info[i].overflowText);

            addMenuEntry(entry, nullptr, item);
        }
        else if(ToolButton *button=gweni_cast<ToolButton>(item))
        {
            MenuItem *entry=menu->addItem(button->getMenuText(), imageNameOf(button));

            entry->setCheckable(button->isToggle());
            addMenuEntry(entry, button, button);
        }
        else if(SplitButton *split=gweni_cast<SplitButton>(item))
        {
            MenuItem *parent=menu->addItem(split->getText(), imageNameOf(split->getMainPart()));
            Menu *submenu=parent->getMenu();
            MenuItem *main=submenu->addItem(split->getText());

            main->setCheckable(split->isToggle());
            addMenuEntry(main, split->getMainPart(), split, true);
            // a divider between it and its menu's items, if it has any
            copyMenu(split->getMenu(), submenu, split, true);
            addMenuEntry(parent, nullptr, split, true);
        }
        else if(DropDownButton *dropDown=gweni_cast<DropDownButton>(item))
        {
            MenuItem *parent=menu->addItem(dropDown->getText(), imageNameOf(dropDown));

            copyMenu(dropDown->getMenu(), parent->getMenu(), dropDown, false);
            addMenuEntry(parent, nullptr, dropDown, true);
        }
    }

    m_menuSerial=m_overflowSerial;
    m_menuBuiltFor=idOf(menu);
    ++m_menuBuilds;
}

void ToolBar::prepareOverflowMenu(Menu *menu)
{
    if(!menu)
        return;
    // a Menu made again since (the button's menu deleted) is built anew
    if((m_menuSerial != m_overflowSerial) || (m_menuBuiltFor != idOf(menu)))
        buildMenu(menu);

    std::vector<Menu *> resizedMenus;
    const auto setText=[&](MenuItem *entry, const String &text)
    {
        if(entry->getText() == text)
            return;

        entry->setText(text);
        Menu *parent=entry->parentMenu();

        if(parent && std::find(resizedMenus.begin(), resizedMenus.end(), parent) == resizedMenus.end())
            resizedMenus.push_back(parent);
    };

    // every entry as its item is now
    for(const MenuEntry &e : m_menuEntries)
    {
        MenuItem *entry=gweni_cast<MenuItem>(getControlById(e.entry));
        Base *item=getControlById(e.item);

        if(!entry || !item)
            continue;

        Base *source=e.source?getControlById(e.source):nullptr;

        // a copy whose original menu item was deleted or hidden since the
        // build is there no more: disabled, not chosen
        if(e.source && (!source || (gweni_cast<MenuItem>(source) && source->hidden())))
        {
            entry->setDisabled(true);
            continue;
        }

        const Base *enabledBy=source?source:item;

        entry->setDisabled(const_cast<Base *>(enabledBy)->isDisabled() || item->isDisabled());

        if(MenuItem *original=gweni_cast<MenuItem>(source))
        {
            setText(entry, original->getText());
            entry->setCheckable(original->isCheckable());
            entry->setChecked(original->isChecked());
        }
        else if(!source && !e.itemText && (indexOf(item) >= 0) && m_info[indexOf(item)].control)
            setText(entry, m_info[indexOf(item)].overflowText);
        else
        {
            // a drop-down's or a SplitButton's own entry, and its main action's
            if(e.itemText)
            {
                if(SplitButton *split=gweni_cast<SplitButton>(item))
                    setText(entry, split->getText());
                else if(Button *button=gweni_cast<Button>(item))
                    setText(entry, button->getText());
            }
            if(Button *button=gweni_cast<Button>(source))
            {
                if(ToolButton *tool=gweni_cast<ToolButton>(button))
                    setText(entry, tool->getMenuText());
                entry->setCheckable(button->isToggle());
                entry->setChecked(button->isToggle() && button->getToggleState());
            }
        }
    }

    // Menu::onAddItem sizes the item to its content before docking stretches
    // it, then grows the menu by that width plus its icon/edge allowance.
    // A cached entry's new text needs the same sizing, starting fresh so a
    // shorter label can shrink the menu too. This is before openNextTo() and
    // its layout pass; entries stay put, and unchanged menus are untouched.
    for(Menu *resized : resizedMenus)
    {
        int width=FreshMenuWidth;

        for(Base *child : resized->getChildren())
        {
            MenuItem *entry=gweni_cast<MenuItem>(child);

            if(!entry || entry->hidden())
                continue;
            entry->sizeToContents();
            width=std::max(width, entry->getWidth()+10+32);
        }
        resized->setWidth(width);
    }
}

Base *ToolBar::getMenuEntrySource(const MenuItem *entry) const
{
    const size_t id=idOf(entry);

    for(const MenuEntry &e : m_menuEntries)
    {
        if(e.entry == id)
            return getControlById(e.source?e.source:e.item);
    }
    return nullptr;
}

MenuItem *ToolBar::getMenuEntry(const Base *source) const
{
    if(!source)
        return nullptr;

    const size_t id=idOf(source);

    for(const MenuEntry &e : m_menuEntries)
    {
        if(e.source == id || (!e.source && e.item == id))
            return gweni_cast<MenuItem>(getControlById(e.entry));
    }
    return nullptr;
}

// The entry has closed the menus and, a checkable one, flipped its check;
// its item now does what its own press does.
void ToolBar::onMenuEntryChosen(event::Info info)
{
    const size_t id=idOf(info.control);
    const MenuEntry *found=nullptr;

    for(const MenuEntry &e : m_menuEntries)
    {
        if(e.entry == id)
        {
            found=&e;
            break;
        }
    }
    if(!found)
        return;

    const MenuEntry entry=*found;
    Base *source=entry.source?getControlById(entry.source):nullptr;
    Base *item=getControlById(entry.item);

    if(!item)
        return;

    if(Button *button=gweni_cast<Button>(source))
    {
        if(source->isDisabled())
            return;
        button->onPress(event::Info(button));
    }

    // its check follows the item (the press may have been refused)
    if(MenuItem *menuEntry=gweni_cast<MenuItem>(getControlById(entry.entry)))
    {
        if(MenuItem *original=gweni_cast<MenuItem>(source))
            menuEntry->setChecked(original->isChecked());
        else if(Button *button=gweni_cast<Button>(source))
        {
            if(button->isToggle())
                menuEntry->setChecked(button->getToggleState());
        }
    }

    onOverflowItemChosen.call(this, event::Info(item));
}

///////////////////////////////////////////////////////////////////////////
//ToolBar: the keyboard

bool ToolBar::canFocus(const Base *item) const
{
    if(!item || item->hidden() || const_cast<Base *>(item)->isDisabled())
        return false;
    if(item == m_overflowButton)
        return true;

    const int i=indexOf(item);

    return i >= 0 && !m_info[i].separator && !m_info[i].control;
}

std::vector<Base *> ToolBar::toolItems() const
{
    std::vector<Base *> items;

    for(Base *item : m_items)
    {
        if(canFocus(item))
            items.push_back(item);
    }
    if(canFocus(m_overflowButton))
        items.push_back(m_overflowButton);
    return items;
}

Base *ToolBar::getTabStop() const
{
    Base *last=getControlById(m_tabStop);

    if(last && last->getParent() == this && canFocus(last))
        return last;

    const std::vector<Base *> items=toolItems();

    return items.empty()?nullptr:items.front();
}

void ToolBar::itemFocused(Base *item)
{
    m_tabStop=idOf(item);
}

bool ToolBar::moveFocus(int steps, bool toEnd)
{
    const std::vector<Base *> items=toolItems();

    if(items.empty())
        return false;

    Base *target=nullptr;

    if(toEnd)
        target=(steps < 0)?items.front():items.back();
    else
    {
        // the tool item holding the focus (a SplitButton holds it itself)
        Base *from=KeyboardFocus;

        while(from && from->getParent() != this)
            from=from->getParent();

        const auto at=std::find(items.begin(), items.end(), from);

        if(at == items.end())
            return false;

        const int index=static_cast<int>(at-items.begin())+steps;

        target=items[static_cast<size_t>(gweni::clamp(index, 0, static_cast<int>(items.size())-1))];
    }
    target->focus();
    return true;
}

namespace
{
// Whether the focus is on (or in) one of the toolbar's tool items.
bool focusInToolItem(const ToolBar *bar)
{
    Base *from=KeyboardFocus;

    while(from && from->getParent() != bar)
        from=from->getParent();
    return from && bar->canFocus(from);
}
}

bool ToolBar::onKeyLeft(bool down)
{
    if(!focusInToolItem(this))
        return false;
    return down?moveFocus(-1):true;
}

bool ToolBar::onKeyRight(bool down)
{
    if(!focusInToolItem(this))
        return false;
    return down?moveFocus(1):true;
}

bool ToolBar::onKeyHome(bool down)
{
    if(!focusInToolItem(this))
        return false;
    return down?moveFocus(-1, true):true;
}

bool ToolBar::onKeyend(bool down)
{
    if(!focusInToolItem(this))
        return false;
    return down?moveFocus(1, true):true;
}

}//namespace controls
}//namespace gweni
