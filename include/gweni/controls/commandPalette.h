/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_commandpalette_h_
#define _gweni_controls_commandpalette_h_

#include <gweni/controls/filteredList.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/searchComboBox.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/virtualList.h>

#include <functional>
#include <unordered_map>
#include <vector>

namespace gweni
{
namespace controls
{

class Canvas;
class CommandPalette;
class Label;

namespace internal
{

//! A CommandPalette's popup: its field docked Top and its list under it,
//! CommandPalette::Width wide (less on a canvas narrower than it and its
//! insets) and as high as the field and min(shown rows, MaxShownRows) rows:
//! the list in it is MaxShownRows rows high whatever is shown, so its pool
//! never changes, and the popup cuts it at the shown rows. Drawn by Popup's
//! renderer.
class GWENI_EXPORT CommandPalettePopup: public Popup
{
public:

    GWENI_CONTROL(CommandPalettePopup, Popup);

public:
    //! Tells its palette: a canvas tearing down can delete it first.
    ~CommandPalettePopup() override;

    void setPalette(CommandPalette *palette) { m_palette=palette; }

    //! In X the palette's width at the canvas's; in Y cut to the shown rows.
    void calculateSize(skin::Base *skin, Dim dim) override;

private:
    CommandPalette *m_palette=nullptr;
};

//! A CommandPalette's list: a VirtualList of CommandPaletteRows over the
//! shown commands, MaxShownRows rows high whatever their count (its pool the
//! same for any query). The popup paints the backdrop, so it draws nothing.
class GWENI_EXPORT CommandPaletteList: public VirtualList
{
public:

    GWENI_CONTROL(CommandPaletteList, VirtualList);

public:
    //! In Y, MaxShownRows rows.
    void calculateSize(skin::Base *skin, Dim dim) override;
};

//! A row of a CommandPalette's list, pooled and bound to a shown command:
//! SearchComboBox's row (its look, its highlight, no keyboard focus), its
//! own text empty, with three Labels docked in it: the category ("View:", in
//! the skin's CommandPaletteCategoryText) on the left, the shortcut (in
//! CommandPaletteShortcutText) on the right and the label filling the rest,
//! in the row's text colour (the highlight's). Each is set only when its
//! string changes. Disabled, it is the "No matching commands" row.
class GWENI_EXPORT CommandPaletteRow: public SearchComboBoxRow
{
public:

    GWENI_CONTROL(CommandPaletteRow, SearchComboBoxRow);

public:
    void setPalette(CommandPalette *palette) { m_palette=palette; }

    //! What it shows: its parts' strings, each set only when it changes.
    void show(const String &category, const String &label, const String &shortcut);

    const String &getCategoryText() const;
    const String &getLabelText() const;
    const String &getShortcutText() const;
    Label *getCategoryLabel() const { return m_category; }
    Label *getLabelLabel() const { return m_label; }
    Label *getShortcutLabel() const { return m_shortcut; }

    //! A real move over it highlights its command.
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    //! The label follows the row's colour (SearchComboBoxRow's: Button's,
    //! its hover colour while highlighted).
    void updateColors() override;

private:
    CommandPalette *m_palette=nullptr;
    Label *m_category=nullptr;
    Label *m_label=nullptr;
    Label *m_shortcut=nullptr;
};

//! A CommandPalette's field: a TextBox whose text is the query. Up, Down,
//! PageUp, PageDown and Return go to the palette; Escape is the canvas's
//! (it closes the popup); Home, End and the rest stay the TextBox's.
class GWENI_EXPORT CommandPaletteField: public TextBox
{
public:

    GWENI_CONTROL(CommandPaletteField, TextBox);

public:
    void setPalette(CommandPalette *palette) { m_palette=palette; }

    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyPageUp(bool down) override;
    bool onKeyPageDown(bool down) override;
    bool onKeyReturn(bool down) override;

