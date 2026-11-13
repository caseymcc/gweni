/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_searchcombobox_h_
#define _gweni_controls_searchcombobox_h_

#include <gweni/controls/button.h>
#include <gweni/controls/chip.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/virtualList.h>

#include <vector>

namespace gweni
{
namespace controls
{

class SearchComboBox;

namespace internal
{

class SearchComboBoxOptions;

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
    void setOptions(SearchComboBoxOptions *options) { m_options=options; }

    //! In Y, the shown rows' height, min(the options' item count,
    //! MaxShownRows) rows, and its padding: the options list below them is
    //! cut off.
    void calculateSize(skin::Base *skin, Dim dim) override;

    //! A press on it (a row, the bar) leaves the keyboard focus with the box
    //! or its field, so a click picks with the field still focused.
    bool pressKeepsKeyboardFocus() override { return true; }

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
    SearchComboBoxOptions *m_options=nullptr;
};

//! The list of a SearchComboBox: a VirtualList of SearchComboBoxRows over the
//! box's shown options, SearchComboBox::MaxShownRows rows high whatever their
//! count (scrolling past them), so its pool is the same for any query: the
//! Menu holding it is as high as the shown rows and cuts it there. The Menu
//! paints the backdrop, so it draws nothing itself.
class GWENI_EXPORT SearchComboBoxOptions: public VirtualList
{
public:

    GWENI_CONTROL(SearchComboBoxOptions, VirtualList);

public:
    //! In Y, MaxShownRows rows.
    void calculateSize(skin::Base *skin, Dim dim) override;
};

//! A row of a SearchComboBox's list, pooled by its SearchComboBoxOptions and
//! bound to a shown option. Drawn as a MenuItem is: nothing while idle, the
//! MenuHover surface while it shows the box's highlighted option (which a
//! mouse move over it makes it). It takes no keyboard focus, so a press on it
//! leaves the focus where it was; a click picks its option (the list's
//! onItemPressed). Disabled, it is the "No matches" row: Button's disabled
//! text colour, no highlight, a press that does nothing.
class GWENI_EXPORT SearchComboBoxRow: public VirtualListRow
{
public:

    GWENI_CONTROL(SearchComboBoxRow, VirtualListRow);

public:
    void setComboBox(SearchComboBox *box) { m_box=box; }

    //! Whether it shows the box's highlighted option; a change redraws it.
    void setHighlighted(bool highlighted);
    bool isHighlighted() const { return m_highlighted; }
    //! Whether it shows a check, for an option a multi-select box has
    //! picked; a change redraws it.
    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }

    //! A real move of the mouse over it highlights its option; a hover that
    //! only follows the rows scrolling under a resting mouse does not, so
    //! the keyboard's highlight stays where the keys put it.
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;

    //! MenuItem's colours: Button's, its hover colour while highlighted.
    void updateColors() override;

private:
    SearchComboBox *m_box=nullptr;
    bool m_highlighted=false;
    bool m_checked=false;
};

//! A filterable SearchComboBox's text field: a TextBox over the box's text
//! area (the box places it, owner-placed, the box's text inset and the
//! chevron's column kept clear), drawn with no frame of its own (the
//! box's surface is behind it), its selection and caret as a TextBox's. It
//! shows the selected option's label until the user edits it; each edit is
//! the box's query. Up, Down, Return and Escape go to the box; a click on it
//! with the list closed opens the list and selects its text, so typing
//! replaces it; losing the keyboard focus closes the list and gives the
//! selection's label back.
class GWENI_EXPORT SearchComboBoxField: public TextBox
{
public:

    GWENI_CONTROL(SearchComboBoxField, TextBox);

public:
    void setComboBox(SearchComboBox *box) { m_box=box; }

    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyReturn(bool down) override;
    bool onKeyEscape(bool down) override;
    //! In an empty field of a multi-select box, it takes the last pick away.
    bool onKeyBackspace(bool down) override;
    //! With the list closed a press opens it with the text selected, and
    //! neither it nor its release places the caret (the TextBox's would
    //! cut the selection at the click).
    void onMouseClickLeft(int x, int y, bool down) override;
    void onLostKeyboardFocus() override;

    //! The text the box shows (no edit: no onTextChanged, the history
    //! cleared), the caret at its end.
    void showText(const String &text);

