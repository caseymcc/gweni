/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/navigationRail.h>
#include <gweni/controls/badge.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/imagePanel.h>
#include <gweni/skin.h>

#include <algorithm>
#include <limits>

namespace gweni
{
namespace controls
{
namespace internal
{

//! A badge may need a visible anchor even when no image was supplied. Keep
//! ImagePanel's resource loading and slot handling for images, but paint
//! nothing for that blank anchor rather than a missing-image placeholder.
class NavigationIcon: public ImagePanel
{
public:
    GWENI_CONTROL_INLINE(NavigationIcon, ImagePanel) {}
    GWENI_DRAWS_NOTHING

    void setImage(const String &name) override
    {
        if(name.empty())
        {
            m_texture.name.clear();
            redraw();
        }
        else
            ParentClass::setImage(name);
    }

    void render(skin::Base *skin) override
    {
        if(!getImageName().empty())
            ParentClass::render(skin);
        else if(getStateChange() != StateChange_Nothing)
        {
            Base::render(skin);
            if(m_primitiveId != std::numeric_limits<size_t>::max())
                skin->getRenderer()->hidePrimitive(m_primitiveId);
        }
    }
};

//! Both item containers enrol their rows as children are added, including a
//! newChild<NavigationItem>() in the footer. Reparenting takes a row out again.
class NavigationRailItems: public Base
{
public:

    GWENI_CONTROL_INLINE(NavigationRailItems, Base)
    {
        setSizeFlags({SizeFlag::Expand, SizeFlag::Elastic});
    }

    GWENI_DRAWS_NOTHING

    SelectableGroup *selection=nullptr;
    NavigationRail *rail=nullptr;
    bool collapsed=false;

    void setCollapsed(bool value)
    {
        collapsed=value;
        for(Base *child : getActualChildren())
            if(auto *item=gweni_cast<NavigationItem>(child))
                item->setCollapsed(value);
    }

    void onChildAdded(Base *child) override
    {
        ParentClass::onChildAdded(child);
        if(selection)
        {
            if(auto *item=gweni_cast<NavigationItem>(child))
            {
                item->m_rail=rail;
                selection->add(item);
                item->setCollapsed(collapsed);
            }
        }
    }

    void onChildRemoved(Base *child) override
    {
        ParentClass::onChildRemoved(child);
        if(selection)
        {
            if(auto *item=gweni_cast<NavigationItem>(child))
            {
                if(rail && rail->m_focusedId == item->getControlId())
                    rail->m_focusedId=size_t(-1);
                item->m_rail=nullptr;
                selection->remove(item);
                item->setToggleable(false);
            }
        }
    }
};

}//namespace internal

GWENI_CONTROL_CONSTRUCTOR(NavigationItem)
{
    setToggleable(false);
    setKeyboardInputEnabled(false);
    setTabable(false);
    setPadding(Padding(PaddingX, PaddingY, PaddingX, PaddingY));
    setSize(0, 0);

    // Selectable inherits Label for Button's handling. Its own Text stays
    // empty and hidden; this separate Label is the part the rail can hide.
    m_text->hide();
    m_label=newChild<Label>();
    m_label->setPlacement(Placement::Owner);
    m_label->setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    m_label->setTextAlignment(Alignment::Left|Alignment::CenterV);

    m_image=newChild<internal::NavigationIcon>();
    m_image->setPlacement(Placement::Owner);
    m_image->setSize(IconSize, IconSize);
    m_image->hide();
    // Enrollment precedes init(), so a collapsed container may already have
    // set the flag before these children existed.
    setCollapsed(m_collapsed);
    updateColors();
}

void NavigationItem::setText(const String &text, bool doEvents)
{
    m_label->setText(text, doEvents);
    if(m_collapsed)
        setToolTipText(text);
}

const String &NavigationItem::getText() const
{
    return m_label->getText();
}

void NavigationItem::setImage(const String &name, bool /*center*/)
{
    // Retain the ImagePanel: it is also the anchor for an item's badge.
    if(name != m_image->getImageName())
        m_image->setImage(name);
    m_image->setHidden(name.empty() && !m_badge);
}

void NavigationItem::setCollapsed(bool collapsed)
{
    m_collapsed=collapsed;
    if(m_label)
    {
        m_label->setHidden(collapsed);
        setToolTipText(collapsed?getText():String());
    }
}

Badge *NavigationItem::ensureBadge()
{
    if(!m_badge)
    {
        m_badge=m_image->newChild<Badge>();
        m_badge->attach(m_image, Corner::TopRight);
        m_image->show();
    }
    return m_badge;
}

void NavigationItem::setBadgeCount(int count)
{
    Badge *badge=ensureBadge();
    badge->setDot(false);
    badge->setCount(count);
}

void NavigationItem::setBadgeDot(bool dot)
{
    ensureBadge()->setDot(dot);
}

void NavigationItem::setTextColor(const Color &color)
{
    ParentClass::setTextColor(color);
    if(m_label)
        m_label->setTextColor(color);
}

void NavigationItem::updateColors()
{
    const auto &colors=getSkin()->Colors.NavigationItem;
    setTextColor(isDisabled()?colors.Disabled:isSelected()?colors.Selected:colors.Normal);
}

bool NavigationItem::isNavigationFocused() const
{
    return m_rail && KeyboardFocus == m_rail && m_rail->getFocused() == this;
}

void NavigationItem::onPress(event::Info info)
{
    if(isDisabled())
        return;
    if(m_rail && KeyboardFocus == m_rail)
        m_rail->focusItem(this);
    ParentClass::onPress(info);
}

// Measure the owned parts too (the icon may hold an attached badge), but only
// the icon space and visible label contribute to the row's preferred size.
void NavigationItem::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);
    const Size label=m_label->hidden()?Size(0, 0):m_label->getPreferredSize();
    const Padding &padding=getPadding();

