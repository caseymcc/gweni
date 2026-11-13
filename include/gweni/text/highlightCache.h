/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_text_highlightCache_h_
#define _gweni_text_highlightCache_h_

#include <gweni/export.h>
#include <gweni/text/document.h>
#include <gweni/text/tokenizer.h>

#include <cstddef>
#include <functional>
#include <vector>

namespace gweni
{
namespace text
{

//! The tokens of every line of a Document, kept as it is edited, so an edit
//! tokenizes the lines it changed and not the text.
//!
//! Each line keeps its tokens, the state it was tokenized from and its end
//! state. A line is fresh when its text has not changed since and the state it
//! was tokenized from is the end state of the fresh line before it (the
//! tokenizer's initial state for line 0). Lines [0, frontier()) are known to be
//! fresh; a line past the frontier is checked when it is asked for, the lines
//! between walked in order: one whose text and start state are what it was
//! tokenized from is fresh as it is, at the cost of one comparison, and any
//! other is tokenized again.
//!
//! So after an edit:
//! - the edited lines are tokenized again; the lines after them are compared,
//!   not tokenized, and an edit that leaves its last line's end state as it was
//!   tokenizes those lines alone;
//! - an edit that changes the end state (an opened block string, fence or
//!   comment) tokenizes forward through the lines on screen (setVisibleLines())
//!   and no further: the lines below are left as they were, stale, and done when
//!   they are asked for (tokens()), when bound to a row or scrolled to;
//! - lines inserted or removed shift the entries after them, which are not
//!   tokenized again (their text did not change, and they stay fresh while the
//!   line before them ends in the state it did).
class GWENI_EXPORT HighlightCache
{
public:
    //! Called after an edit with the lines, past the edited ones or not, whose
    //! tokens that edit's pass through the lines on screen changed, in
    //! ascending order (for the rows showing them to be drawn again). Not
    //! called for what tokens() does when asked.
    typedef std::function<void(const std::vector<size_t> &lines)> ChangeHandler;

    HighlightCache();
    ~HighlightCache();

    HighlightCache(const HighlightCache &)=delete;
    HighlightCache &operator=(const HighlightCache &)=delete;

    //! Follows \p document's edits through an observer of its own, taking every
    //! line as stale; nullptr detaches. A control that observes the document too
    //! and reads the tokens from its observer can instead call linesChanged()
    //! itself, first, and attach nothing.
    void attach(Document *document);
    //! Takes \p document's lines as stale without observing it (for a caller that
    //! forwards its edits through linesChanged()).
    void setDocument(const Document *document);
    const Document *document() const { return m_document; }

    //! The tokenizer, every line stale after a new one. nullptr (the default)
    //! highlights nothing: tokens() is empty for every line, and the cache
    //! keeps no entry per line.
    void setTokenizer(const Tokenizer *tokenizer);
    const Tokenizer *tokenizer() const { return m_tokenizer; }

    //! The lines on screen, [first, first+count), which an edit's pass keeps fresh.
    //! An edit above them moves them with their lines, as a view anchored on
    //! them moves, until the next call.
    void setVisibleLines(size_t first, size_t count);
    size_t visibleFirst() const { return m_visibleFirst; }
    size_t visibleCount() const { return m_visibleCount; }

    //! The lines \p change replaced (Document's LineChange, after the edit):
    //! shifts the entries, marks the new lines stale and, when the edit is at or
    //! above the last line on screen, makes every line through that one fresh.
    void linesChanged(const LineChange &change);

    //! Line \p line's tokens, made fresh first (with every stale line before
    //! it); empty for a line past the end or without a tokenizer.
    const std::vector<Token> &tokens(size_t line);
    //! The state line \p line ends in, made fresh first as tokens() does.
    TokenState endState(size_t line);

    //! The lines it keeps an entry for: the document's, 0 without a tokenizer.
    size_t entryCount() const { return m_entries.size(); }
    //! Lines known fresh without a check: [0, frontier()).
    size_t frontier() const { return m_frontier; }
    //! Whether line \p line is fresh now, without making it so: a line past the
    //! frontier is fresh if every line from the frontier to it is.
    bool isFresh(size_t line) const;

    //! Lines tokenized since this cache was made, every tokenize() call one.
    size_t tokenizedLines() const { return m_tokenizedLines; }

    void setChangeHandler(ChangeHandler handler) { m_changeHandler=std::move(handler); }

private:
    struct Entry
    {
        std::vector<Token> tokens;
        TokenState start;
        TokenState end;
        //! the text changed (or was never tokenized) since its tokens were made
        bool stale=true;
    };

    void reset();
    //! Makes lines [m_frontier, last] fresh, recording in m_changed the ones
    //! whose tokens changed when \p record is set.
    void freshen(size_t last, bool record);

    const Document *m_document=nullptr;
    Document *m_observed=nullptr;
    size_t m_observerId=0;
    const Tokenizer *m_tokenizer=nullptr;
    std::vector<Entry> m_entries;
    size_t m_frontier=0;
    size_t m_visibleFirst=0;
    size_t m_visibleCount=0;
    size_t m_tokenizedLines=0;
    ChangeHandler m_changeHandler;
    //! the pass's changed lines, and the tokens a line had before it, kept for
    //! their capacity
    std::vector<size_t> m_changed;
    std::vector<Token> m_previous;
};

}//namespace text
}//namespace gweni

#endif//_gweni_text_highlightCache_h_
