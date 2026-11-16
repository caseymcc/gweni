/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_toolBar_h_
#define _gweni_controls_toolBar_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/controls/dropDownButton.h>
#include <gweni/controls/splitButton.h>

#include <vector>

namespace gweni
{
namespace controls
{
class MenuItem;
class Separator;
class ToolBar;

//! What a toolbar's tool buttons show (Qt's toolButtonStyle).
enum class ToolBarDisplay
{
    //! The icon alone, the text its tooltip (the text, for a button with no
    //! icon).
    IconOnly,
    //! The text alone (the icon, for a button with no text).
    TextOnly,
    //! The icon, then the text.
    IconAndText
};

//! Where a toolbar item goes when the bar is too narrow for every item
//! (WinUI's IsDynamicOverflowEnabled and DynamicOverflowOrder).
enum class OverflowPriority
{
    //! Always in the bar, however narrow it is.
    AlwaysShow,
    //! In the bar while it fits; the Normal items overflow from the end.
    Normal,
    //! Always in the overflow menu.
    AlwaysOverflow
};

//! A toolbar's tool button: a flat Button showing an icon, a text or both,
//! as its toolbar's display says, as large as its content.
class GWENI_EXPORT ToolButton: public Button
{
public:

    GWENI_CONTROL(ToolButton, Button);

public:
    //! The room around the content, in units.
    static constexpr int SidePadding=6;
    static constexpr int PaddingY=3;
    //! Between the icon and the text.
    static constexpr int ImageGap=4;

    void setText(const String &str, bool doEvents=true) override;
    void setImage(const String &name, bool center=false) override;

    //! What it shows of its icon and text; set by its toolbar.
    void setDisplay(ToolBarDisplay display);
    ToolBarDisplay getDisplay() const { return m_display; }

    bool hasImage() const { return m_image != nullptr; }
    //! Whether its icon and its text are shown, by its display, its icon and
    //! its text.
    bool imageShows() const;
    bool textShows() const;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

    //! Its toolbar's Tab stop alone is tabable (ToolBar::getTabStop()).
    bool isTabable() override;
    void onKeyboardFocus() override;
    //! Return presses it, as Space does.
    bool onKeyReturn(bool down) override;

protected:
    int imageWidth() const;
    int imageHeight() const;
    int contentWidth() const;
    //! The text is the tooltip in IconOnly; the tooltip it had before (the
    //! app's own, or none) is put aside meanwhile and given back after.
    void updateToolTip();

    ToolBarDisplay m_display=ToolBarDisplay::IconAndText;
    //! Whether the tooltip is the text, set by updateToolTip().
    bool m_textToolTip=false;
    //! The tooltip put aside while the text is the tooltip.
    String m_appToolTip;
};

namespace internal
{

//! A toolbar's overflow button: a DropDownButton showing the chevron alone,
//! whose menu holds the items the bar has no room for.
class GWENI_EXPORT ToolBarOverflowButton: public DropDownButton
{
public:

    GWENI_CONTROL(ToolBarOverflowButton, DropDownButton);

public:
    //! Its width, in units.
    static constexpr int Width=24;

    ToolBar *getToolBar() const;

    void calculateSize(skin::Base *skin, Dim dim) override;
    bool isTabable() override;
    void onKeyboardFocus() override;

protected:
    //! The toolbar fills or updates the menu (ToolBar::prepareOverflowMenu).
    void aboutToOpenMenu(Menu *menu) override;
};

//! A toolbar's drop-down item (ToolBar::addDropDown): a DropDownButton that
//! is a Tab stop only as its toolbar's one (ToolBar::getTabStop()).
class GWENI_EXPORT ToolBarDropDown: public DropDownButton
{
public:

    GWENI_CONTROL(ToolBarDropDown, DropDownButton);

public:
    bool isTabable() override;
    void onKeyboardFocus() override;
};

//! A toolbar's split button item (ToolBar::addSplitButton), a Tab stop as
//! ToolBarDropDown is.
class GWENI_EXPORT ToolBarSplitButton: public SplitButton
{
public:

