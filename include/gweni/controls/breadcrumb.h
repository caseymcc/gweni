/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_breadcrumb_h_
#define _gweni_controls_breadcrumb_h_

#include <gweni/controls/button.h>
#include <gweni/controls/dropDownButton.h>

#include <vector>

namespace gweni
{
namespace controls
{

class Breadcrumb;

namespace internal
{

//! A persistent path item. Button supplies its press and hover handling;
//! Breadcrumb gives it its index and decides whether it takes input.
class GWENI_EXPORT BreadcrumbItem: public Button
{
public:
    GWENI_CONTROL(BreadcrumbItem, Button);

public:
    size_t getIndex() const { return m_index; }
    bool isCurrent() const { return m_current; }
    bool isClickable() const { return m_clickable; }

    void onPress(event::Info info) override;
    //! Return presses it as Space does (Button's).
    bool onKeyReturn(bool down) override;
    bool onKeySpace(bool down) override;
    void setDisabled(bool disabled) override;
    void updateColors() override;
    //! The path is one Tab stop: Breadcrumb::getTabStop().
    bool isTabable() override;

private:
    friend class controls::Breadcrumb;

    void setCurrent(bool current, bool clickable);

    Breadcrumb *m_breadcrumb=nullptr;
    size_t m_index=0;
    bool m_current=false;
    bool m_clickable=true;
};

//! The separator before a path item: a right chevron, or Label's text.
class GWENI_EXPORT BreadcrumbSeparator: public Label
{
public:
    GWENI_CONTROL(BreadcrumbSeparator, Label);

public:
    static constexpr int ChevronSize=8;
    static constexpr int ChevronWidth=16;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void updateColors() override;
};

//! The "…" in place of the collapsed middle items: a DropDownButton drawn as
//! a path item, whose menu lists the collapsed items in path order. The menu
//! is filled as it opens, and only when the collapsed set (or one of its
//! labels) differs from the one it was last filled with.
class GWENI_EXPORT BreadcrumbEllipsis: public DropDownButton
{
public:
    GWENI_CONTROL(BreadcrumbEllipsis, DropDownButton);

public:
    ~BreadcrumbEllipsis() override;

    void openMenu(MenuFocus focus=MenuFocus::FirstItem) override;
    void updateColors() override;
    bool isTabable() override;

private:
    friend class controls::Breadcrumb;

    Breadcrumb *m_breadcrumb=nullptr;
};

}//namespace internal

//! A clickable path whose last item is the current location. Items and
//! separators are kept across pushes and pops; only the changed tail is
//! made or removed. Clicking reports an index and leaves navigation to the
//! app (which may popTo that index from its handler).
//!
//! When the path is wider than the Breadcrumb, its X arrange (as
//! TabControl::handleOverflow does, owner-placed parts under a
//! PlacementScope, its preferred size always the whole path's) collapses
//! the items after the first getItemsBeforeCollapse() into an ellipsis
//! DropDownButton, one more per step towards the current item, until the
//! rest fits or only getItemsAfterCollapse() remain after it. A collapsed
//! item and its separator are hidden, not deleted; the first collapsed
//! item's separator stays, before the ellipsis. If even that does not fit,
//! the last item is cut at the Breadcrumb's right edge (clipped by its own
//! bounds: gweni's Text has no ellipsis truncation), and so is any part
//! that would start past it.
//!
//! The keyboard (WinUI's BreadcrumbBar): one Tab stop, the focused part or
//! else the first shown clickable item (or the ellipsis); Left/Right move
//! between the shown clickable items and the ellipsis, Home/End to the first
//! and last of them; Return and Space activate an item, and open the
//! ellipsis's menu with its first item highlighted, as Alt+Down does.
class GWENI_EXPORT Breadcrumb: public Base
{
public:
    GWENI_CONTROL(Breadcrumb, Base);
    GWENI_DRAWS_NOTHING;

public:
    struct Item
    {
        String label;
        String name; //!< Optional app id or name, as SearchComboBox::Option.
    };

    ~Breadcrumb() override;