    //! One edit for the box however many text changes it makes (typing over
    //! a selection erases it, then inserts).
    void insertText(const String &str) override;

protected:
    //! Tells the box of the edit, once its insertText() is over.
    void onTextChanged() override;

private:
    SearchComboBox *m_box=nullptr;
    //! Inside insertText(), and whether the text changed there.
    bool m_inserting=false;
    bool m_changedInInsert=false;
    //! The press opened the list and selected the text: its release is ours.
    bool m_pressOpened=false;
    //! The box took the Return press: the release is the field's too, so it
    //! does not move the focus on as a TextBox's would.
    bool m_returnTaken=false;
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
//!
//! Filterable (setFilterable(true)), a SearchComboBoxField over the box's
//! text area takes the keyboard (made on first use, so a plain box has none;
//! the box's own text is then empty and the field shows the selection).
//! Typing opens the list and shows the options whose label contains the
//! field's text, case-insensitively (ASCII letters; the lower-cased labels
//! are kept with the options), all of them for an empty text, and
//! highlights the first; no option matching shows one disabled "No matches"
//! row. Each edit fires onTextChangedCaller. A keystroke matches over the
//! options' data, makes and deletes no control and binds again only the
//! rows whose option changed. The list closing without a pick (Escape, a
//! press outside it, the box toggling it) and the field losing the keyboard
//! focus give the field back the selection's label.
//!
//! Free text (setAllowCustomValue(true), which makes it filterable): a typed
//! text that is no option's label (case aside) shows an Add “<text>” row
//! after the matches (alone when nothing matches, in place of "No matches"),
//! highlighted when no option matches. Return on it, Return with the list
//! closed, or the field losing the focus, with such a text, fires
//! onCustomValue (the info's string the text). A handler may refuse it with
//! refuseCustomValue(): the field then keeps the text in the skin's
//! SearchComboRefusedText colour, until it is edited, Escape gives the value
//! back, or something is picked. Accepted, it is the value: getValue() is
//! the text, getSelectedIndex() NoOption, the field shows it, and the box
//! adds no option (the app may; a text that is an option's label picks that
//! option instead). The list closing leaves a free-text box's field as it
//! is, for Return or the lost focus to decide.
//!
//! Multi-select (setMultiSelect(true)): the box holds picks, each shown as a
//! removable Chip in pick order, then the field when filterable, wrapped onto
//! rows in the box's own arrange (ChipGap and RowGap apart, the field after
//! the last chip when MinFieldWidth fits beside it), the box growing to its
//! rows (its height flag Expand, so a request is the least it is). A pick of
//! an option not picked adds it, of a picked one takes it away; picked rows
//! show a check; the list stays open, and a filterable box's field is emptied
//! for the next query. The chip's x, and Backspace in an empty field (or on
//! a plain box), take a pick away. Each change fires onSelectionChange once;
//! getSelectedIndices()/getSelectedValues() read the picks and
//! setSelectedIndices() sets them, firing nothing. With free text a taken
//! text is a pick of its own, a chip like the others. Up and Down on the
//! closed box open the list. The open list follows the box's new bottom edge
//! in the frame a chip wraps or goes.
//!
//! The value is data, not an index: an app may replace the options at any
//! time (setOptions(), clearOptions() and addOption(), an async search's
//! results from onTextChangedCaller). The selection, and each multi-select
//! pick, stays on an option of the same label and name; while the options
//! hold none it is held, still the value (getValue(), the shown text, the
//! chip, getSelectedValues(), Escape and a lost focus showing it again) but
//! at no index (getSelectedIndex() NoOption, getSelectedIndices() leaving it
//! out), and is that option's again once one equal to it comes back. Only a
//! pick, the user's x or Backspace, or the app's setSelectedIndex() and
//! setSelectedIndices() change it. A box fitting its widest label changes
//! width with each result set: give a box whose options are search results a
//! width of its own.
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
    //! A picked row's check: its side, and its inset from the row's right
    //! edge, inside the row's right text padding (RowPaddingRight).
    static constexpr int CheckSize=10;
    static constexpr int CheckInset=3;
    static constexpr int RowPaddingRight=16;
    //! No option selected or highlighted.
    static constexpr int NoOption=-1;

    //! Appends an option. A held selection or pick equal to it is its; the
    //! first one added to a box with no value (no selection, held or not, and
    //! no free-text value) is selected, with no event (as
    //! ComboBox::addItem), unless the field holds what the user typed, which
    //! it keeps, as setOptions() does.
    void addOption(const String &label, const String &name="");
    //! Replaces the options. The selection stays on an equal option (the
    //! same label and name) among them, else it is held (the class); with
    //! nothing selected the first option is, unless the field holds what the
    //! user typed; a free-text value stays the value. Multi-select, each pick
    //! of an option stays on an equal one, else it is held, its chip
    //! staying. No event fires. While the field holds what
    //! the user typed (an app refilling the options from
    //! onTextChangedCaller), it keeps it and the open list shows the new
    //! options matching it; the value shows again when the edit ends.
    void setOptions(std::vector<Option> options);
    //! No options; the value is held, as setOptions({}) holds it
    //! (setSelectedIndex(NoOption) or setSelectedIndices({}) drop it).
    void clearOptions();

