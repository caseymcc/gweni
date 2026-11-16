/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/breadcrumb.h>
#include <gweni/controls/canvas.h>
#include <gweni/gweni.h>

#include <algorithm>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(BreadcrumbItem)
{
    setPlacement(Placement::Owner);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setTextPadding(Padding(6, 4, 6, 4));
    setTextAlignment(Alignment::Left|Alignment::CenterV);
    setCursor(cursor::Finger);
}

void BreadcrumbItem::setCurrent(bool current, bool clickable)
{
    if((current == m_current) && (clickable == m_clickable))
        return;

    m_current=current;
    m_clickable=clickable;
    setMouseInputEnabled(clickable);
    setCursor(clickable?cursor::Finger:cursor::Normal);
    if(!clickable)
    {
        setDepressed(false);
        if(KeyboardFocus == this)
            unfocus();
    }
    redraw();
    if(Canvas *canvas=getCanvas())
        canvas->hoverMayHaveMoved();
}

void BreadcrumbItem::onPress(event::Info /*info*/)
{
    if(m_breadcrumb && m_clickable && !isDisabled())
        m_breadcrumb->itemClicked(m_index);
}

void BreadcrumbItem::updateColors()
{
    const auto &colors=getSkin()->Colors.Breadcrumb;

    setTextColor(m_current?colors.CurrentText:colors.Text);
}

GWENI_CONTROL_CONSTRUCTOR(BreadcrumbSeparator)
{
    setPlacement(Placement::Owner);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setTextPadding(Padding(4, 0, 4, 0));
    setTextAlignment(Alignment::Center);
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
}

void BreadcrumbSeparator::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);
    if(getText().empty())
    {
        if(dim == Dim::X)
            m_preferredSize.width=preferredFromContent(ChevronWidth, dim);
        else
            m_preferredSize.height=preferredFromContent(ChevronSize, dim);
    }
}

void BreadcrumbSeparator::updateColors()
{
    setTextColor(getSkin()->Colors.Breadcrumb.Separator);
}

}//namespace internal

GWENI_CONTROL_CONSTRUCTOR(Breadcrumb)
{
    setDock(DockPosition::Top);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
}

Breadcrumb::~Breadcrumb()
{
    for(internal::BreadcrumbItem *item : m_itemControls)
        item->m_breadcrumb=nullptr;
}

void Breadcrumb::push(const String &text, const String &name)
{
    const size_t index=m_items.size();
    internal::BreadcrumbSeparator *separator=nullptr;

    if(index > 0)
    {
        separator=newChild<internal::BreadcrumbSeparator>();
        separator->setText(m_separatorText);
        separator->updateColors();
    }
    internal::BreadcrumbItem *item=newChild<internal::BreadcrumbItem>();

    item->m_breadcrumb=this;
    item->m_index=index;
    item->setText(text);
    item->setDisabled(isDisabled());
    m_items.push_back({text, name});
    m_itemControls.push_back(item);
    m_separators.push_back(separator);
    updateCurrent();
    invalidate();
}

void Breadcrumb::removeTail(size_t count)
{
    while(m_items.size() > count)
    {
        internal::BreadcrumbItem *item=m_itemControls.back();

        item->m_breadcrumb=nullptr;
        item->setHidden(true);
        item->delayedDelete();
        if(internal::BreadcrumbSeparator *separator=m_separators.back())
        {
            separator->setHidden(true);
            separator->delayedDelete();
        }
        m_items.pop_back();
        m_itemControls.pop_back();
        m_separators.pop_back();
    }
}

void Breadcrumb::popTo(size_t index)
{
    if(index >= m_items.size() || index+1 == m_items.size())
        return;

    removeTail(index+1);
    updateCurrent();
    invalidate();
}

