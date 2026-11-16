/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_filteredlist_h_
#define _gweni_controls_filteredlist_h_

#include <gweni/controls/virtualList.h>

#include <functional>
#include <vector>

namespace gweni
{
namespace controls
{

//! The list part a SearchComboBox and a CommandPalette share: not a control,
//! but the data and the bookkeeping behind a VirtualList of pooled rows that
//! shows the options a query matches.
//!
//! - The options as the matcher sees them: one lower-cased text each
//!   (lowerAscii(), so a match is case-insensitive over ASCII letters and
//!   compares every other byte, a UTF-8 sequence's included, as it is). The
//!   owner keeps what else an option is (a label and a name, a command).
//! - The query, kept as typed and lower-cased.
//! - The matches: indices into the options, in the order the list shows them,
//!   which the matcher (setMatcher(); substringMatcher() by default) fills
//!   from the texts and the lower-cased query on match().
//! - An optional trailing row after the matches that is no option (a
//!   SearchComboBox's Add row or "No matches" row), with a key of the owner's
//!   and whether it takes the highlight.
//! - The highlight: a position among the rows (NoItem for none) and the
//!   option it shows, moved by the keys (step(), page()) and the mouse, each
//!   move redrawing only the two rows it leaves and reaches through the
//!   owner's row highlighter, and scrolled into view when asked.
//! - rebindChanged(): after the matches changed, the list told the new row
//!   count and only the pooled rows that now show another key bound again
//!   (the binder is the owner's), the others only told their highlight.
class GWENI_EXPORT FilteredList
{
public:
    static constexpr size_t NoItem=VirtualList::NoItem;
    //! No option highlighted.
    static constexpr int NoOption=-1;

    //! Fills \p matches with the options the list's lower-cased query
    //! matches, in the order they are shown (an empty query: what the owner
    //! lists then).
    using Matcher=std::function<void(const FilteredList &list, std::vector<size_t> &matches)>;
    //! Shows \p row highlighted or not (a change redraws it).
    using RowHighlighter=std::function<void(Base *row, bool highlighted)>;

    FilteredList();

    //! ASCII letters lower-cased, every other byte (a UTF-8 sequence's
    //! included) kept: a substring of the lower-cased text is a substring of
    //! the text.
    static String lowerAscii(const String &text);
    //! Every option whose lower-cased text contains the lower-cased query, in
    //! option order; every option for an empty query.
    static Matcher substringMatcher();

    void setMatcher(Matcher matcher);

    //! The options' texts, lower-cased as they are added.
    void addText(const String &text);
    void clearTexts();
    size_t textCount() const { return m_lowerTexts.size(); }
    const String &lowerText(size_t option) const { return m_lowerTexts[option]; }
    const std::vector<String> &lowerTexts() const { return m_lowerTexts; }

    //! The query (and its lower-cased form); match() matches it.
    void setQuery(const String &query);
    void clearQuery();
    const String &query() const { return m_query; }
    const String &lowerQuery() const { return m_lowerQuery; }

    //! The matches for the query, through the matcher. The highlight is the
    //! caller's to set again (the positions are now others).
    void match();
    //! No matches and no trailing row (nothing shown).
    void clearMatches();
    const std::vector<size_t> &matches() const { return m_matches; }

    //! A row after the matches showing \p key (no option index), taking the
    //! highlight with \p highlightable, bound again by rebindChanged() even
    //! when its key stays with \p alwaysRebind (a row naming the query).
    void setTrailingRow(size_t key, bool highlightable, bool alwaysRebind);
    void clearTrailingRow();
    bool hasTrailingRow() const { return m_trailing; }
    bool isTrailingHighlightable() const { return m_trailing && m_trailingHighlightable; }

    //! The rows shown: the matches and the trailing row.
    size_t shownRowCount() const { return m_matches.size()+(m_trailing?1:0); }
    //! The rows that take the highlight: the matches, and the trailing row
    //! when highlightable.
    size_t highlightableCount() const { return m_matches.size()+(isTrailingHighlightable()?1:0); }
    //! What the row at \p position shows: an option's index, or the trailing
    //! row's key.
    size_t rowKey(size_t position) const;
    void rowKeys(std::vector<size_t> &keys) const;
    //! The position of option \p option among the matches, or NoItem;
    //! O(matches).
    size_t positionOf(size_t option) const;

    //! The VirtualList the rows are pooled in (null while there is none) and
    //! how a row shows the highlight.
    void setRows(VirtualList *rows, RowHighlighter highlighter);
    VirtualList *rows() const { return m_rows; }

    //! The list told the shown row count; the pooled rows whose key differs
    //! from \p previous (each position's rowKey() before the matches
    //! changed) bound again, and the trailing row with alwaysRebind; every
    //! other row only its highlight.
    void rebindChanged(const std::vector<size_t> &previous);

    //! The highlighted row's position (NoItem for none) and the option it
    //! shows (NoOption for none or the trailing row).
    size_t highlightPosition() const { return m_highlightPos; }
    int highlightedOption() const { return m_highlight; }
    //! The highlight set to \p position with no row told (the rows are about
    //! to be bound again).
    void setHighlightState(size_t position);
    //! Highlights the row at \p position (NoItem, or one that takes no
    //! highlight: none), the row it leaves and the one it reaches told,
    //! scrolling it into view with \p scroll.
    void highlight(size_t position, bool scroll);
    //! Moves the highlight by \p step rows, wrapping; with none, the first
    //! row for a step down and the last for one up.
    void step(int step);
    //! Moves the highlight by \p pages pages of \p pageRows rows, held at the
    //! first and last rows (no wrap); with none, the first or last row.
    void page(int pages, size_t pageRows);
    //! The pooled row at \p row was moved over: its row highlighted, not
    //! scrolled to (it is on screen).
    void rowHovered(const Base *row);

    //! Counts one bind of a row by the owner's binder.
    void countBind() { ++m_binds; }
    size_t bindCount() const { return m_binds; }

private:
    Matcher m_matcher;
    std::vector<String> m_lowerTexts;
    String m_query;
    String m_lowerQuery;
    std::vector<size_t> m_matches;

    bool m_trailing=false;
    bool m_trailingHighlightable=false;
    bool m_trailingRebind=false;
    size_t m_trailingKey=NoItem;

    VirtualList *m_rows=nullptr;
    RowHighlighter m_highlighter;

    int m_highlight=NoOption;
    size_t m_highlightPos=NoItem;
    size_t m_binds=0;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_filteredlist_h_