    GWENI_CONTROL(ToolBarSplitButton, SplitButton);

public:
    bool isTabable() override;
    void onKeyboardFocus() override;
};

}//namespace internal

//! A row of tool buttons, toggles, separators, drop-down buttons and any
//! other controls (Qt's QToolBar, WinUI's CommandBar) whose items that do not
//! fit go into an overflow menu at its right end and come back as it widens.
//!
//! Every item is made through the toolbar (addButton, addToggle,
//! addSeparator, addDropDown, addSplitButton, addControl) and is one of its
//! children in item order, owner-placed: the toolbar sets their bounds and
//! their visibility (an item's own setHidden is the toolbar's to call), so it
//! lays them out left to right, Spacing apart inside its padding, each its
//! preferred width and, but for an addControl one (centred at its preferred
//! height), the content height; a separator takes SeparatorSlot units with
//! its rule in the middle, SeparatorInset short of the top and bottom.
//!
//! Its preferred width is the width that shows every item but the
//! AlwaysOverflow ones (fullWidth(): those items left out, the overflow
//! button they force counted), and its height the tallest item's, wherever
//! they are: what overflows changes no preferred size. Its minimum
//! width (minimumWidth()) is the AlwaysShow items' and the overflow button's.
//!
//! The overflow is decided in its X arrange, once its width is final, as
//! TabControl's is: the items are placed left to right while they fit, and
//! when one does not (or an item is AlwaysOverflow) the overflow button shows
//! at the right end and the row keeps its room, the Normal items overflowing
//! from the end, one at a time, until the rest fits. The overflowed items are
//! hidden. A separator never starts or ends the shown row, nor follows another
//! shown separator. An addControl item with no overflow text has no menu form
//! and is never overflowed, as an AlwaysShow item. Below minimumWidth(),
//! when the AlwaysShow items alone do not fit, the overflow button stays at
//! the toolbar's right edge, inside it, so the menu is still in reach, and
//! the AlwaysShow items are placed at their widths all the same, those that
//! do not fit running on under the button (which is drawn and hit in front
//! of them) and past the toolbar's right edge, where they are drawn (it does
//! not clip, shouldClip(); an ancestor that does cuts them) but cannot be hit.
class GWENI_EXPORT ToolBar: public Base
{
public:

    GWENI_CONTROL(ToolBar, Base);

public:
    //! The room inside its bounds and between two items, in units.
    static constexpr int Padding=3;
    static constexpr int Spacing=2;
    //! A separator's room in the row, and how far its rule stops short of the
    //! content's top and bottom.
    static constexpr int SeparatorSlot=9;
    static constexpr int SeparatorInset=3;
    //! The overflow menu's width as it is rebuilt: a fresh Menu's, which its
    //! items widen.
    static constexpr int FreshMenuWidth=10;

    //! A tool button showing \p text and the image \p icon, as the display
    //! says.
    ToolButton *addButton(const String &text, const String &icon=String());
    //! A tool button that toggles (Button::setIsToggle).
    ToolButton *addToggle(const String &text, const String &icon=String(), bool on=false);
    //! A vertical Separator.
    Separator *addSeparator();
    //! A DropDownButton, its menu the app's to fill.
    DropDownButton *addDropDown(const String &text, const String &icon=String());
    //! A SplitButton, its menu the app's to fill.
    SplitButton *addSplitButton(const String &text, const String &icon=String());
    //! Any control (a search TextBox), made a child of the toolbar. With an
    //! \p overflowText it may overflow and shows in the menu as that text;
    //! with none it is always in the bar.
    Base *addControl(Base *control, const String &overflowText=String());

    //! The items in order (the overflow button is none).
    const std::vector<Base *> &getItems() const { return m_items; }
    size_t itemCount() const { return m_items.size(); }

    //! An item's priority (Normal by default). An addControl item with no
    //! overflow text stays in the bar whatever its priority.
    void setOverflowPriority(Base *item, OverflowPriority priority);
    OverflowPriority getOverflowPriority(const Base *item) const;
    //! An addControl item's text in the overflow menu; empty for none.
    void setOverflowText(Base *item, const String &text);
    String getOverflowText(const Base *item) const;
    //! Whether the item can go into the overflow menu.
    bool canOverflow(const Base *item) const;

    //! Whether the last layout put the item in the overflow menu.
    bool isOverflowed(const Base *item) const;
    //! The overflowed items in order, separators included (the menu decides
    //! which of those it shows).
    std::vector<Base *> getOverflowedItems() const;
    //! Raised each time the overflowed set changes.
    size_t getOverflowSerial() const { return m_overflowSerial; }

    //! What the tool buttons show; drop-downs, split buttons and addControl
    //! items always show what they were made with.
    void setDisplay(ToolBarDisplay display);
    ToolBarDisplay getDisplay() const { return m_display; }

    internal::ToolBarOverflowButton *getOverflowButton() const { return m_overflowButton; }

    //! \name The overflow menu
    //! The overflow button's Menu, built from getOverflowedItems() as it
    //! opens, again only when the overflowed set changed since it was last
    //! built: a tool button is a MenuItem with its text and icon, a toggle a
    //! checkable one, an overflowed separator a MenuDivider (never first,
    //! last or after another), a drop-down a submenu holding copies of its
    //! menu's items (a SplitButton's with its main action first, then a
    //! divider if its menu has items), an addControl item a MenuItem with its
    //! overflow text. Each
    //! open brings every entry's text, disabled state and check up to date
    //! with its item. Choosing an entry fires what pressing its item would:
    //! a tool button's onPress (a toggle flipping its state, which the entry's
    //! check shows), the SplitButton main part's, a drop-down menu item's own
    //! onPress (so its onMenuItemSelected); then onOverflowItemChosen, with
    //! the toolbar item in info.control (an addControl item's only event).
    //! \{
    //! Builds or updates the menu; the overflow button calls it as the menu
    //! opens.
    void prepareOverflowMenu(Menu *menu);
    //! The times the menu was built.
    size_t getMenuBuildCount() const { return m_menuBuilds; }
    //! The toolbar item (or drop-down menu item) a menu entry stands for,
    //! null for none.
    Base *getMenuEntrySource(const MenuItem *entry) const;
    //! The menu entry standing for \p source (a toolbar item, or one of a
    //! drop-down's menu items), null while it has none.
    MenuItem *getMenuEntry(const Base *source) const;
    gweni::event::Caller onOverflowItemChosen;
    //! \}

