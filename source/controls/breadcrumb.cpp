/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/breadcrumb.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
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
    setKeyboardInputEnabled(clickable);
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

bool BreadcrumbItem::onKeyReturn(bool down)
{
    return onKeySpace(down);
}

bool BreadcrumbItem::isTabable()
{
    return m_breadcrumb && (m_breadcrumb->getTabStop() == this);
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

GWENI_CONTROL_CONSTRUCTOR(BreadcrumbEllipsis)
{
    setPlacement(Placement::Owner);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    // a path item's padding: no chevron
    setTextPadding(Padding(6, 4, 6, 4));
    setTextAlignment(Alignment::Center);
    setText("\xe2\x80\xa6");
    setCursor(cursor::Finger);
}

BreadcrumbEllipsis::~BreadcrumbEllipsis()
{
    m_breadcrumb=nullptr;
}

void BreadcrumbEllipsis::openMenu(MenuFocus focus)
{
    if(m_breadcrumb && !isDisabled() && visible())
        m_breadcrumb->updateMenu();
    ParentClass::openMenu(focus);
}

void BreadcrumbEllipsis::updateColors()
{
    setTextColor(getSkin()->Colors.Breadcrumb.Text);
}

bool BreadcrumbEllipsis::isTabable()
{
    return m_breadcrumb && (m_breadcrumb->getTabStop() == this);
}

}//namespace internal

GWENI_CONTROL_CONSTRUCTOR(Breadcrumb)
{
    setDock(DockPosition::Top);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
    m_ellipsis=newChild<internal::BreadcrumbEllipsis>();
    m_ellipsis->m_breadcrumb=this;
    m_ellipsis->setHidden(true);
}

Breadcrumb::~Breadcrumb()
{
    for(internal::BreadcrumbItem *item : m_itemControls)
        item->m_breadcrumb=nullptr;
    m_ellipsis->m_breadcrumb=nullptr;
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
    m_ellipsis->setDisabled(disabled);
}

void Breadcrumb::setItemsBeforeCollapse(size_t count)
{
    if(count == m_itemsBefore)
        return;
    m_itemsBefore=count;
    invalidate();
}

void Breadcrumb::setItemsAfterCollapse(size_t count)
{
    count=std::max<size_t>(count, 1);
    if(count == m_itemsAfter)
        return;
    m_itemsAfter=count;
    invalidate();
}

void Breadcrumb::updateMenu()
{
    std::vector<String> labels;

    for(size_t i=m_collapseBegin; i < m_collapseEnd; ++i)
        labels.push_back(m_items[i].label);
    if(m_menuBuilt && (m_menuBegin == m_collapseBegin) && (labels == m_menuLabels))
        return;

    Menu *menu=m_ellipsis->getMenu();

    if(!menu)
        return;

    menu->clearItems();
    // clearItems() deletes them delayed: hidden now, so the open shows the
    // new items alone
    for(auto &&child : const_cast<Base *>(menu->getInnerPanel())->getChildren())
    {
        if(child)
            child->setHidden(true);
    }
    for(size_t i=0; i < labels.size(); ++i)
    {
        MenuItem *item=menu->addItem(labels[i]);

        item->onMenuItemSelected.add(this, &Breadcrumb::menuItemPicked, event::Packet(m_collapseBegin+i));
    }
    m_menuLabels=std::move(labels);
    m_menuBegin=m_collapseBegin;
    m_menuBuilt=true;
    ++m_menuBuilds;
}

void Breadcrumb::menuItemPicked(event::Info info)
{
    if(info.packet && info.packet->isSizeT())
        itemClicked(info.packet->get<size_t>());
}

std::vector<Base *> Breadcrumb::focusOrder()
{
    std::vector<Base *> parts;

    if(isDisabled())
        return parts;

    const auto add=[&](size_t i)
        {
            internal::BreadcrumbItem *item=m_itemControls[i];

            if(item->isClickable() && !item->hidden())
                parts.push_back(item);
        };

    for(size_t i=0; i < m_itemControls.size(); ++i)
    {
        if(i == m_collapseBegin && m_collapseEnd > m_collapseBegin && !m_ellipsis->hidden())
            parts.push_back(m_ellipsis);
        if(!isCollapsed(i))
            add(i);
    }
    return parts;
}

Base *Breadcrumb::getTabStop()
{
    const std::vector<Base *> parts=focusOrder();

    for(Base *part : parts)
    {
        if(part == KeyboardFocus)
            return part;
    }
    return parts.empty()?nullptr:parts.front();
}

bool Breadcrumb::moveFocus(int steps)
{
    const std::vector<Base *> parts=focusOrder();
    const auto at=std::find(parts.begin(), parts.end(), KeyboardFocus);

    if(at == parts.end())
        return false;

    const long index=std::clamp<long>(static_cast<long>(at-parts.begin())+steps, 0,
        static_cast<long>(parts.size())-1);

    parts[static_cast<size_t>(index)]->focus();
    return true;
}

