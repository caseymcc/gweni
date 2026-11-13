/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_searchcombobox_h_
#define _gweni_controls_searchcombobox_h_

#include <gweni/controls/button.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/virtualList.h>

#include <vector>

namespace gweni
{
namespace controls
{

class SearchComboBox;

namespace internal
{

//! A SearchComboBox's open list: a Menu on the canvas, opened next to its box
//! (Menu::openNextTo, Below), so the canvas follows the box with it in the
//! frame the box moves, closes it on a press outside it and its box, and
//! leaves a Popup the box is in open under it. It is as wide as its box, set
//! in its own arrange from the box's canvas rect, and holds one
//! SearchComboBoxOptions docked Top, so the Menu's own scrolling is unused.
//! Drawn by Menu's renderer, the skin's lookup walking up the parent types.
class GWENI_EXPORT SearchComboBoxList: public Menu
{
public:

    GWENI_CONTROL(SearchComboBoxList, Menu);

public:
    //! Tells its box: a canvas tearing down can delete the list first.
    ~SearchComboBoxList() override;

    void setComboBox(SearchComboBox *box) { m_box=box; }

    //! Next to its box, as wide as the box's canvas rect, in each axis's
    //! arrange: the box was arranged before it in the pass (the list comes
    //! after the box's top-level ancestor among the canvas's children), its
    //! width final by the Y arrange at the latest (a ScrollControl arranges
    //! its children in X in its Y arrange), which is where the list's own
    //! ScrollControl lays its content out in X.
    void arrange(skin::Base *skin, Dim dim) override;

private:
    //! The width of the box as drawn, or -1 when the list is not open next
    //! to it on the canvas.
    int boxWidth();

    SearchComboBox *m_box=nullptr;
};

//! The list of a SearchComboBox: a VirtualList of SearchComboBoxRows over the
//! box's shown options, as high as the shown rows, up to
//! SearchComboBox::MaxShownRows (scrolling past them). The Menu it fills
//! paints the backdrop, so it draws nothing itself.
class GWENI_EXPORT SearchComboBoxOptions: public VirtualList
{
public:

    GWENI_CONTROL(SearchComboBoxOptions, VirtualList);

public:
    //! In Y, the shown rows' height: min(item count, MaxShownRows) rows.
    void calculateSize(skin::Base *skin, Dim dim) override;
};

//! A row of a SearchComboBox's list, pooled by its SearchComboBoxOptions and
//! bound to a shown option. Drawn as a MenuItem is: nothing while idle, the
//! MenuHover surface while it shows the box's highlighted option (which a
//! mouse move over it makes it). It takes no keyboard focus, so a press on it
//! leaves the focus where it was; a click picks its option (the list's
//! onItemPressed).
class GWENI_EXPORT SearchComboBoxRow: public VirtualListRow
{
public:

    GWENI_CONTROL(SearchComboBoxRow, VirtualListRow);

public:
    void setComboBox(SearchComboBox *box) { m_box=box; }

    //! Whether it shows the box's highlighted option; a change redraws it.
    void setHighlighted(bool highlighted);
    bool isHighlighted() const { return m_highlighted; }

    //! A real move of the mouse over it highlights its option; a hover that
    //! only follows the rows scrolling under a resting mouse does not, so
    //! the keyboard's highlight stays where the keys put it.
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;

    //! MenuItem's colours: Button's, its hover colour while highlighted.
    void updateColors() override;

private:
    SearchComboBox *m_box=nullptr;
    bool m_highlighted=false;
};

}//namespace internal

//! A pick-one box over options held as data (a label and a name each), with
//! its list a VirtualList in a Menu, so the list costs the rows it shows
//! however many options there are. It is a Button, as ComboBox is (hover,
//! press, the selected option's label as its text), drawn like a ComboBox
//! (the textured skin's paintComboBox), and at rest a box showing the same
//! text at the same size draws the same pixels as one.
//!
//! A press opens the list below the box (above it where it would run past
//! the canvas's bottom) and a second press closes it; a click on a row picks
//! that option. While the list is open Up and Down move the highlight
//! through the shown options, wrapping, scrolling it into view, Return picks
//! it and Escape closes the list; while it is closed they change the
//! selection, as a ComboBox's do. A pick fires onSelection when it changes
//! the selection. The list follows the box: opened next to it, it is placed
//! in every layout pass the box moves in, in that frame, and it opens over a
//! Popup the box is in without closing it.
class GWENI_EXPORT SearchComboBox: public Button
{
public:

