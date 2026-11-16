/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/toolBar.h>
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

bool ToolButton::onKeyReturn(bool down)
{
    return onKeySpace(down);
}

void ToolButton::setText(const String &str, bool doEvents)
{
    ParentClass::setText(str, doEvents);
    m_text->setHidden(!textShows());
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

void ToolButton::updateToolTip()
{
    const bool iconOnly=imageShows() && !textShows() && !m_text->getText().empty();

    setToolTipText(iconOnly?m_text->getText():String());
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

int ToolBar::minimumWidth() const
{
    std::vector<bool> shown(m_items.size());
    bool overflows=false;

    for(size_t i=0; i < m_items.size(); ++i)
    {
        shown[i]=!m_info[i].separator && !canOverflow(m_items[i]);
        overflows=overflows || canOverflow(m_items[i]);
    }

    int width=rowWidth(shown);

    if(overflows && m_overflowButton)
        width+=(width > 0?Spacing:0)+m_overflowButton->getPreferredSize().width;
    return width+2*Padding;
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
        std::vector<bool> shown(m_items.size(), true);

        m_preferredSize.width=preferredFromContent(rowWidth(shown)+padding.left+padding.right, Dim::X);
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

    ParentClass::arrange(skin, dim);
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

            placeChild(m_overflowButton, std::max(x, padding.left+available-button), m_overflowButton->getY(),
                button, m_overflowButton->getHeight());
        }
    }

    if(changed)
        ++m_overflowSerial;
}

///////////////////////////////////////////////////////////////////////////
//ToolBar: the overflow menu

size_t ToolBar::idOf(const Base *control)
{
    return control?const_cast<Base *>(control)->getControlId():0;
}

void ToolBar::addMenuEntry(MenuItem *entry, Base *source, Base *item)
{
    MenuEntry e;

    e.entry=idOf(entry);
    e.source=idOf(source);
    e.item=idOf(item);
    m_menuEntries.push_back(e);
    entry->onMenuItemSelected.add(this, &ToolBar::onMenuEntryChosen);
}

void ToolBar::copyMenu(Menu *from, Menu *to, Base *item)
{
    if(!from)
        return;

    bool pendingDivider=false;
    bool any=false;

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

        MenuItem *entry=to->addItem(source->getText());

        // a submenu is copied whole, its items chosen as themselves
        if(source->hasMenu())
        {
            copyMenu(source->getMenu(), entry->getMenu(), item);
            addMenuEntry(entry, nullptr, item);
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

        if(ToolButton *button=gweni_cast<ToolButton>(item))
        {
            MenuItem *entry=menu->addItem(button->getText());

            entry->setCheckable(button->isToggle());
            addMenuEntry(entry, button, button);
        }
        else if(SplitButton *split=gweni_cast<SplitButton>(item))
        {
            MenuItem *parent=menu->addItem(split->getText());
            Menu *submenu=parent->getMenu();
            MenuItem *main=submenu->addItem(split->getText());

            main->setCheckable(split->isToggle());
            addMenuEntry(main, split->getMainPart(), split);
            submenu->addDivider();
            copyMenu(split->getMenu(), submenu, split);
            addMenuEntry(parent, nullptr, split);
        }
        else if(DropDownButton *dropDown=gweni_cast<DropDownButton>(item))
        {
            MenuItem *parent=menu->addItem(dropDown->getText());

            copyMenu(dropDown->getMenu(), parent->getMenu(), dropDown);
            addMenuEntry(parent, nullptr, dropDown);
        }
        else
        {
            MenuItem *entry=menu->addItem(m_info[i].overflowText);

            addMenuEntry(entry, item, item);
        }
    }

    m_menuSerial=m_overflowSerial;
    ++m_menuBuilds;
}

void ToolBar::prepareOverflowMenu(Menu *menu)
{
    if(!menu)
        return;
    if(m_menuSerial != m_overflowSerial)
        buildMenu(menu);

    // every entry as its item is now
    for(const MenuEntry &e : m_menuEntries)
    {
        MenuItem *entry=gweni_cast<MenuItem>(getControlById(e.entry));
        Base *item=getControlById(e.item);

        if(!entry || !item)
            continue;

        Base *source=getControlById(e.source);
        const Base *enabledBy=source?source:item;

        entry->setDisabled(const_cast<Base *>(enabledBy)->isDisabled() || item->isDisabled());

        if(MenuItem *original=gweni_cast<MenuItem>(source))
        {
            entry->setText(original->getText());
            entry->setCheckable(original->isCheckable());
            entry->setChecked(original->isChecked());
        }
        else if((source == item) && (indexOf(item) >= 0) && m_info[indexOf(item)].control)
            entry->setText(m_info[indexOf(item)].overflowText);
        else if(Button *button=gweni_cast<Button>(source))
        {
            if(gweni_cast<ToolButton>(button))
                entry->setText(button->getText());
            if(button->isToggle())
                entry->setChecked(button->getToggleState());
        }
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
    Base *source=getControlById(entry.source);
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