    if(dim == Dim::X)
        m_preferredSize.width=preferredFromContent(padding.left+IconSize
            +(m_label->hidden()?0:IconGap+label.width)+padding.right, dim);
    else
        m_preferredSize.height=preferredFromContent(padding.top+std::max(IconSize, label.height)
            +padding.bottom, dim);
}

void NavigationItem::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        placeChild(m_image, padding.left, m_image->getY(), IconSize, m_image->getHeight());
        const int left=padding.left+IconSize+IconGap;
        placeChild(m_label, left, m_label->getY(), std::max(0, getWidth()-left-padding.right),
            m_label->getHeight());
    }
    else
    {
        placeChild(m_image, m_image->getX(), (getHeight()-IconSize)/2, m_image->getWidth(), IconSize);
        placeChild(m_label, m_label->getX(), padding.top, m_label->getWidth(),
            std::max(0, getHeight()-padding.top-padding.bottom));
    }
    ParentClass::arrange(skin, dim);
}

GWENI_CONTROL_CONSTRUCTOR(NavigationRail)
{
    setDock(DockPosition::Left);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Expand});
    setKeyboardInputEnabled(true);
    setTabable(true);
    m_selection.onSelectionChange.add(this, &NavigationRail::groupSelectionChanged);

    m_headerSlot=newChild<Base>();
    m_headerSlot->setDock(DockPosition::Top);
    m_headerSlot->setSizeFlags({SizeFlag::Expand, SizeFlag::Elastic});

    auto *footer=newChild<internal::NavigationRailItems>();
    footer->selection=&m_selection;
    footer->rail=this;
    footer->setDock(DockPosition::Bottom);
    m_footer=footer;

    auto *items=newChild<internal::NavigationRailItems>();
    items->selection=&m_selection;
    items->rail=this;
    items->setDock(DockPosition::Center);
    m_items=items;
    // newChild() on the rail also places a main item in the item container.
    // Set this only after making the private header and footer.
    m_innerPanel=m_items;
}

NavigationRail::~NavigationRail()
{
    // Members die before Base deletes its children. The group releases all
    // rows itself; the containers must not use it during the subsequent deletes.
    static_cast<internal::NavigationRailItems *>(m_items)->selection=nullptr;
    static_cast<internal::NavigationRailItems *>(m_footer)->selection=nullptr;
    for(Selectable *member : m_selection.getMembers())
        if(auto *item=gweni_cast<NavigationItem>(member))
            item->m_rail=nullptr;
}

NavigationItem *NavigationRail::addItem(const String &label, const String &image, const String &name)
{
    auto *item=newChild<NavigationItem>(name);
    item->setText(label);
    item->setImage(image);
    return item;
}

void NavigationRail::setSelected(NavigationItem *item)
{
    if(item)
    {
        if(item->getGroup() == &m_selection)
            item->setSelected(true);
    }
    else if(Selectable *selected=m_selection.getSelected())
        selected->setSelected(false);
}

NavigationItem *NavigationRail::getSelected() const
{
    return gweni_cast<NavigationItem>(m_selection.getSelected());
}

NavigationItem *NavigationRail::getFocused() const
{
    auto *item=gweni_cast<NavigationItem>(Base::getControlById(m_focusedId));
    return item && item->m_rail == this && item->visible() && !item->isDisabled()?item:nullptr;
}

