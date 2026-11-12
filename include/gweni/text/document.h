/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_text_document_h_
#define _gweni_text_document_h_

#include <gweni/export.h>
#include <gweni/platforms/platformTypes.h>

#include <cstddef>
#include <functional>
#include <vector>

namespace gweni
{
namespace text
{

//! A place in a text: a line and a byte offset in that line (0 is before its
//! first byte, the line's length after its last). Lines are split on '\n'
//! and hold no '\n' of their own.
struct Position
{
    size_t line=0;
    size_t byte=0;

    bool operator==(const Position &other) const { return line == other.line && byte == other.byte; }
    bool operator!=(const Position &other) const { return !(*this == other); }
    bool operator<(const Position &other) const
    {
        return line < other.line || (line == other.line && byte < other.byte);
    }
    bool operator<=(const Position &other) const { return !(other < *this); }
};

//! The text from \p begin up to \p end (begin <= end; equal is empty).
struct Range
{
    Position begin;
    Position end;

    bool empty() const { return begin == end; }
};

//! One change of a text: what \p range held is replaced by \p text. \p removed
//! and \p textEnd are filled in by whoever applies it (Document::apply(), or
//! a control keeping its text in a String of its own), so a record carries
//! everything its inverse needs and undoing it needs no knowledge of how the
//! positions are counted. The positions are only compared and stored, never
//! interpreted, outside the applier: a single-line control can count them
//! as (0, byte offset), a Document as (line, byte).
struct Edit
{
    //! The text replaced, in the positions before the edit.
    Range range;
    //! The text put in its place.
    String text;
    //! What \p range held before the edit.
    String removed;
    //! Where \p text ends once it is in, in the positions after the edit.
    Position textEnd;

    //! The edit that takes this one back: the inserted text replaced by the
    //! removed one, which ends where this edit's range ended.
    Edit inverse() const
    {
        Edit edit;
        edit.range={range.begin, textEnd};
        edit.text=removed;
        edit.removed=text;
        edit.textEnd=range.end;
        return edit;
    }
};

//! The lines an applied edit replaced: lines [first, first+removed) of the
//! text before it are now lines [first, first+inserted). An edit inside one
//! line is (line, 1, 1). Lines past them are the same text, moved by
//! inserted-removed.
struct LineChange
{
    size_t first=0;
    size_t removed=0;
    size_t inserted=0;
};

//! A text as a vector of lines, for an editor whose edit must cost the lines
//! it changes and not the whole text. Line i is O(1) to reach, and an edit
//! rewrites only the lines its range and its text touch: one that adds or
//! removes lines moves the vector's later entries (their strings' handles,
//! not their bytes), and does nothing per line beyond that. There is always
//! at least one line (an empty text is one empty line).
//!
//! Observers (addObserver()) are told of every applied edit and every
//! setText() as the lines it replaced, after the change.
class GWENI_EXPORT Document
{
public:
    typedef std::function<void(const LineChange &)> Observer;

    Document();
    explicit Document(const String &text);

    //! Replaces the whole text, split on '\n' ("\r\n" is read as "\n").
    //! Observers see it as every old line replaced by every new one.
    void setText(const String &text);
    //! The lines joined with '\n'.
    String text() const;

    size_t lineCount() const { return m_lines.size(); }
    //! Line \p index without its '\n'; \p index must be below lineCount().
    const String &line(size_t index) const { return m_lines[index]; }

    //! The text of \p range, its lines joined with '\n'. The range is
    //! clamped as apply() clamps it.
    String textIn(Range range) const;

    //! The position after the last byte of the last line.
    Position endPosition() const;
    //! \p position with its line clamped to the last line and its byte to
    //! that line's length.
    Position clamp(Position position) const;

    //! Applies \p edit: its range is clamped (and its ends swapped if begin
    //! is after end), "\r\n" in its text is read as "\n", and \p removed and
    //! \p textEnd are filled in. Returns the lines it replaced, which the
    //! observers are told too.
    LineChange apply(Edit &edit);
    //! apply() of {range, text}, returning the applied record (for an undo
    //! stack, say).
    Edit replace(Range range, const String &text);
    Edit insert(Position position, const String &text) { return replace({position, position}, text); }
    Edit erase(Range range) { return replace(range, String()); }

    //! Calls \p observer after every change of the text, until
    //! removeObserver() with the id returned. An observer must not add or
    //! remove observers while it is being called.
    size_t addObserver(Observer observer);
    void removeObserver(size_t id);

private:
    void notify(const LineChange &change);

    std::vector<String> m_lines;

    struct ObserverEntry
    {
        size_t id;
        Observer observer;
    };
    std::vector<ObserverEntry> m_observers;
    size_t m_nextObserverId=1;
};

//! \p text with every "\r\n" made "\n" (a lone '\r' is kept).
GWENI_EXPORT String normalizeNewlines(const String &text);

}//namespace text
}//namespace gweni

#endif//_gweni_text_document_h_
