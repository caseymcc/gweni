/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_markdownDocument_h_
#define _gweni_controls_markdownDocument_h_

#include <gweni/controls/base.h>
#include <gweni/controls/richText.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/text/markdown.h>

#include <functional>
#include <vector>

namespace gweni
{
namespace controls
{

class MarkdownDocument;

//! A code block's colours: the spans its text (in the language the fence
//! names, empty for none) is shown as. Their texts, in order, must make up
//! the code exactly, else it is shown plain; a span's Code bit is dropped
//! (the block is already in the mono face, on its own background).
using MarkdownHighlighter=std::function<std::vector<RichSpan>(const gweni::String &language,
    const gweni::String &code)>;

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
//! and as high as it asks to be at that width, getSpacing() between two (the
//! skin's MarkdownBlockSpacing or MarkdownTightSpacing metric): the
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

    //! The space between two blocks: the skin's MarkdownBlockSpacing (Block,
    //! the default) or MarkdownTightSpacing (Tight, a tight list's items and
    //! the blocks in them), read as the column lays out
    enum class Spacing
    {
        Block,
        Tight
    };
    void setSpacing(Spacing spacing);
    Spacing getSpacingKind() const { return m_spacing; }
    //! The spacing in units, from the column's skin
    int getSpacing();

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
    Spacing m_spacing=Spacing::Block;
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
    //! bits as they are, a url a link, an image its alt text (or "image" for
    //! none) on a placeholder (RichSpan::Placeholder, in italics)
    static std::vector<RichSpan> richSpans(const std::vector<text::MarkdownSpan> &spans);

    //! A table's header cell: its text bold (set before setBlock)
    void setHeader(bool header) { m_header=header; }
    bool isHeader() const { return m_header; }
    //! The heading level it shows (0 for none)
    int getHeadingLevel() const { return m_headingLevel; }

    //! A theme change: the font again, a heading's size being a metric
    void updateColors() override;

protected:
    void onLink(event::Info info);
    //! Its font from the document, its heading level and whether it is a
    //! header cell
    void applyFont();

    bool m_header=false;
    int m_headingLevel=0;
    //! shows a block given by setBlock (not a marker or a code block's text,
    //! whose owners set their fonts)
    bool m_fromBlock=false;
};

//! The ScrollControl a code block scrolls sideways in: horizontal only, and a
//! vertical wheel over it goes on to its parents (a page scrolling past it)
//! instead of moving its horizontal bar.
class GWENI_EXPORT MarkdownCodeScroll: public ScrollControl
{
public:

    GWENI_CONTROL(MarkdownCodeScroll, ScrollControl);

public:
    GWENI_DRAWS_NOTHING

    bool onMouseWheeled(int delta) override;
};

//! A fenced or indented code block: its text a RichText in the skin's mono
//! face at the body font's size (MarkdownDocument::codeFont), coloured by the
//! document's highlighter if it has one, on the MarkdownCodeBlock surface,
//! Padding units in. Wrapped (MarkdownDocument::setCodeWrap(true)), the text
//! is laid out at the block's width and wraps; else it is one line per line
//! of code in a MarkdownCodeScroll that scrolls it sideways when it is wider
//! than the block, the block then that much taller for the bar.
class GWENI_EXPORT MarkdownCodeBlock: public Base, public MarkdownBlockControl
{
public:

    GWENI_CONTROL(MarkdownCodeBlock, Base);

public:
    static constexpr int Padding=8;

    void setBlock(const text::MarkdownBlock &block) override;

    //! Wraps the code (true) or scrolls it sideways (false); nothing done
    //! when it already does
    void setWrap(bool wrap);
    bool isWrapped() const { return m_wrap; }

    MarkdownText *getText() const { return m_text; }
    //! The scroller, null while wrapped
    MarkdownCodeScroll *getScroll() const { return m_wrap?nullptr:m_scroll; }
    //! The code's language, as the fence names it
    const gweni::String &getLanguage() const { return m_language; }

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

protected:
    //! The text's spans from the code, the highlighter and the font
    void showCode();