    GWENI_CONTROL(SearchComboBox, Button);

public:
    //! One option: what the box and its list show, and the name an app keys
    //! it by (getValue()).
    struct Option
    {
        String label;
        String name;
    };

    //! The list (closed or open) goes with the box.
    ~SearchComboBox() override;

    //! The chevron's column and the paddings are a ComboBox's.
    static constexpr int ChevronSize=ComboBox::ChevronSize;
    static constexpr int ChevronInset=ComboBox::ChevronInset;
    static constexpr int VerticalPadding=ComboBox::VerticalPadding;
    //! The most rows the open list shows; past them it scrolls.
    static constexpr size_t MaxShownRows=10;
    //! A row's text padding above and below its line (a MenuItem's).
    static constexpr int RowPaddingY=4;
    //! No option selected or highlighted.
    static constexpr int NoOption=-1;

    //! Appends an option. The first one added to a box with no selection is
    //! selected, with no event (as ComboBox::addItem).
    void addOption(const String &label, const String &name="");
    //! Replaces the options; the selection is the first option, or none.
    void setOptions(std::vector<Option> options);
    //! No options, nothing selected.
    void clearOptions();

    size_t getOptionCount() const { return m_options.size(); }
    const Option &getOption(size_t index) const { return m_options[index]; }

    //! The selected option, or NoOption.
    int getSelectedIndex() const { return m_selected; }
    //! Selects option \p index (NoOption, or one out of range, for none),
    //! firing nothing.
    void setSelectedIndex(int index);
    //! The selected option's name, or its label when it has none; empty with
    //! nothing selected.
    String getValue() override;

    //! Fired once per pick or Up/Down step that changes the selection.
    gweni::event::Caller onSelection;

    void openList();
    void closeList();
    bool isListOpen() const;
    //! The list, made with the box (null once deleted from outside).
    internal::SearchComboBoxList *getList() const { return m_list; }
    internal::SearchComboBoxOptions *getOptionsList() const { return m_rows; }

    //! The options the open list shows, in order (indices into the options):
    //! every option.
    const std::vector<size_t> &getShownOptions() const { return m_matches; }
    //! The option the keys move through the open list, or NoOption.
    int getHighlightedIndex() const { return m_highlight; }
    //! Highlights option \p index (shown in the list), scrolling it into
    //! view; NoOption takes the highlight away.
    void setHighlightedIndex(int index);

    void onPress(event::Info info) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyReturn(bool down) override;
    bool onKeyEscape(bool down) override;

    //! In X, the widest option's label in this box's font as well as the
    //! shown text, with the text padding and the chevron column, as a
    //! ComboBox's: the labels are measured again only when they or the font
    //! (the renderer's text layout) changed.
    void calculateSize(skin::Base *skin, Dim dim) override;
    void updateColors() override;
    void renderFocus(skin::Base *skin) override {}

    bool isMenuComponent() override
    {
        return true;
    }

protected:

    friend class internal::SearchComboBoxList;
    friend class internal::SearchComboBoxRow;

    //! The list is being deleted: it is no longer held.
    void listDeleted();
    //! The list was shown or hidden, by the box or from outside (a press
    //! outside it, Escape over a Popup): the box's open look follows it.
    void listVisibilityChanged(event::Info info);
    void makeList();
    //! Makes \p index the selection and shows its label (NoOption: none,
    //! empty), firing onSelection with \p doEvents when it changed.
    void select(int index, bool doEvents);
    //! Picks option \p index from the list: selects it and closes the list.
    void pick(int index, bool doEvents);
    void rowPressed(event::Info info);
    void bindRow(Base *row, size_t item);
    //! Moves the highlight by \p step through the shown options, wrapping.
    void stepHighlight(int step);
    //! A row was moved over: its option becomes the highlight, not scrolled
    //! to (it is on screen).
    void rowHovered(internal::SearchComboBoxRow *row);
    //! The shown options again: every option, and the list told their count.
    void rebuildMatches();
    //! The row height of the list in this box's list font.
    int rowHeight();
    int optionsTextWidth();

    std::vector<Option> m_options;
    std::vector<size_t> m_matches;
    int m_selected=NoOption;
    int m_highlight=NoOption;

    internal::SearchComboBoxList *m_list=nullptr;
    internal::SearchComboBoxOptions *m_rows=nullptr;

    //! What optionsTextWidth() measured: each option's width, valid while
    //! m_widthsValid and the font, renderer and text layout are these.
    std::vector<int> m_optionWidths;
    bool m_widthsValid=false;
    gweni::Font m_widthsFont;
    const renderer::Base *m_widthsRenderer=nullptr;
    unsigned m_widthsSerial=0;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_searchcombobox_h_