    //! Empties it with no edit (no query, no onTextChanged, the history
    //! cleared).
    void clearText();

protected:
    void onTextChanged() override;

private:
    CommandPalette *m_palette=nullptr;
    bool m_clearing=false;
    //! The palette took the Return press: the release is the field's too.
    bool m_returnTaken=false;
};

}//namespace internal

//! A command palette (VS Code's Ctrl+Shift+P): a Popup at the canvas's top
//! centre (PopupPlacement::CanvasTop) holding a text field with the keyboard
//! focus and, under it, the app's commands that match what is typed, best
//! first; Return runs the highlighted one.
//!
//! Commands are data, not controls: addCommand() keeps an id, a label, a
//! category, the shortcut text shown (the palette binds no key) and whether
//! it is enabled (a flag, or a predicate asked at each match). onCommand fires
//! with the id in the info's string.
//!
//! The palette itself is a control that draws nothing and is hidden, put
//! anywhere on the canvas (attach() puts one with no parent on the canvas).
//! attach(canvas, "Ctrl+Shift+P") registers the accelerator on the canvas
//! (Base::addAccelerator, so input::handleAccelerator finds it whatever has
//! the focus, but under a modal); a press opens the palette and a second
//! one closes it. open(), close() and toggle() do the same from code.
//!
//! Opening makes the popup and its controls the first time only, empties the
//! field and lists every enabled command. It closes on Escape, a press
//! outside it, a command run and the accelerator, and gives the keyboard
//! focus back to the control that had it at the open (if it is still
//! shown); a press outside on another control leaves that control the focus.
//!
//! The list is a FilteredList over a VirtualList of pooled rows (the shared
//! part of SearchComboBox), its matcher text::fuzzyRank over "category:
//! label" (the label alone with no category): the query's characters in
//! order, case-insensitively, scored by text::fuzzyMatch. Equal scores go to
//! the recently run commands first (with setRecentFirst(true), the default:
//! the last getRecentLimit() run, the latest first) and then by the order
//! they were added. An empty query lists the recent commands first, then the
//! rest by category and label (then the order added). Disabled commands are
//! not listed; with nothing to list one disabled "No matching commands" row
//! shows.
//!
//! Keys (VS Code's): Up and Down move the highlight, wrapping; PageUp and
//! PageDown a page (MaxShownRows rows), stopping at the ends; Return runs the
//! highlighted command (closing first, then onCommand once); Escape closes;
//! Home, End and editing are the field's. A click on a row runs its command,
//! and a mouse move over one highlights it.
//!
//! Cost: a keystroke ranks the command data again and binds only the pooled
//! rows that show another command; it makes and deletes no control and lays
//! out nothing outside the popup, so it costs the same with 50 commands as
//! with 5 000.
class GWENI_EXPORT CommandPalette: public Base
{
public:

    GWENI_CONTROL(CommandPalette, Base);
    GWENI_DRAWS_NOTHING

public:
    //! One command.
    struct Command
    {
        //! What onCommand gives back; unique among the commands.
        String id;
        String label;
        //! Shown before the label as "category:", empty for none.
        String category;
        //! The shortcut shown right-aligned ("Ctrl+S"); only shown.
        String shortcut;
        //! Listed only while enabled: the flag, and the predicate too when
        //! one is set (asked at each match).
        bool enabled=true;
        std::function<bool()> isEnabled;
    };

    //! Its width, less on a canvas narrower than it and 2*CanvasInset.
    static constexpr int Width=600;
    static constexpr int CanvasInset=16;
    //! The most rows the list shows; past them it scrolls.
    static constexpr size_t MaxShownRows=10;
    //! Units between the field and the list.
    static constexpr int ListGap=4;
    //! Units between a row's category and its label.
    static constexpr int CategoryGap=4;
    //! No command highlighted or found.
    static constexpr int NoCommand=-1;

    //! Takes its accelerator off the canvas.
    ~CommandPalette() override;

    //! Appends a command; one whose id a command has replaces it in place.
    void addCommand(const Command &command);
    //! Takes the command with \p id away; false when there is none.
    bool removeCommand(const String &id);
    void clearCommands();
    size_t getCommandCount() const { return m_commands.size(); }
    const Command &getCommand(size_t index) const { return m_commands[index]; }
    //! The index of the command with \p id, or NoCommand.
    int findCommand(const String &id) const;
    //! Whether the command at \p index is listed now (enabled).
    bool isCommandEnabled(size_t index) const;

    //! Registers \p accelerator on \p canvas to toggle the palette (one given
    //! before is taken off); the palette goes on \p canvas if it has no
    //! parent.
    void attach(Canvas *canvas, const String &accelerator="Ctrl+Shift+P");
    //! Takes the accelerator off its canvas.
    void detach();
    const String &getAccelerator() const { return m_accelerator; }

    void open();
    void close();
    void toggle();
    bool isOpen() const;

