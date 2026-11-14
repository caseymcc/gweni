/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/navigationRail.h>
#include <gweni/controls/badge.h>
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
    m_selection.onSelectionChange.add(this, &NavigationRail::groupSelectionChanged);

    m_headerSlot=newChild<Base>();
    m_headerSlot->setDock(DockPosition::Top);
    m_headerSlot->setSizeFlags({SizeFlag::Expand, SizeFlag::Elastic});

    auto *footer=newChild<internal::NavigationRailItems>();
    footer->selection=&m_selection;
    footer->setDock(DockPosition::Bottom);
    m_footer=footer;

    auto *items=newChild<internal::NavigationRailItems>();
    items->selection=&m_selection;
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
    return m_headerSlot->getChild(0);
}

void NavigationRail::setHeader(Base *header)
{
    Base *previous=getHeader();
    if(previous == header)
        return;

    if(previous)
        delete previous;
    if(header)
    {
        header->setParent(m_headerSlot);
        header->setDock(DockPosition::Top);
    }
}

}//namespace controls
}//namespace gweni
