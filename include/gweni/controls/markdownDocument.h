/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_markdownDocument_h_
#define _gweni_controls_markdownDocument_h_

#include <gweni/controls/base.h>
#include <gweni/controls/richText.h>
#include <gweni/text/markdown.h>

#include <vector>

namespace gweni
{
namespace controls
{

class MarkdownDocument;

namespace internal
{

//! A control showing one Markdown block (text::MarkdownBlock): the column
//! that holds it hands it the block when it is built and again when the diff
//! finds it Changed (the same type, other content), so the control is kept
//! and only its content is set.
class GWENI_EXPORT MarkdownBlockControl
{
public:
    virtual ~MarkdownBlockControl()=default;

    //! Shows block (of the type the control was built for)
    virtual void setBlock(const text::MarkdownBlock &block)=0;
};

//! Blocks stacked top to bottom, each as wide as the column less its padding
//! and as high as it asks to be at that width, getSpacing() between two: the
//! document, a quote and a list item hold their blocks in one, a list its
//! items. The blocks are owner-placed (Placement::Owner): their widths in the
//! X arrange, their y and heights in the Y arrange, so a block whose height
//! stays lays out alone, and the blocks below one that grew are moved, which
//! does not dirty their subtrees.
//!
//! setBlocks() matches the new blocks against the ones it shows
//! (text::markdownDiff): a Kept block keeps its control untouched, a Changed
//! one keeps its control and is given the new block, a New one is built, and
//! the control of an old block no new one names is hidden at once and
//! deleted with the canvas's next delayed deletes. The controls stay in the
//! blocks' order among the column's children, moving only those out of it.
class GWENI_EXPORT MarkdownColumn: public Base
{
public:

    GWENI_CONTROL(MarkdownColumn, Base);

public:
    GWENI_DRAWS_NOTHING

    //! Shows blocks, keeping the controls of the ones the diff matches
    void setBlocks(const std::vector<text::MarkdownBlock> &blocks);
    const std::vector<text::MarkdownBlock> &getBlocks() const { return m_blocks; }
    size_t getBlockCount() const { return m_blockControls.size(); }
    //! The control showing block index (null past the last)
    Base *getBlockControl(size_t index) const;

    //! Units between two blocks
    void setSpacing(int spacing);
    int getSpacing() const { return m_spacing; }

    //! The document the column is in (itself for the document)
    MarkdownDocument *getDocument() const { return m_document; }
    void setDocument(MarkdownDocument *document) { m_document=document; }

    //! X: its widest block's preferred width and the padding. Y: the blocks'
    //! heights at the widths the X arrange gave them, the spacing between
    //! them and the padding; a Y measure before this pass's X arrange (a
    //! ScrollControl measuring its content first) enters no block and keeps
    //! the last height, as layout::Flow does.
    void calculateSize(skin::Base *skin, Dim dim) override;
    //! X: each block's x and width. Y: each block's y and height. Both before
    //! the blocks are arranged.
    void arrange(skin::Base *skin, Dim dim) override;

protected:
    //! The control for a new block, a child of this column, not yet given
    //! the block (setBlocks does that)
    virtual Base *buildBlock(const text::MarkdownBlock &block);
    //! The blocks changed (setBlocks has applied them): a list numbers its
    //! items' markers
    virtual void blocksChanged() {}

    std::vector<text::MarkdownBlock> m_blocks;
    std::vector<Base *> m_blockControls;
    int m_spacing=8;
    MarkdownDocument *m_document=nullptr;
};

//! A paragraph, a heading, a table cell or a list marker: a RichText of the
//! block's spans in the document's font for the block (a heading's size,
//! bold).
class GWENI_EXPORT MarkdownText: public RichText, public MarkdownBlockControl
{
public:

    GWENI_CONTROL(MarkdownText, RichText);

public:
    void setBlock(const text::MarkdownBlock &block) override;
    //! The RichText spans a Markdown block's spans are shown as: the style
    //! bits as they are, a url a link, an image its alt text in italics
    static std::vector<RichSpan> richSpans(const std::vector<text::MarkdownSpan> &spans);

protected:
    void onLink(event::Info info);
};

//! A horizontal rule: a 1-unit line across its middle (the MarkdownRule
//! surface), RuleHeight units high.
class GWENI_EXPORT MarkdownRule: public Base, public MarkdownBlockControl
{
public:

    GWENI_CONTROL(MarkdownRule, Base);

public:
    static constexpr int RuleHeight=9;

    void setBlock(const text::MarkdownBlock &) override {}
    void calculateSize(skin::Base *skin, Dim dim) override;
};

//! A block quote: its blocks in a column right of a bar (the MarkdownQuoteBar
//! surface, BarWidth units wide down its left edge), BarWidth+BarGap units in.
class GWENI_EXPORT MarkdownQuote: public MarkdownColumn, public MarkdownBlockControl
{
public:

    GWENI_CONTROL(MarkdownQuote, MarkdownColumn);

public:
    static constexpr int BarWidth=3;
    static constexpr int BarGap=10;

    void setBlock(const text::MarkdownBlock &block) override;
};

class MarkdownListItem;

//! A list: its items in a column, the spacing between them the document's
//! tight or loose list spacing, each item's marker set from the list (a
//! bullet by the list's depth, or its number and the delimiter).
class GWENI_EXPORT MarkdownList: public MarkdownColumn, public MarkdownBlockControl
{
public:

    GWENI_CONTROL(MarkdownList, MarkdownColumn);

public:
    GWENI_DRAWS_NOTHING

    void setBlock(const text::MarkdownBlock &block) override;

    //! How many lists hold this one (0 for one in no list)
    int getDepth() const { return m_depth; }
    bool isTight() const { return m_tight; }
    //! The marker of item index: "1." for an ordered list starting at 1 and
    //! delimited by '.', else a bullet by the depth (U+2022, U+25E6, U+25AA,
    //! then the last again)
    gweni::String markerText(size_t index) const;

protected:
    Base *buildBlock(const text::MarkdownBlock &block) override;
    void blocksChanged() override;

    int m_depth=0;
    bool m_ordered=false;
    unsigned m_start=1;
    char m_marker='-';
    bool m_tight=true;
};

//! A list item: its blocks in a column the document's list indent in from its
//! left, its marker (a MarkdownText) right-aligned in that indent,
//! MarkerGap units before the blocks, on their first line.
class GWENI_EXPORT MarkdownListItem: public MarkdownColumn, public MarkdownBlockControl
{
public:

    GWENI_CONTROL(MarkdownListItem, MarkdownColumn);

public:
    GWENI_DRAWS_NOTHING

    static constexpr int MarkerGap=6;

    void setBlock(const text::MarkdownBlock &block) override;

    //! Shows text as the marker (nothing done when it is already shown)
    void setMarker(const gweni::String &text);
    MarkdownText *getMarker() const { return m_marker; }

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

protected:
    MarkdownText *m_marker=nullptr;
};

}//namespace internal

//! A Markdown document (CommonMark with GFM tables and strikethrough, parsed
//! by text::parseMarkdown) shown as a column of blocks, with no scroll bar of
//! its own: MarkdownView is one in a ScrollControl, and anything that gives
//! it a width (a dock, an owner placing it, a log view's row) can host one.
//!
//! **Width and height.** It is laid out at the width its parent gives it:
//! every block is that wide less its padding, and a paragraph wraps there.
//! Its preferred height, worked out in the Y pass from that width, is its
//! blocks' heights and the spacing between them; its preferred width is its
//! widest block's on one line. Docked Top by default, so a parent gives it
//! its width and it asks for its height. It needs no ScrollControl and no
//! canvas-wide state.
//!
//! **Incremental.** setMarkdown parses the whole text and matches its blocks
//! against the ones shown (text::markdownDiff, at every level of the nesting):
//! a block that is the same keeps its control and lays out nothing; a changed
//! one keeps its control and is given its new content; only new ones are
//! built and only removed ones deleted. The same text again does nothing.
//!
//! The blocks: a paragraph or a heading is a RichText (internal::MarkdownText,
//! a heading at headingSize(level) and bold), a rule an internal::MarkdownRule,
//! a quote an internal::MarkdownQuote, a list an internal::MarkdownList of
//! internal::MarkdownListItem; a code block, raw HTML and a table are shown
//! as their text for now.
class GWENI_EXPORT MarkdownDocument: public internal::MarkdownColumn
{
public:

    GWENI_CONTROL(MarkdownDocument, internal::MarkdownColumn);

public:
    GWENI_DRAWS_NOTHING

    //! Parses markdown and shows it, keeping the controls of the blocks that
    //! match those shown; the same text again does nothing
    void setMarkdown(const gweni::String &markdown);
    const gweni::String &getMarkdown() const { return m_markdown; }

    //! The font of body text (the skin's default font until set)
    void setFont(const gweni::Font &font);
    gweni::Font getFont();
    //! The font of a heading of level 1..6: the body font, bold, at
    //! headingSize(level)
    gweni::Font headingFont(int level);
    //! A heading's size in units: 24, 20, 17, 15, 13, 12 for levels 1..6 at
    //! the default font's 11 (each scaled by the body font's size / 11)
    float headingSize(int level);

    //! Units between two blocks of the document, a quote or a loose list's
    //! item, and between two items of a tight list
    static constexpr int BlockSpacing=10;
    static constexpr int TightSpacing=2;
    //! Units a list item's blocks are in from its left (its marker in them)
    static constexpr int ListIndent=22;

    //! Called with a link's url in info.string when one of the document's
    //! links is clicked (RichText::onLinkClicked); gweni opens nothing
    event::Caller onLinkClicked;

private:
    gweni::String m_markdown;
    gweni::Font m_font;
    bool m_hasFont=false;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_markdownDocument_h_