    //! Sets the labels, clearing their app data. Existing item controls at
    //! shared indices stay in place, and equal labels do no work.
    void setItems(std::vector<String> items);
    void push(const String &text, const String &name="");
    //! Keeps items through index (inclusive); an out-of-range index does
    //! nothing. setItems({}) clears the path.
    void popTo(size_t index);

    size_t getItemCount() const { return m_items.size(); }
    const Item &getItem(size_t index) const { return m_items[index]; }
    //! App data changes no drawing and fires no event.
    void setItemData(size_t index, const String &name);
    const String &getItemData(size_t index) const { return m_items[index].name; }

    internal::BreadcrumbItem *getItemControl(size_t index) const;
    //! The separator before index, null for the first or an invalid index.
    internal::BreadcrumbSeparator *getSeparatorControl(size_t index) const;

    void setLastItemClickable(bool clickable);
    bool isLastItemClickable() const { return m_lastClickable; }

    //! Empty uses right chevrons (the default).
    void setSeparatorText(const String &text);
    const String &getSeparatorText() const { return m_separatorText; }

    //! How many leading items never collapse (default 1, WinUI's; 0 puts
    //! the ellipsis first), and how many trailing ones (default 1, the
    //! current item; at least 1). MUI's itemsBeforeCollapse/AfterCollapse.
    void setItemsBeforeCollapse(size_t count);
    size_t getItemsBeforeCollapse() const { return m_itemsBefore; }
    void setItemsAfterCollapse(size_t count);
    size_t getItemsAfterCollapse() const { return m_itemsAfter; }

    //! The collapsed items are [getCollapsedBegin(), getCollapsedEnd()),
    //! empty while everything fits (as of the last X arrange, clamped when
    //! the path shrinks to keep the trailing items outside the menu).
    size_t getCollapsedBegin() const { return m_collapseBegin; }
    size_t getCollapsedEnd() const { return m_collapseEnd; }
    bool isCollapsed(size_t index) const { return index >= m_collapseBegin && index < m_collapseEnd; }
    //! Whether the last item is narrower than its text (cut at the edge).
    bool isLastItemCut() const { return m_lastCut; }

    internal::BreadcrumbEllipsis *getEllipsisControl() const { return m_ellipsis; }
    //! How many times the ellipsis's menu has been filled.
    size_t getMenuBuildCount() const { return m_menuBuilds; }

    //! The one Tab stop: the focused part, else the first shown clickable
    //! item or the ellipsis; null when none takes the focus.
    Base *getTabStop();

    void setDisabled(bool disabled) override;

    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;

    //! The info's integer is the path index; control is this Breadcrumb.
    event::Caller onItemClicked;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;
    //! Its parts are cut at its right edge, so it clips nothing: a clip
    //! region following its width would re-stamp every part on a resize.
    bool shouldClip() override { return false; }

private:
    friend class internal::BreadcrumbItem;
    friend class internal::BreadcrumbEllipsis;

    void itemClicked(size_t index);
    void updateCurrent();
    void removeTail(size_t count);
    //! Decides the collapsed range from the width and places the parts in X.
    void handleOverflow();
    //! Fills the ellipsis's menu if the collapsed labels changed.
    void updateMenu();
    void menuItemPicked(event::Info info);
    //! The shown parts that take the focus, in path order.
    std::vector<Base *> focusOrder();
    bool moveFocus(int steps);
    bool focusEnd(bool last);

    std::vector<Item> m_items;
    std::vector<internal::BreadcrumbItem *> m_itemControls;
    //! Indexed like the items, with null at zero.
    std::vector<internal::BreadcrumbSeparator *> m_separators;
    String m_separatorText;
    bool m_lastClickable=false;

    internal::BreadcrumbEllipsis *m_ellipsis=nullptr;
    size_t m_itemsBefore=1;
    size_t m_itemsAfter=1;
    size_t m_collapseBegin=0;
    size_t m_collapseEnd=0;
    bool m_lastCut=false;
    //! The labels and first index the menu was last filled with.
    std::vector<String> m_menuLabels;
    size_t m_menuBegin=0;
    bool m_menuBuilt=false;
    size_t m_menuBuilds=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_breadcrumb_h_
