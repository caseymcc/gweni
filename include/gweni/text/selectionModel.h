/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_text_selectionmodel_h_
#define _gweni_text_selectionmodel_h_

#include <gweni/export.h>
#include <gweni/platforms/platformTypes.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace gweni
{
namespace text
{

//! A place in a document of text blocks: the block, by its path (its index
//! in its parent's blocks, from the outermost; empty for a lone paragraph),
//! and the byte before which it is in the block's shown text, a codepoint
//! boundary. Positions are ordered as the blocks are read: by path, a block
//! before the blocks inside it ({1} < {1, 0} < {2}), then by byte.
struct GWENI_EXPORT SelectionPosition
{
    std::vector<size_t> block;
    size_t offset=0;

    SelectionPosition()=default;
    explicit SelectionPosition(size_t offset): offset(offset) {}
    SelectionPosition(std::vector<size_t> block, size_t offset): block(std::move(block)), offset(offset) {}

    bool operator==(const SelectionPosition &other) const
    {
        return (offset == other.offset) && (block == other.block);
    }
    bool operator!=(const SelectionPosition &other) const { return !(*this == other); }
    bool operator<(const SelectionPosition &other) const
    {
        if(block != other.block)
            return block < other.block;
        return offset < other.offset;
    }
    bool operator<=(const SelectionPosition &other) const { return !(other < *this); }
};

//! A selection of shown text across the blocks of one container: an anchor
//! (where it started) and a focus (where it is extended to), anchor == focus
//! being none. It is plain data, held by the outermost container (a lone
//! RichText's own, a document's for every block in it); the container hit-
//! tests the mouse into positions and each block asks range() for the part
//! of its text to draw. serial() moves on every change, so a block can tell
//! whether what it drew is still the selection.
class GWENI_EXPORT SelectionModel
{
public:
    const SelectionPosition &anchor() const { return m_anchor; }
    const SelectionPosition &focus() const { return m_focus; }
    //! The earlier and the later end
    const SelectionPosition &start() const { return (m_focus < m_anchor)?m_focus:m_anchor; }
    const SelectionPosition &end() const { return (m_focus < m_anchor)?m_anchor:m_focus; }
    bool empty() const { return m_anchor == m_focus; }

    //! Anchor and focus at \p at: nothing selected, a drag starting there
    void setCaret(const SelectionPosition &at);
    //! The focus to \p at, the anchor kept
    void extendTo(const SelectionPosition &at);
    //! From \p anchor to \p focus
    void select(const SelectionPosition &anchor, const SelectionPosition &focus);
    //! Nothing selected, both ends at the start
    void clear();

    //! The bytes of block \p block (whose text is \p length bytes) the
    //! selection covers, [first, second), each end clamped to length; false
    //! (and {0, 0}) when it covers none of them. A block between the ends is
    //! covered whole.
    bool range(const std::vector<size_t> &block, size_t length, std::pair<size_t, size_t> &bytes) const;

    //! Counts every change of either end
    unsigned serial() const { return m_serial; }

private:
    bool set(const SelectionPosition &anchor, const SelectionPosition &focus);

    SelectionPosition m_anchor;
    SelectionPosition m_focus;
    unsigned m_serial=0;
};

//! The word, run of blanks or run of punctuation byte \p at of \p text is in
//! (the one before it at the text's end or before a '\n'), as [first,
//! second): letters, digits, '_' and every non-ASCII codepoint make words,
//! spaces and tabs blanks, and a '\n' is a run of its own. {at, at} for an
//! empty text.
GWENI_EXPORT std::pair<size_t, size_t> wordAt(const gweni::String &text, size_t at);

}//namespace text
}//namespace gweni

#endif//_gweni_text_selectionmodel_h_