    size_t getOptionCount() const { return m_options.size(); }
    const Option &getOption(size_t index) const { return m_options[index]; }

    //! The selected option, or NoOption (nothing, a free-text value, or a
    //! held selection).
    int getSelectedIndex() const { return m_selected; }
    //! Whether the selection is held: an option no option equals now.
    bool isSelectionHeld() const { return m_held; }
    //! Selects option \p index (NoOption, or one out of range, for none),
    //! firing nothing; a free-text or held value is dropped.
    void setSelectedIndex(int index);
    //! The selected option's name (held or not), or its label when it has
    //! none; a free-text value's text; empty with nothing selected.
    String getValue() override;

    //! Fired once per pick or Up/Down step that changes the selection. The
    //! box goes on after its events: a handler deletes it with
    //! delayedDelete(), as any control.
    gweni::event::Caller onSelection;
    //! Fired once per edit of the field (the info's string is its text).
    gweni::event::Caller onTextChangedCaller;
    //! Fired when a typed text that is no option's label is to become the
    //! value (the info's string); refuseCustomValue() from the handler keeps
    //! the value as it was.
    gweni::event::Caller onCustomValue;

    //! Free text (above); on, it makes the box filterable, which stays on
    //! while free text is allowed.
    void setAllowCustomValue(bool allow);
    bool isCustomValueAllowed() const { return m_allowCustom; }
    //! From an onCustomValue handler: the text is not taken.
    void refuseCustomValue() { m_refusing=true; }
    //! Whether the value is a free-text one (getValue() its text).
    bool hasCustomValue() const { return m_hasCustom; }
    //! Whether the field shows a text a handler refused.
    bool isCustomValueRefused() const { return m_refused; }
    //! The Add row's text for \p text: Add “text”.
    static String addRowText(const String &text);

    //! A text field over the box filters the list (above). Off again, the
    //! field is hidden and the box shows the selection itself.
    void setFilterable(bool filterable);
    bool isFilterable() const { return m_filterable; }
    //! The field, or null before setFilterable(true) first made it.
    internal::SearchComboBoxField *getField() const { return m_field; }
    //! What the shown options are matched against: the field's text since
    //! its last edit, empty while the list is closed or was opened unedited.
    const String &getQuery() const { return m_query; }
    //! The text of the disabled row shown when no option matches.
    static const char *noMatchesText() { return "No matches"; }

    //! Multi-select (above). Either way the picks or the selection start
    //! empty, firing nothing; on, the height flag becomes Expand, and off
    //! it is the one it was before.
    void setMultiSelect(bool multi);
    bool isMultiSelect() const { return m_multi; }
    //! The picked options, in pick order (free-text and held picks left
    //! out).
    std::vector<int> getSelectedIndices() const;
    //! Every pick's value, in pick order: an option's name (its label without
    //! one), a held one's included, or a free-text pick's text.
    std::vector<String> getSelectedValues() const;
    //! The picks become these options, in this order (out of range and
    //! repeated ones left out, free-text and held picks dropped), firing
    //! nothing.
    void setSelectedIndices(const std::vector<int> &indices);
    //! Whether option \p index is picked.
    bool isPicked(int index) const;
    //! The chips, in pick order.
    std::vector<Chip *> getChips() const;
    //! Fired once per change of the picks (the info's integer the option
    //! picked or taken away, NoOption for a free-text or held one).
    gweni::event::Caller onSelectionChange;

    //! Units between two chips on a row, and between two rows.
    static constexpr int ChipGap=4;
    static constexpr int RowGap=2;
    //! The least room the field takes beside the last chip; less, and it
    //! goes to a row of its own.
    static constexpr int MinFieldWidth=40;
    //! Where the chips and the field go at the box's \p width: each chip's
    //! rect, then the field's (empty with no field), in the box's units; and
    //! the box's height for them.
    void flowRects(int width, std::vector<Rect> &chips, Rect &field, int &height);

    void openList();
    void closeList();
    bool isListOpen() const;
    //! The list, made with the box (null once deleted from outside).
    internal::SearchComboBoxList *getList() const { return m_list; }
    internal::SearchComboBoxOptions *getOptionsList() const { return m_rows; }

