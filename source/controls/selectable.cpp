/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/selectable.h>
#include <gweni/skin.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

// Docked Top and Expand along x: the parent's (or the layout cell's) whole
// width, and the text's height plus its padding tall. Tabable, so Space and
// Return reach it.
GWENI_CONTROL_CONSTRUCTOR(Selectable)
{
    setDock(DockPosition::Top);
    setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});
    setTextPadding(Padding(6, 3, 6, 3));
    setDefaultTextAlignment(Alignment::Left|Alignment::CenterV);
    setKeyboardInputEnabled(true);
    setTabable(true);
}

Selectable::~Selectable()
{
    // no event: the handler may be a parent that is being destroyed
    if(m_group)
        m_group->forget(this, false);
}

void Selectable::setSelected(bool selected, bool doEvents)
{
    if(m_selected == selected)
        return;

    m_selected=selected;
    // the background and the text colour (updateColors), this row only
    redraw();

    if(m_group)
        m_group->memberSelected(this, selected, doEvents);

    if(!doEvents)
        return;

    if(selected)
        onSelect.call(this);
    else
        onDeselect.call(this);
}

void Selectable::toggle()
{
    setSelected(!m_selected);
}

void Selectable::onPress(event::Info info)
{
    if(isDisabled())
        return;

    if(!m_selected)
        setSelected(true);
    else if(m_toggleable)
        setSelected(false);

    ParentClass::onPress(info);
}

bool Selectable::onKeyReturn(bool down)
{
    return onKeySpace(down);
}

void Selectable::updateColors()
{
    const auto &colors=getSkin()->Colors.Selectable;

    if(isDisabled())
        setTextColor(colors.Disabled);
    else if(m_selected)
        setTextColor(colors.Selected);
    else if(isHovered())
        setTextColor(colors.Hover);
    else
        setTextColor(colors.Normal);
}


SelectableGroup::~SelectableGroup()
{
    for(Selectable *member : m_members)
    {
        member->m_group=nullptr;
        member->m_toggleable=true;
    }
}

void SelectableGroup::add(Selectable *member)
{
    if(!member || member->m_group == this)
        return;

    if(member->m_group)
        member->m_group->remove(member);

    m_members.push_back(member);
    member->m_group=this;
    member->m_toggleable=false;

    if(member->isSelected())
        memberSelected(member, true, true);
}

void SelectableGroup::remove(Selectable *member)
{
    if(!member || member->m_group != this)
        return;

    forget(member, true);
    member->m_group=nullptr;
    member->m_toggleable=true;
}

void SelectableGroup::forget(Selectable *member, bool notify)
{
    m_members.erase(std::remove(m_members.begin(), m_members.end(), member), m_members.end());

    if(m_selected == member)
    {
        m_selected=nullptr;
        if(notify)
            onSelectionChange.call(nullptr);
    }
}

// Called by a member whose selection changed. The previous selection is
// deselected through its own setSelected, which calls back here with nothing
// left to do, since m_selected has moved on.
void SelectableGroup::memberSelected(Selectable *member, bool selected, bool doEvents)
{
    if(selected)
    {
        if(m_selected == member)
            return;

        Selectable *previous=m_selected;

        m_selected=member;
        if(previous)
            previous->setSelected(false, doEvents);
    }
    else
    {
        if(m_selected != member)
            return;

        m_selected=nullptr;
    }

    if(doEvents)
        onSelectionChange.call(m_selected);
}

}//namespace controls
}//namespace gweni
