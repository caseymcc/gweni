/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_navigationrail_h_
#define _gweni_controls_navigationrail_h_

#include <gweni/controls/selectable.h>

namespace gweni
{
namespace controls
{

//! A navigation row: a Selectable's press and selection handling, with an
//! ImagePanel and a Label of its own. Clicking a selected row keeps it selected.
//! The label is a child so a collapsed rail can hide it without hiding the row.
class GWENI_EXPORT NavigationItem: public Selectable
{
public:

    GWENI_CONTROL(NavigationItem, Selectable);

public:

    static constexpr int IconSize=24;
    static constexpr int IconGap=8;
    static constexpr int PaddingX=12;
    static constexpr int PaddingY=8;

    void setText(const String &text, bool doEvents=true) override;
    const String &getText() const override;
    //! Loads the named resource as Button::setImage does. An empty name hides
    //! the image, keeping its icon space and the ImagePanel available.
    void setImage(const String &name, bool center=false) override;
    ImagePanel *getIcon() const { return m_image; }
    //! The row's visible label; customise its font and text alignment here.
    Label *getLabel() const { return m_label; }

    void setTextColor(const Color &color) override;
    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

private:

    Label *m_label=nullptr;
};

//! A side rail whose main and footer items share one SelectableGroup.
//! Docked Left by default, it fills its parent's height. Items dock Top in
//! the space below the optional header and above the bottom-docked footer.
//! The rail owns its header and containers, and their children.
class GWENI_EXPORT NavigationRail: public Base
{
public:

    GWENI_CONTROL(NavigationRail, Base);

public:

    ~NavigationRail() override;

    NavigationItem *addItem(const String &label, const String &image="", const String &name="");
    //! Only items belonging to this rail may be selected. Null clears it.
    void setSelected(NavigationItem *item);
    NavigationItem *getSelected() const;

    //! Any controls may be placed here. NavigationItems made with newChild()
    //! automatically join the same group as the main items.
    Base *getFooter() const { return m_footer; }
    //! Transfers the control to the header slot and docks it Top. Replacing
    //! or clearing the header deletes the old one.
    void setHeader(Base *header);
    Base *getHeader() const;

    //! Once per real selection change: controlCaller is this rail, control
    //! is the newly selected NavigationItem (or null for none).
    event::Caller onSelectionChange;

private:

    void groupSelectionChanged(event::Info info);

    SelectableGroup m_selection;
    Base *m_items=nullptr;
    Base *m_footer=nullptr;
    Base *m_headerSlot=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_navigationrail_h_