    //! \name The keyboard
    //! The tool items (tool buttons, drop-downs, split buttons and the
    //! overflow button) are one Tab stop, getTabStop(): the last one focused
    //! while it is shown and enabled, else the first that is. Left and Right
    //! (and Home, End) move the focus over the shown, enabled tool items in
    //! order, the overflow button last, stopping at the ends; separators and
    //! addControl items (which keep their own keys and Tab stop) are passed
    //! over. Space and Return press the focused item; on the overflow button
    //! they open the menu navigating by keyboard (DropDownButton's).
    //! \{
    Base *getTabStop() const;
    //! Whether \p item is a tool item that can take the focus now.
    bool canFocus(const Base *item) const;
    //! A tool item took the focus: it is the Tab stop from now on.
    void itemFocused(Base *item);
    //! Focuses the tool item \p steps away from the focused one (the Tab
    //! stop when none is), or the first (last) with \p toEnd; false if none.
    bool moveFocus(int steps, bool toEnd=false);
    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    //! \}

    //! The width that shows every item but the AlwaysOverflow ones, and the
    //! overflow button if there are any: its preferred width with no width
    //! asked for (a Fixed toolbar's preferred width is its request).
    int fullWidth() const;
    //! The least width that shows every AlwaysShow item and the overflow
    //! button (the button only if some item may overflow).
    int minimumWidth() const;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;
    void onChildRemoved(Base *child) override;
    //! It does not clip its items (Label's way): a clip box of its bounds
    //! would be every item's, so each width change would give every shown
    //! item a new one, and only the moved items may change. The items are
    //! placed inside it but for the AlwaysShow ones that do not fit.
    bool shouldClip() override { return false; }

protected:
    struct ItemInfo
    {
        OverflowPriority priority=OverflowPriority::Normal;
        String overflowText;
        //! An addControl item (its overflow text decides whether it can go).
        bool control=false;
        bool separator=false;
        bool overflowed=false;
    };

    //! Adds \p item (a child already) at the end, owner-placed.
    void addItem(Base *item, bool separator, bool control);
    int indexOf(const Base *item) const;
    //! A tool button's or a drop-down's display.
    void applyDisplay(Base *item);
    //! The width an item takes in the row.
    int itemWidth(size_t i) const;
    //! Which items show in a bar of \p shown (true: in the bar), separators
    //! settled: one shows only between two shown items with no shown
    //! separator since the first. Returns the row's width.
    int rowWidth(std::vector<bool> &shown) const;
    //! Decides the overflow at the current width and places the items in X.
    void handleOverflow();
    //! Whether the item has the bar's content height (not an addControl one).
    bool fillsHeight(size_t i) const;

    //! A menu entry: its MenuItem's id and its source's (a toolbar item, a
    //! SplitButton's main part, or a drop-down's menu item), and the toolbar
    //! item it is under.
    struct MenuEntry
    {
        size_t entry=0;
        size_t source=0;
        size_t item=0;
        //! A drop-down's or a SplitButton's own entry, its text the item's.
        bool itemText=false;
    };
    //! The tool items in focus order: the shown items that are no separator
    //! nor addControl one, then the overflow button.
    std::vector<Base *> toolItems() const;
    void buildMenu(Menu *menu);
    //! Copies \p from's items (recursively) into \p to, under \p item, a
    //! divider before them with \p dividerFirst (none if it has no item).
    void copyMenu(Menu *from, Menu *to, Base *item, bool dividerFirst);
    void addMenuEntry(MenuItem *entry, Base *source, Base *item, bool itemText=false);
    void onMenuEntryChosen(event::Info info);
    //! A control's id, or 0.
    static size_t idOf(const Base *control);
    //! A button's image name, empty for none or no button.
    static String imageNameOf(const Button *button);

    std::vector<Base *> m_items;
    std::vector<ItemInfo> m_info;
    internal::ToolBarOverflowButton *m_overflowButton=nullptr;
    ToolBarDisplay m_display=ToolBarDisplay::IconAndText;
    size_t m_overflowSerial=0;
    std::vector<MenuEntry> m_menuEntries;
    //! The overflow serial the menu was built at; none yet.
    size_t m_menuSerial=static_cast<size_t>(-1);
    size_t m_menuBuilds=0;
    //! The last tool item focused (by id), 0 for none.
    size_t m_tabStop=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_toolBar_h_