    //! Fired once per command run, the id in the info's string, after the
    //! palette has closed (the focus back where it was).
    gweni::event::Caller onCommand;

    //! Recently run commands first (above); on by default.
    void setRecentFirst(bool recentFirst);
    bool isRecentFirst() const { return m_recentFirst; }
    //! How many run commands are kept as recent (10 by default).
    void setRecentLimit(size_t limit);
    size_t getRecentLimit() const { return m_recentLimit; }
    //! The recent commands' ids, the latest first.
    const std::vector<String> &getRecent() const { return m_recent; }
    void clearRecent();

    //! Runs the command with \p id as Return would (recent, onCommand),
    //! closing the palette if it is open; false when it is not listed.
    bool runCommand(const String &id);

    //! The popup, the field and the list, or null before the first open.
    internal::CommandPalettePopup *getPopup() const { return m_popup; }
    internal::CommandPaletteField *getField() const { return m_field; }
    internal::CommandPaletteList *getList() const { return m_list; }
    //! The query (the field's text since its last edit).
    const String &getQuery() const { return m_filter.query(); }
    //! The shown commands, in order (indices into the commands).
    const std::vector<size_t> &getShownCommands() const { return m_filter.matches(); }
    //! The rows shown: the commands, or the one "No matching commands" row.
    size_t shownRowCount() const { return m_filter.shownRowCount(); }
    //! The highlighted command, or NoCommand.
    int getHighlightedIndex() const { return m_filter.highlightedOption(); }
    //! Its row's position, or VirtualList::NoItem.
    size_t getHighlightedPosition() const { return m_filter.highlightPosition(); }
    //! How many times a row was bound: what a keystroke binds again.
    size_t getRowBindCount() const { return m_filter.bindCount(); }
    static const char *noMatchesText() { return "No matching commands"; }
    static const char *placeholderText() { return "Type a command"; }
    //! "category: label", or the label with no category: what is matched.
    static String matchText(const Command &command);

protected:
    friend class internal::CommandPalettePopup;
    friend class internal::CommandPaletteRow;
    friend class internal::CommandPaletteField;

    //! The canvas's accelerator.
    void acceleratorPressed(event::Info info);
    void makePopup();
    //! The popup is being deleted: it and its controls are no longer held.
    void popupDeleted();
    void popupClosed(event::Info info);
    //! The commands changed: the cached texts and orders, and an open list
    //! shown again.
    void commandsChanged();
    //! The match texts and the empty query's order, made again when stale.
    void prepare() const;
    //! The matcher (above).
    void matchCommands(std::vector<size_t> &matches) const;
    //! The recency rank of every command: its place in m_recent, or past it.
    void recentRanks(std::vector<int> &ranks) const;
    //! The shown commands for the query again, the first highlighted,
    //! only the rows showing another command bound again.
    void filter();
    void fieldTextChanged();
    void bindRow(Base *row, size_t item);
    void rowPressed(event::Info info);
    void rowHovered(internal::CommandPaletteRow *row);
    void stepHighlight(int step);
    void pageHighlight(int pages);
    //! Runs the highlighted command; false with none.
    bool runHighlighted();
    void run(size_t index);
    int rowHeight();
    //! The popup's width at a canvas \p canvasWidth wide.
    static int widthFor(int canvasWidth);

    std::vector<Command> m_commands;
    //! Each command's matchText(), and the empty query's order (by category,
    //! label, then index), valid while !m_stale.
    mutable std::vector<String> m_texts;
    mutable std::vector<size_t> m_sortedOrder;
    std::unordered_map<String, size_t> m_byId;
    mutable bool m_stale=true;

    FilteredList m_filter;

    bool m_recentFirst=true;
    size_t m_recentLimit=10;
    std::vector<String> m_recent;

    Canvas *m_attachedCanvas=nullptr;
    //! The canvas's control id, to tell it is still there.
    size_t m_attachedCanvasId=0;
    String m_accelerator;
    //! The canvas's caller attach() added, which detach() takes away.
    event::Caller *m_acceleratorCaller=nullptr;

    internal::CommandPalettePopup *m_popup=nullptr;
    internal::CommandPaletteField *m_field=nullptr;
    internal::CommandPaletteList *m_list=nullptr;
    //! The control that had the keyboard focus at the open, by id.
    size_t m_returnFocus=0;
    bool m_hasReturnFocus=false;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_commandpalette_h_
