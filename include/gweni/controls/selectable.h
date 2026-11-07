/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_selectable_h_
#define _gweni_controls_selectable_h_

#include <gweni/controls/button.h>

#include <vector>

namespace gweni
{
namespace controls
{

class SelectableGroup;

//! A row that highlights while hovered and stays highlighted while selected
//! (imgui's Selectable). It is a Button for the press handling (the click
//! lands on release over the row, Space presses it), but draws through its own
//! renderer: nothing while idle, a surface for hover, press and selection.
//! It is docked Top and grows along x only, so by default it is the full width
//! of its parent (or of its layout cell) and its text's height tall. A left
//! click toggles it; in a SelectableGroup, or after setToggleable(false), a
//! click only selects. The text sits left-aligned and vertically centred in
//! the text padding.
class GWENI_EXPORT Selectable: public Button
{
public:

    GWENI_CONTROL(Selectable, Button);

public:

    ~Selectable() override;

    bool isSelected() const { return m_selected; }

    //! Selects or deselects the row. A real change repaints it (its background
    //! and its text colour) and, with doEvents, fires onSelect or onDeselect;
    //! selecting a row of a group deselects the group's previous selection.
    virtual void setSelected(bool selected, bool doEvents=true);

    //! Whether a click on a selected row deselects it (the default); a group
    //! turns it off for its members.
    void setToggleable(bool toggleable) { m_toggleable=toggleable; }
    bool isToggleable() const { return m_toggleable; }

    //! The group the row is a member of, or null (SelectableGroup::add/remove).
    SelectableGroup *getGroup() const { return m_group; }

    void onPress(event::Info) override;
    bool onKeyReturn(bool down) override;
    void toggle() override;
    void updateColors() override;

    gweni::event::Caller onSelect;
    gweni::event::Caller onDeselect;

private:

    friend class SelectableGroup;

    bool m_selected=false;
    bool m_toggleable=true;
    SelectableGroup *m_group=nullptr;
};

//! At most one of its Selectables selected: selecting one deselects the
//! previous one, and a click on the selected one keeps it. It is a plain object
//! rather than a container control, so its rows can sit anywhere (in different
//! parents, in a layout, later in a table's cells); whoever creates it owns it.
//! A member deleted before the group leaves it, and the group's destructor
//! releases its members, so either may go first.
class GWENI_EXPORT SelectableGroup
{
public:

    SelectableGroup()=default;
    ~SelectableGroup();

    SelectableGroup(const SelectableGroup &)=delete;
    SelectableGroup &operator=(const SelectableGroup &)=delete;

    //! Makes the row a member (leaving any other group) and turns its toggling
    //! off. A row added selected becomes the group's selection.
    void add(Selectable *member);

    //! The row leaves the group, keeping its selection state and getting its
    //! toggling back; if it was the group's selection the group has none
    //! (onSelectionChange fires; a deleted member leaves without the event).
    void remove(Selectable *member);

    Selectable *getSelected() const { return m_selected; }
    const std::vector<Selectable *> &getMembers() const { return m_members; }

    //! Fired once per change of getSelected() (to a row or to none), with the
    //! newly selected row as the control, or null for none.
    gweni::event::Caller onSelectionChange;

private:

    friend class Selectable;

    void memberSelected(Selectable *member, bool selected, bool doEvents);
    void forget(Selectable *member, bool notify);

    std::vector<Selectable *> m_members;
    Selectable *m_selected=nullptr;
};

}
}
#endif//_gweni_controls_selectable_h_
