/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/toolBar.h>
#include <gweni/controls/imagePanel.h>
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
    DropDownButton *button=newChild<DropDownButton>();

    button->setText(text);
    if(!icon.empty())
        button->setImage(icon);
    button->setDock(DockPosition::None);
    addItem(button, false, false);
    return button;
}

SplitButton *ToolBar::addSplitButton(const String &text, const String &icon)
{
    SplitButton *button=newChild<SplitButton>();

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

}//namespace controls
}//namespace gweni