std::vector<NavigationItem *> NavigationRail::keyboardItems() const
{
    std::vector<NavigationItem *> items;
    for(Base *container : {m_items, m_footer})
        for(Base *child : container->getActualChildren())
            if(auto *item=gweni_cast<NavigationItem>(child))
                if(item->visible() && !item->isDisabled())
                    items.push_back(item);
    return items;
}

void NavigationRail::focusItem(NavigationItem *item)
{
    NavigationItem *previous=getFocused();
    if(previous == item)
        return;
    m_focusedId=item?item->getControlId():size_t(-1);
    if(previous)
        previous->redraw();
    if(item)
        item->redraw();
}

void NavigationRail::onKeyboardFocus()
{
    const auto items=keyboardItems();
    NavigationItem *selected=getSelected();
    if(std::find(items.begin(), items.end(), selected) == items.end())
        selected=items.empty()?nullptr:items.front();
    focusItem(selected);
    // The logical item may be the same one as on the previous focus entry.
    if(selected)
        selected->redraw();
}

void NavigationRail::onLostKeyboardFocus()
{
    if(NavigationItem *item=getFocused())
        item->redraw();
}

bool NavigationRail::moveFocus(bool down, int direction, bool end)
{
    if(KeyboardFocus != this || isDisabled())
        return false;
    if(!down)
        return true;
    const auto items=keyboardItems();
    if(items.empty())
        return true;
    if(end)
        focusItem(direction > 0?items.back():items.front());
    else
    {
        auto current=std::find(items.begin(), items.end(), getFocused());
        const int index=current == items.end()?(direction > 0?-1:0):int(current-items.begin());
        focusItem(items[(index+direction+int(items.size()))%items.size()]);
    }
    return true;
}

bool NavigationRail::onKeyUp(bool down) { return moveFocus(down, -1); }
bool NavigationRail::onKeyDown(bool down) { return moveFocus(down, 1); }
bool NavigationRail::onKeyHome(bool down) { return moveFocus(down, -1, true); }
bool NavigationRail::onKeyend(bool down) { return moveFocus(down, 1, true); }
bool NavigationRail::onKeyReturn(bool down) { return onKeySpace(down); }

bool NavigationRail::onKeySpace(bool down)
{
    if(KeyboardFocus != this || isDisabled())
        return false;
    if(down)
        if(NavigationItem *item=getFocused())
            item->onPress(event::Info(item));
    return true;
}

void NavigationRail::groupSelectionChanged(event::Info info)
{
    onSelectionChange.call(this, info);
}

void NavigationRail::setCollapsed(bool collapsed)
{
    if(m_collapsed == collapsed)
        return;
    m_collapsed=collapsed;
    static_cast<internal::NavigationRailItems *>(m_items)->setCollapsed(collapsed);
    static_cast<internal::NavigationRailItems *>(m_footer)->setCollapsed(collapsed);
    invalidate();
    event::Info info(this);
    info.integer=collapsed?1:0;
    onCollapsedChanged.call(this, info);
}

void NavigationRail::setExpandedWidth(int width)
{
    width=std::max(0, width);
    if(m_expandedWidth == width)
        return;
    m_expandedWidth=width;
    if(!m_collapsed)
        invalidate();
}

void NavigationRail::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);
    if(dim != Dim::X)
        return;

    int width=NavigationItem::IconSize+2*NavigationItem::PaddingX;
    if(!m_collapsed)
    {
        if(m_expandedWidth)
            width=m_expandedWidth;
        else
            for(Base *container : {m_items, m_footer})
                for(Base *child : container->getActualChildren())
                    if(auto *item=gweni_cast<NavigationItem>(child))
                        if(!item->hidden())
                            width=std::max(width, item->getPreferredSize().width);
    }
    m_preferredSize.width=preferredFromContent(width, dim);
}

Base *NavigationRail::getHeader() const
{
    // a replaced header waits in the slot for its delayed delete
    Canvas *canvas=const_cast<NavigationRail *>(this)->getCanvas();
    for(Base *child : m_headerSlot->getActualChildren())
        if(!canvas || !canvas->isPendingDelete(child))
            return child;
    return nullptr;
}

void NavigationRail::setHeader(Base *header)
{
    Base *previous=getHeader();
    if(previous == header)
        return;

    // The new header first, so one inside the old header is moved out of it
    // before that goes.
    if(header)
    {
        header->setParent(m_headerSlot);
        header->setDock(DockPosition::Top);
    }
    // Delayed: the old header may be what is replacing itself (a Menu button's
    // press handler), and its code is still running.
    if(previous)
    {
        previous->setHidden(true);
        previous->delayedDelete();
    }
}

}//namespace controls
}//namespace gweni