    //! The options the open list shows, in order (indices into the options):
    //! those the query matches, every option for an empty one. The list may
    //! show one row after them that is no option: the Add row
    //! (isAddRowShown()), or with no match and no free text "No matches".
    const std::vector<size_t> &getShownOptions() const { return m_matches; }
    bool isAddRowShown() const { return m_addRow; }
    //! Whether the Add row is the highlighted one.
    bool isAddRowHighlighted() const { return m_addRow && (m_highlightPos == m_matches.size()); }
    //! The open list's rows: the shown options and the row after them.
    size_t shownRowCount() const;
    //! The option the keys move through the open list, or NoOption.
    int getHighlightedIndex() const { return m_highlight; }
    //! Highlights option \p index (shown in the list), scrolling it into
    //! view; NoOption takes the highlight away.
    void setHighlightedIndex(int index);
    //! How many times the list's binder has filled a row, since the box was
    //! made: what a keystroke binds again, for tests and tools.
    size_t getRowBindCount() const { return m_rowBinds; }

    void onPress(event::Info info) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyReturn(bool down) override;
    bool onKeyEscape(bool down) override;
    //! A plain multi-select box: the last pick away.
    bool onKeyBackspace(bool down) override;
    //! A filterable box gives the focus to its field.
    void onKeyboardFocus() override;
    //! Filterable, a press on the box (its chevron's column) leaves the
    //! focus where it is: its release focuses the field (onPress).
    bool pressKeepsKeyboardFocus() override { return m_filterable; }
    //! The field and the chips are disabled with the box, and disabling
    //! closes the list. A disabled box opens no list and picks nothing (Up,
    //! Down, Return, Backspace and its rows do nothing), and its field losing
    //! the focus gives back the value's text rather than taking a free-text
    //! value.
    void setDisabled(bool disabled) override;
    using ParentClass::setFont;
    //! The field takes the box's font too.
    void setFont(const gweni::Font &font) override;
    //! The field over the text area (multi-select: the chips and the field
    //! where flowRects() puts them), along each axis before the children.
    void arrange(skin::Base *skin, Dim dim) override;

    //! In X, the widest option's label in this box's font as well as the
    //! shown text and a free-text value, with the text padding and the
    //! chevron column, as a ComboBox's: the labels are measured again only when they or the font
    //! (the renderer's text layout) changed. Multi-select, in Y the height
    //! flowRects() gives at the box's width, final by then.
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
    friend class internal::SearchComboBoxField;

    //! The list is being deleted: it is no longer held.
    void listDeleted();
    //! The list was shown or hidden, by the box or from outside (a press
    //! outside it, Escape over a Popup): the box's open look follows it.
    void listVisibilityChanged(event::Info info);
    void makeList();
    //! Makes \p index the selection and shows its label (NoOption: none,
    //! empty), firing onSelection with \p doEvents when it changed.
    void select(int index, bool doEvents);
    //! The selection's label on the box, or in the field when filterable.
    void showSelection();
    //! Whether the field holds text the user typed: an edit of it made it,
    //! and showing the value, the value's own text resolved or emptying the
    //! field after a multi-select pick ends it. An app refilling the options then leaves it as it is.
    bool fieldHoldsEdit() const;
    //! The new options' index of an option equal to \p option (its label
    //! and name), or NoOption.
    static int findOption(const std::vector<Option> &options, const Option &option);
    //! The field's text was edited: the new query, the list opened or
    //! filtered.
    void fieldTextChanged();
    //! Whether the click opened the list (closed and enabled).
    bool fieldClicked();
    void fieldLostFocus();
    //! The keyboard focus to the field when filterable, else to the box.
    void focusInput();
    //! Shows the list for the current query; the highlight the selection
    //! (opened by a press) or the first shown option (by an edit).
    void showList(bool edited);
    //! The query changed while open: the shown options again, the first
    //! highlighted and scrolled to.
    void filter();
    //! The shown options (m_matches) for the query, and whether the "No
    //! matches" row shows in their place.
    void matchOptions();
    //! The list told the shown rows' count, and the pooled rows that showed
    //! another row before (\p previous, each position's rowKey()) bound
    //! again, and the Add row; every other row only its highlight.
    void rebindChanged(const std::vector<size_t> &previous);
    //! The rows the keys and the mouse highlight: the shown options and the
    //! Add row.
    size_t highlightableCount() const;
    //! What the row at \p position shows: an option's index, or a key for
    //! the Add row or the "No matches" row.
    size_t rowKey(size_t position) const;
    void rowKeys(std::vector<size_t> &keys) const;
    //! Where the highlight starts for a query: the first row that takes it.
    size_t firstHighlight() const;
    //! Picks what the row at \p position shows.
    void pickPosition(size_t position);
    //! The value's text: the free-text value, or the selection's label.
    String shownValueText() const;
    //! The selected option, held or not, or null.
    const Option *selectedOption() const;
    //! The field's refused colour on or off.
    void setRefused(bool refused);
    //! The option whose label is \p text, case aside, or NoOption.
    int optionWithLabel(const String &text) const;
    //! A free-text box takes the field's text as the value (see the class).
    void resolveText();
    //! onCustomValue for \p text; whether it was taken.
    bool commitCustom(const String &text);
    //! Highlights the shown option at \p position (VirtualList::NoItem for
    //! none), scrolling it into view with \p scroll.
    void highlightPosition(size_t position, bool scroll);
    //! Picks option \p index from the list: selects it and closes the list.
    void pick(int index, bool doEvents);
    //! Multi-select: option \p index picked if it was not, taken away if it
    //! was (onSelectionChange once); the list stays open.
    void togglePick(int index);
    //! A pick added: option \p index, or free text \p text with NoOption.
    void addPick(int index, const String &text);
    //! Pick \p at taken away, its chip hidden and deleted after the frame.
    void removePick(size_t at, bool doEvents);
    //! A chip's x was clicked.
    void chipRemoved(event::Info info);
    //! The picks dropped, firing nothing; free-text ones kept with \p keepCustom.
    void clearPicks(bool keepCustom);
    //! After a multi-select pick: the field emptied for the next query, the
    //! list showing every option again, the highlight on \p position's option.
    void afterMultiPick(int option);
    //! The row showing option \p index checked or not.
    void refreshCheck(int index);
    //! onSelectionChange for option \p index (NoOption: a free-text pick).
    void fireSelectionChange(int index);
    void rowPressed(event::Info info);
    void bindRow(Base *row, size_t item);
    //! Moves the highlight by \p step through the shown options, wrapping.
    void stepHighlight(int step);
    //! A row was moved over: its option becomes the highlight, not scrolled
    //! to (it is on screen).
    void rowHovered(internal::SearchComboBoxRow *row);
    //! The options changed: the lower-cased labels, and while the list is
    //! open the shown options again with every row bound again.
    void optionsChanged();
    //! The row height of the list in this box's list font.
    int rowHeight();
    int optionsTextWidth();