    gweni::String m_code;
    gweni::String m_language;
    bool m_wrap=true;
    MarkdownText *m_text=nullptr;
    MarkdownCodeScroll *m_scroll=nullptr;
};

//! A GFM table: a grid of MarkdownText cells, the header row's bold on the
//! MarkdownTableHeader surface, a Border-unit line (MarkdownTableBorder)
//! round and between the cells, each cell CellPaddingX and CellPaddingY units
//! in from its lines. A column is as wide as its widest cell on one line;
//! when the columns are wider than the table they are narrowed
//! (splitColumns) and their cells wrap. A cell narrower than its column is
//! placed in it by the column's alignment. Kept across setBlock: a cell
//! keeps its control by its row and column, so an edited cell is the only
//! one given new spans.
class GWENI_EXPORT MarkdownTable: public Base, public MarkdownBlockControl
{
public:

    GWENI_CONTROL(MarkdownTable, Base);

public:
    static constexpr int Border=1;
    static constexpr int CellPaddingX=6;
    static constexpr int CellPaddingY=4;

    void setBlock(const text::MarkdownBlock &block) override;

    size_t getRowCount() const { return m_cells.size(); }
    size_t getColumnCount() const { return m_aligns.size(); }
    MarkdownText *getCell(size_t row, size_t column) const;
    text::MarkdownBlock::Align getAlign(size_t column) const { return m_aligns[column]; }

    //! The x of each column's left line and, last, of the right line, from
    //! the table's left, as last laid out
    const std::vector<int> &getColumnEdges() const { return m_columnEdges; }
    //! The y of each row's top line and, last, of the bottom line
    const std::vector<int> &getRowEdges() const { return m_rowEdges; }

    //! Column widths (lines aside) for columns that want natural widths in
    //! available units: the natural widths when they fit; else each column
    //! wanting no more than an even share of what is left keeps its width
    //! and the rest share what remains in proportion to theirs, the integer
    //! remainder to the lowest-index ones, each at least 1, so the widths add
    //! up to available (when it is at least one a column).
    static std::vector<int> splitColumns(const std::vector<int> &natural, int available);

    //! X: its columns at their natural widths and the lines. Y: its rows at
    //! the widths the X arrange gave the cells, as layout::Flow measures.
    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

protected:
    std::vector<std::vector<MarkdownText *>> m_cells;
    std::vector<text::MarkdownBlock::Align> m_aligns;
    //! each column's widest cell on one line, as the X measure gave it
    std::vector<int> m_natural;
    std::vector<int> m_columnWidths;
    std::vector<int> m_columnEdges;
    std::vector<int> m_rowHeights;
    std::vector<int> m_rowEdges;
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

//! A list item: its blocks in a column the skin's MarkdownListIndent in from
//! its left, its marker (a MarkdownText) right-aligned in that indent,
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

    //! A theme change: the indent again, a metric
    void updateColors() override;

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
//! a heading at headingSize(level) and bold, its inline code on the
//! MarkdownCode surface), a rule an internal::MarkdownRule,
//! a quote an internal::MarkdownQuote, a list an internal::MarkdownList of
//! internal::MarkdownListItem, a code block an internal::MarkdownCodeBlock, a
//! table an internal::MarkdownTable, and raw HTML a MarkdownText of the HTML
//! as it was written. An image is its alt text on a placeholder.
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
    //! A heading's size in units: the skin's MarkdownHeading1..6 metric (24,
    //! 20, 17, 15, 13, 12 by default), each for an 11-unit body font and
    //! scaled by the body font's size / 11
    float headingSize(int level);
    //! The font of code: the skin's mono face at the body font's size
    gweni::Font codeFont();

    //! Code blocks wrap at the document's width (true) or scroll sideways
    //! (false, the default)
    void setCodeWrap(bool wrap);
    bool getCodeWrap() const { return m_codeWrap; }
    //! Colours every code block (none by default: plain); a new one shows
    //! every code block again
    void setCodeHighlighter(MarkdownHighlighter highlighter);
    const MarkdownHighlighter &getCodeHighlighter() const { return m_highlighter; }

    //! Called with a link's url in info.string when one of the document's
    //! links is clicked (RichText::onLinkClicked); gweni opens nothing
    event::Caller onLinkClicked;

private:
    //! Every block control under the document that is no column, and the
    //! block it shows, with each list item's marker (block null)
    void forEachBlock(const std::function<void(Base *, const text::MarkdownBlock *)> &visit);

    gweni::String m_markdown;
    gweni::Font m_font;
    bool m_hasFont=false;
    bool m_codeWrap=false;
    MarkdownHighlighter m_highlighter;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_markdownDocument_h_