void Breadcrumb::setItems(std::vector<String> items)
{
    const size_t shared=std::min(items.size(), m_items.size());

    for(size_t i=0; i < shared; ++i)
    {
        m_items[i].label=items[i];
        m_items[i].name.clear();
        m_itemControls[i]->setText(items[i]);
    }
    if(items.size() < m_items.size())
    {
        removeTail(items.size());
        updateCurrent();
        invalidate();
    }
    for(size_t i=shared; i < items.size(); ++i)
        push(items[i]);
}

void Breadcrumb::setItemData(size_t index, const String &name)
{
    if(index < m_items.size())
        m_items[index].name=name;
}

internal::BreadcrumbItem *Breadcrumb::getItemControl(size_t index) const
{
    return index < m_itemControls.size()?m_itemControls[index]:nullptr;
}

internal::BreadcrumbSeparator *Breadcrumb::getSeparatorControl(size_t index) const
{
    return index < m_separators.size()?m_separators[index]:nullptr;
}

void Breadcrumb::updateCurrent()
{
    // Only the old and new current item's state can change; the setters
    // leave every equal state alone, including its text colour and slots.
    for(size_t i=0; i < m_itemControls.size(); ++i)
    {
        const bool current=i+1 == m_itemControls.size();

        m_itemControls[i]->setCurrent(current, !current || m_lastClickable);
    }
}

void Breadcrumb::setLastItemClickable(bool clickable)
{
    if(clickable == m_lastClickable)
        return;

    m_lastClickable=clickable;
    updateCurrent();
}

void Breadcrumb::setSeparatorText(const String &text)
{
    if(text == m_separatorText)
        return;

    m_separatorText=text;
    for(internal::BreadcrumbSeparator *separator : m_separators)
    {
        if(!separator)
            continue;
        separator->setText(text);
        separator->addStateChange(StateChange_Background);
    }
}

void Breadcrumb::itemClicked(size_t index)
{
    if(isDisabled() || index >= m_items.size() || (index+1 == m_items.size() && !m_lastClickable))
        return;

    event::Info info(this);

    info.integer=index;
    onItemClicked.call(this, info);
}

void Breadcrumb::setDisabled(bool disabled)
{
    if(disabled == isDisabled())
        return;

    ParentClass::setDisabled(disabled);
    for(internal::BreadcrumbItem *item : m_itemControls)
        item->setDisabled(disabled);
}

void Breadcrumb::calculateSize(skin::Base *skin, Dim dim)
{
    // Owner-placed parts are measured by Base but do not count toward its
    // size. The path's natural width is the sum, its height the tallest part.
    ParentClass::calculateSize(skin, dim);

    int size=0;

    for(size_t i=0; i < m_itemControls.size(); ++i)
    {
        const Size &item=m_itemControls[i]->getPreferredSize();
        const Size separator=m_separators[i]?m_separators[i]->getPreferredSize():Size();

        if(dim == Dim::X)
            size+=item.width+separator.width;
        else
            size=std::max(size, std::max(item.height, separator.height));
    }
    const Padding &padding=getPadding();

    if(dim == Dim::X)
        m_preferredSize.width=preferredFromContent(size+padding.left+padding.right, dim);
    else
        m_preferredSize.height=preferredFromContent(size+padding.top+padding.bottom, dim);
}

void Breadcrumb::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();
    int x=padding.left;
    const int height=std::max(0, getHeight()-padding.top-padding.bottom);

    // Place before Base enters the children, including its changed-child
    // walk. Placement never changes the parts' requested sizes.
    for(size_t i=0; i < m_itemControls.size(); ++i)
    {
        for(Base *part : {static_cast<Base *>(m_separators[i]), static_cast<Base *>(m_itemControls[i])})
        {
            if(!part)
                continue;
            if(dim == Dim::X)
            {
                const int width=part->getPreferredSize().width;

                placeChild(part, x, part->getY(), width, part->getHeight());
                x+=width;
            }
            else
                placeChild(part, part->getX(), padding.top, part->getWidth(), height);
        }
    }
    ParentClass::arrange(skin, dim);
}

}//namespace controls
}//namespace gweni