    std::vector<Option> m_options;
    //! Each option's label lower-cased, the query matched against them.
    std::vector<String> m_lowerLabels;
    std::vector<size_t> m_matches;
    //! The query matched nothing: the list shows the "No matches" row.
    bool m_noMatch=false;
    //! The list shows the Add row after the matches.
    bool m_addRow=false;
    bool m_allowCustom=false;
    //! The value is the free-text m_custom (m_selected NoOption).
    bool m_hasCustom=false;
    String m_custom;
    bool m_refused=false;
    //! fieldHoldsEdit()
    bool m_fieldEdited=false;
    //! Set by refuseCustomValue() during onCustomValue.
    bool m_refusing=false;
    int m_selected=NoOption;
    //! The selection is m_heldOption, which no option equals now
    //! (m_selected NoOption).
    bool m_held=false;
    Option m_heldOption;
    //! The highlighted option and its place in m_matches (NoItem for none).
    int m_highlight=NoOption;
    size_t m_highlightPos=VirtualList::NoItem;

    bool m_filterable=false;
    internal::SearchComboBoxField *m_field=nullptr;
    String m_query;
    String m_lowerQuery;
    size_t m_rowBinds=0;

    //! Multi-select: the picks in order, each its option (NoOption for free
    //! text, or held), whether it is free text, its option's label and name
    //! (free text: the text as its label) and its chip; m_picked whether each
    //! option is.
    struct Pick
    {
        int option=NoOption;
        bool custom=false;
        Option value;
        Chip *chip=nullptr;
    };
    bool m_multi=false;
    //! The height flag setMultiSelect(true) replaced, given back by false.
    SizeFlag m_singleHeightFlag=SizeFlag::Fixed;
    std::vector<Pick> m_picks;
    std::vector<char> m_picked;

    internal::SearchComboBoxList *m_list=nullptr;
    internal::SearchComboBoxOptions *m_rows=nullptr;

    //! What optionsTextWidth() measured: each option's width, valid while
    //! m_widthsValid and the font, renderer and text layout are these.
    std::vector<int> m_optionWidths;
    //! The largest of m_optionWidths.
    int m_widestWidth=0;
    bool m_widthsValid=false;
    gweni::Font m_widthsFont;
    const renderer::Base *m_widthsRenderer=nullptr;
    unsigned m_widthsSerial=0;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_searchcombobox_h_
