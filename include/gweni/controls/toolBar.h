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

#include <vector>

namespace gweni
{
namespace controls
{
class Separator;
class SplitButton;
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

protected:
    int imageWidth() const;
    int imageHeight() const;
    int contentWidth() const;
    //! The text's tooltip in IconOnly, none otherwise.
    void updateToolTip();

    ToolBarDisplay m_display=ToolBarDisplay::IconAndText;
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
//! Its preferred width is every item's, and its height the tallest item's,
//! wherever they are: what overflows changes no preferred size. Its minimum
//! width (minimumWidth()) is the AlwaysShow items' and the overflow button's.
//!
//! The overflow is decided in its X arrange, once its width is final, as
//! TabControl's is: the items are placed left to right while they fit, and
//! when one does not (or an item is AlwaysOverflow) the overflow button shows
//! at the right end and the row keeps its room, the Normal items overflowing
//! from the end, one at a time, until the rest fits. The overflowed items are
//! hidden. A separator never starts or ends the shown row, nor follows another
//! shown separator. An addControl item with no overflow text has no menu form
//! and is never overflowed, as an AlwaysShow item; when the AlwaysShow items
//! alone do not fit, they are placed at their widths all the same and the
//! toolbar (which clips its children) cuts them at its right edge, the
//! overflow button then holding the rest of the row past them.
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

    //! What the tool buttons (and the drop-downs) show.
    void setDisplay(ToolBarDisplay display);
    ToolBarDisplay getDisplay() const { return m_display; }

    internal::ToolBarOverflowButton *getOverflowButton() const { return m_overflowButton; }

    //! The least width that shows every AlwaysShow item and the overflow
    //! button (the button only if some item may overflow).
    int minimumWidth() const;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;
    void onChildRemoved(Base *child) override;

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

    std::vector<Base *> m_items;
    std::vector<ItemInfo> m_info;
    internal::ToolBarOverflowButton *m_overflowButton=nullptr;
    ToolBarDisplay m_display=ToolBarDisplay::IconAndText;
    size_t m_overflowSerial=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_toolBar_h_
