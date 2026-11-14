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

class Badge;
namespace internal { class NavigationRailItems; }

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
    //! the image, keeping its icon space and the ImagePanel available. A
    //! badge keeps a blank icon anchor visible without painting an image.
    void setImage(const String &name, bool center=false) override;
    ImagePanel *getIcon() const { return m_image; }
    //! The row's visible label; customise its font and text alignment here.
    Label *getLabel() const { return m_label; }

    //! Lazily makes a Badge attached to the icon's top-right corner. A count
    //! switches out of dot mode; turning the dot off restores the count.
    void setBadgeCount(int count);
    void setBadgeDot(bool dot);
    Badge *getBadge() const { return m_badge; }

    void setTextColor(const Color &color) override;
    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

private:

    friend class NavigationRail;
    friend class internal::NavigationRailItems;
    void setCollapsed(bool collapsed);
    Badge *ensureBadge();

    Label *m_label=nullptr;
    Badge *m_badge=nullptr;
    bool m_collapsed=false;
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

    //! The app decides when to collapse. Labels stay alive, hidden, and their
    //! text becomes each item's tooltip. Expanded, those tooltips are cleared.
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }
    //! A positive width fixes the expanded width; zero restores the widest
    //! item's preferred width. The collapsed width is icon plus row padding.
    void setExpandedWidth(int width);
    int getExpandedWidth() const { return m_expandedWidth; }
    void calculateSize(skin::Base *skin, Dim dim) override;

    //! Once per real selection change: controlCaller is this rail, control
    //! is the newly selected NavigationItem (or null for none).
    event::Caller onSelectionChange;
    //! Once per real change, with integer 1 for collapsed and 0 for expanded.
    event::Caller onCollapsedChanged;

private:

    void groupSelectionChanged(event::Info info);

    SelectableGroup m_selection;
    Base *m_items=nullptr;
    Base *m_footer=nullptr;
    Base *m_headerSlot=nullptr;
    bool m_collapsed=false;
    int m_expandedWidth=0;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_navigationrail_h_
