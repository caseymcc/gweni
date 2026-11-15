/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_text_markdown_h_
#define _gweni_text_markdown_h_

#include <gweni/export.h>
#include <gweni/platforms/platformTypes.h>

#include <cstdint>
#include <vector>

namespace gweni
{
namespace text
{

//! One run of a Markdown block's inline text in one style: what a RichSpan
//! is made from (its style bits are RichSpan::Style's).
struct GWENI_EXPORT MarkdownSpan
{
    enum Style: unsigned
    {
        Plain=0,
        Bold=1,     //!< **strong**
        Italic=2,   //!< *emphasis*
        Code=4,     //!< `inline code`
        Strike=8    //!< ~~GFM strikethrough~~
    };

    //! The text as shown: entities decoded, a soft break a ' ', a hard break
    //! a '\n', raw inline HTML as it was written
    gweni::String text;
    unsigned style=Plain;
    //! A link's target (an autolink's with "http://" before a bare www. one
    //! and "mailto:" before a bare e-mail address); empty for no link
    gweni::String url;
    //! An image ![alt](src): text is its alt text (flattened, every style
    //! inside it dropped), src its source
    bool image=false;
    gweni::String src;

    bool operator==(const MarkdownSpan &other) const
    {
        return (text == other.text) && (style == other.style) && (url == other.url)
            && (image == other.image) && (src == other.src);
    }
    bool operator!=(const MarkdownSpan &other) const { return !(*this == other); }
};

//! A block of a Markdown document as md4c parses it (CommonMark 0.31 with
//! GFM tables, strikethrough and autolinks), kept as data: a leaf block
//! holds inline spans or verbatim text, a container its child blocks.
struct GWENI_EXPORT MarkdownBlock
{
    enum class Type
    {
        Paragraph,  //!< spans; also the text of a tight list item
        Heading,    //!< spans, level 1..6
        List,       //!< ListItem children; ordered, start, marker, tight
        ListItem,   //!< blocks
        Quote,      //!< blocks
        Rule,       //!< nothing
        CodeBlock,  //!< text (no final '\n'), language, fenced
        Html,       //!< text: a raw HTML block, shown as it was written
        Table,      //!< TableRow children, the header row first; aligns
        TableRow,   //!< TableCell children; header
        TableCell   //!< spans, align
    };

    enum class Align
    {
        Default,
        Left,
        Center,
        Right
    };

    Type type=Type::Paragraph;
    std::vector<MarkdownSpan> spans;    //!< Paragraph, Heading, TableCell
    gweni::String text;                 //!< CodeBlock, Html
    std::vector<MarkdownBlock> children;//!< List, ListItem, Quote, Table, TableRow

    int level=0;            //!< Heading: 1..6
    bool ordered=false;     //!< List
    unsigned start=1;       //!< List: an ordered list's first number
    //! List: an unordered list's bullet ('-', '+', '*'), an ordered one's
    //! delimiter ('.', ')')
    char marker='-';
    bool tight=true;        //!< List: no blank line between its items
    gweni::String language; //!< CodeBlock: the info string's first word
    bool fenced=false;      //!< CodeBlock: ``` or ~~~, not indented
    bool header=false;      //!< TableRow: the header row
    Align align=Align::Default;     //!< TableCell: its column's
    std::vector<Align> aligns;      //!< Table: each column's

    //! A hash of the block's type, attributes, content and children's hashes
    //! (so of its whole nesting), set by parseMarkdown: two blocks with one
    //! hash are taken to be the same block. FNV-1a, 64-bit on every target.
    uint64_t hash=0;

    //! Every span's text, in order
    gweni::String plainText() const;

    //! Works hash out from the block's fields and its children's hashes,
    //! which it sets first
    void updateHash();

    //! The same block, field by field and child by child (hash included)
    bool operator==(const MarkdownBlock &other) const;
    bool operator!=(const MarkdownBlock &other) const { return !(*this == other); }
};

//! The top-level blocks of markdown, each with its hash (and its children's)
//! set. Raw HTML, a block or inline, is kept as the text it was written as;
//! footnotes, task lists and the like are not recognised, so their syntax is
//! text too. A named entity is decoded only if it is one of the common HTML
//! names md4c leaves to the caller (amp, lt, gt, quot, apos, nbsp, copy,
//! reg, trade, hellip, mdash, ndash, lsquo, rsquo, ldquo, rdquo, bull,
//! middot, times, divide, deg, plusmn, para, sect, laquo, raquo, larr, rarr,
//! uarr, darr, harr, euro, pound, yen, cent); any other stays as written. A
//! numeric one is decoded, U+FFFD for an invalid codepoint, as for NUL.
//! Quotes and lists nest MarkdownMaxNesting deep at most: one deeper is not a
//! block of its own, its blocks going into the deepest one kept (a tight
//! item's text a paragraph there), so a hostile input cannot build a model,
//! or controls, deep enough to exhaust the stack.
GWENI_EXPORT std::vector<MarkdownBlock> parseMarkdown(const gweni::String &markdown);

//! How one block of a new parse maps onto the previous parse's blocks at the
//! same level (markdownDiff)
struct GWENI_EXPORT MarkdownBlockMatch
{
    enum class Kind
    {
        //! The same block (equal hash): its controls are kept as they are,
        //! moved at most
        Kept,
        //! The old block at old, of the same type, with other content: its
        //! controls are kept and given the new content; for a container,
        //! children holds the diff of its children
        Changed,
        //! No old block: built new
        New
    };

    Kind kind=Kind::New;
    //! The old block's index among its siblings (Kept, Changed)
    size_t old=0;
    //! A Changed container's children, matched against the old block's
    std::vector<MarkdownBlockMatch> children;
};

//! Matches the blocks of a new parse against the previous one's, one entry
//! per new block, in order:
//! - the blocks equal at the start and at the end (by hash) are Kept;
//! - in the middle, the longest common subsequence of hashes is Kept (when
//!   the middle is no more than MarkdownDiffLimit old-times-new blocks; past
//!   it, no subsequence is looked for);
//! - a new block left over whose hash an old block left over has is Kept,
//!   the old block moved (the first such old block in order, so a swap of
//!   two blocks keeps both);
//! - the rest are paired in order within each gap between two Kept blocks:
//!   the i-th new block left in a gap with the i-th old block left in it when
//!   they are of one type (Changed, a container's children diffed the same
//!   way), else New.
//! An old block no entry names is gone. Each old block is named once at most.
GWENI_EXPORT std::vector<MarkdownBlockMatch> markdownDiff(const std::vector<MarkdownBlock> &old,
    const std::vector<MarkdownBlock> &now);

//! The most quotes and lists parseMarkdown nests inside one another
constexpr int MarkdownMaxNesting=32;

//! The most old-times-new blocks markdownDiff runs its subsequence over
constexpr size_t MarkdownDiffLimit=1<<20;

//! The type's name ("Paragraph", "ListItem", ...), for tests and dumps
GWENI_EXPORT const char *markdownBlockTypeName(MarkdownBlock::Type type);

}//namespace text
}//namespace gweni

#endif//_gweni_text_markdown_h_