bool Breadcrumb::focusEnd(bool last)
{
    const std::vector<Base *> parts=focusOrder();

    if(parts.empty() || std::find(parts.begin(), parts.end(), KeyboardFocus) == parts.end())
        return false;
    (last?parts.back():parts.front())->focus();
    return true;
}

// Each on its press; a key whose press moved nothing is still the path's.
bool Breadcrumb::onKeyLeft(bool down)
{
    return down?moveFocus(-1):!isDisabled();
}

bool Breadcrumb::onKeyRight(bool down)
{
    return down?moveFocus(1):!isDisabled();
}

bool Breadcrumb::onKeyHome(bool down)
{
    return down?focusEnd(false):!isDisabled();
}

bool Breadcrumb::onKeyend(bool down)
{
    return down?focusEnd(true):!isDisabled();
}

void Breadcrumb::calculateSize(skin::Base *skin, Dim dim)
{
    // Owner-placed parts are measured by Base but do not count toward its
    // size. The path's natural width is the sum, its height the tallest part.
    ParentClass::calculateSize(skin, dim);

    // the collapsed parts and the ellipsis are measured while hidden too, so
    // the arrange can show them at their size
    if(m_ellipsis->hidden())
        m_ellipsis->doCalculateSize(skin, dim);
    for(size_t i=0; i < m_itemControls.size(); ++i)
    {
        if(m_itemControls[i]->hidden())
            m_itemControls[i]->doCalculateSize(skin, dim);
        if(m_separators[i] && m_separators[i]->hidden())
            m_separators[i]->doCalculateSize(skin, dim);
    }

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

// Which items collapse needs only this control's width, final in the X
// arrange, and the parts' preferred widths, so it is decided before the parts
// are placed; hiding and showing them changes no preferred size (the path's
// is always the whole path's).
void Breadcrumb::arrange(skin::Base *skin, Dim dim)
{
    {
        PlacementScope placement;

        if(dim == Dim::X)
            handleOverflow();
        else
        {
            const Padding &padding=getPadding();
            const int height=std::max(0, getHeight()-padding.top-padding.bottom);

            placeChild(m_ellipsis, m_ellipsis->getX(), padding.top, m_ellipsis->getWidth(), height);
            for(size_t i=0; i < m_itemControls.size(); ++i)
            {
                for(Base *part : {static_cast<Base *>(m_separators[i]), static_cast<Base *>(m_itemControls[i])})
                {
                    if(part)
                        placeChild(part, part->getX(), padding.top, part->getWidth(), height);
                }
            }
        }
    }
    ParentClass::arrange(skin, dim);
}

void Breadcrumb::handleOverflow()
{
    const Padding &padding=getPadding();
    const int left=padding.left;
    const int right=std::max(left, getWidth()-padding.right);
    const size_t count=m_items.size();
    const auto separatorWidth=[&](size_t i)
        {
            return m_separators[i]?m_separators[i]->getPreferredSize().width:0;
        };
    const auto width=[&](size_t i)
        {
            return m_itemControls[i]->getPreferredSize().width+separatorWidth(i);
        };

    int total=0;

    for(size_t i=0; i < count; ++i)
        total+=width(i);

    const size_t before=std::min(m_itemsBefore, count);
    const size_t after=std::min(m_itemsAfter, count);
    size_t begin=before;
    size_t end=before;

    if(total > right-left && before+after < count)
    {
        // the parts outside [begin, end), the ellipsis and the separator before
        // it (the first collapsed item's); one more item per step
        const int ellipsis=m_ellipsis->getPreferredSize().width+separatorWidth(begin);
        int shown=total+ellipsis;

        while(end < count-after)
        {
            shown-=width(end);
            ++end;
            if(shown <= right-left)
                break;
        }
    }

    const bool collapsed=end > begin;

    if(!collapsed)
        begin=end=0;
    // a part shown or hidden can move what is under a resting mouse
    if(begin != m_collapseBegin || end != m_collapseEnd)
    {
        if(Canvas *canvas=getCanvas())
            canvas->hoverMayHaveMoved();
    }
    m_collapseBegin=begin;
    m_collapseEnd=end;

    int x=left;
    const auto place=[&](Base *part, bool show, bool cut)
        {
            if(!part)
                return;
            part->setHidden(!show);
            if(!show)
                return;

            const int preferred=part->getPreferredSize().width;
            const int w=cut?std::max(0, std::min(preferred, right-x)):preferred;

            placeChild(part, x, part->getY(), w, part->getHeight());
            x+=w;
        };

    m_lastCut=false;
    for(size_t i=0; i < count; ++i)
    {
        const bool hiddenItem=collapsed && i >= begin && i < end;

        // the first collapsed item's separator goes before the ellipsis
        place(m_separators[i], !hiddenItem || i == begin, false);
        if(collapsed && i == begin)
            place(m_ellipsis, true, false);
        place(m_itemControls[i], !hiddenItem, i+1 == count);
        if(i+1 == count)
            m_lastCut=m_itemControls[i]->getWidth() < m_itemControls[i]->getPreferredSize().width;
    }
    if(!collapsed)
        m_ellipsis->setHidden(true);
}

}//namespace controls
}//namespace gweni
