/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_richtext_h_
#define _gweni_controls_richtext_h_

#include <gweni/controls/base.h>
#include <gweni/text/richLayout.h>
#include <gweni/text/selectionModel.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace gweni
{
namespace controls
{

//! One styled run of a RichText paragraph.
struct GWENI_EXPORT RichSpan
{
    //! What a span looks like; the flags combine (a bold link, struck-through
    //! italic code).
    enum Style: unsigned
    {
        Plain=0,
        Bold=1,         //!< the font family's bold face
        Italic=2,       //!< its italic face (with Bold, its bold italic one)
        Code=4,         //!< the skin's mono face, on the RichTextCode surface
        Strike=8,       //!< a line through the text
        Underline=16    //!< a line under the text
    };

    gweni::String text;
    unsigned style=Plain;
    //! The text's colour; alpha 0 (the default) is the skin's: Label.Default,
    //! a link's RichTextLink
    gweni::Color color{0, 0, 0, 0};
    //! A link's target: a span with one is a link, in the link colour and
    //! underlined
    gweni::String url;

    bool isLink() const { return !url.empty(); }

    static RichSpan plain(const gweni::String &text, unsigned style=Plain);
    static RichSpan colored(const gweni::String &text, const gweni::Color &color, unsigned style=Plain);
    static RichSpan link(const gweni::String &text, const gweni::String &url, unsigned style=Plain);

    bool operator==(const RichSpan &other) const
    {
        return (text == other.text) && (style == other.style) && (color == other.color) && (url == other.url);
    }
    bool operator!=(const RichSpan &other) const { return !(*this == other); }
};

//! One paragraph of styled spans wrapped together (text::RichLayout): bold,
//! italic, inline code, coloured, struck-through, underlined text and links in
//! one run of lines, each line's pieces on one baseline.
//!
//! It has no children: the layout is data, and the textured skin draws each
//! piece (one span's run on one line) as one text run of its own, in its
//! span's font and colour, and the code backgrounds, underlines and
//! strikethroughs as a list of primitive slots that follows the pieces. A
//! changed span or a new width lays out and redraws this paragraph alone, and
//! only the runs and slots whose bytes change are written.
//!
//! It is as wide as its text on one line (each '\n' a line) and, at the width
//! it is given, as high as its lines there: a wrapped paragraph, its height
//! worked out in the Y pass from its final width. A url span (a link whose
//! text is a URL, "://" in it or starting "www.") may also break after '/',
//! '-', '?', '&' and '='.
class GWENI_EXPORT RichText: public Base
{
public:

    GWENI_CONTROL(RichText, Base);

public:

    //! Room a Code span's background takes either side of its text
    static constexpr int CodePadding=3;

    void setSpans(std::vector<RichSpan> spans);
    const std::vector<RichSpan> &getSpans() const { return m_spans; }
    size_t getSpanCount() const { return m_spans.size(); }
    //! Replaces span \p index (nothing past the last); an equal span changes nothing
    void setSpan(size_t index, const RichSpan &span);
    //! Gives span \p index new text, keeping its style
    void setSpanText(size_t index, const gweni::String &text);
    void appendSpan(const RichSpan &span);
    void clear();

    //! The text shown: every span's, in order (a '\n' a line break)
    gweni::String getText() const;

    //! The font of a plain span (the skin's default font until set): a Bold or
    //! Italic span takes its family's face of that style at its size, a Code
    //! one the skin's mono face at its size.
    void setFont(const gweni::Font &font);
    const gweni::Font &getFont();

    //! The font span \p index is drawn in
    gweni::Font spanFont(size_t index);
    //! The colour span \p index's text is drawn in (its own, or the skin's)
    gweni::Color spanColor(size_t index);

    //! The lines and pieces as last laid out, in this control's coordinates
    //! less its padding (contentOrigin() is their origin)
    const text::RichLayout &getLayout() const { return m_layout; }
    //! Where the layout's (0, 0) is in this control
    gweni::Point contentOrigin() const { return gweni::Point(m_padding.left, m_padding.top); }

    //! How many times the spans were measured (each a measureCarets of every
    //! span and its fonts' metrics): once per change of the spans, the font or
    //! the renderer's text layout, however many widths it is laid out at.
    size_t measureCount() const { return m_measureCount; }

    void calculateSize(skin::Base *skin, Dim dim) override;

    //! \sect{Links}
    //! A link is a run of consecutive spans with the same url. Hovered, its
    //! spans are drawn in the skin's RichText.LinkHover (their own colour
    //! aside) and the cursor is cursor::Finger (the web's "pointer"); a press
    //! and a release on the same link call onLinkClicked, with the url in
    //! info.string and the link's first span in info.integer. gweni opens
    //! nothing itself: the app routes the url.

    //! The first span of the link at \p local (this control's coordinates),
    //! -1 for none: the link whose piece's box, as high as its line, holds it
    int linkAt(const gweni::Point &local);
    //! The first span of the hovered link, -1 for none
    int getHoveredLink() const { return m_hoveredFirst; }
    //! Whether span \p index is part of the hovered link
    bool isLinkHovered(size_t index) const;

    event::Caller onLinkClicked;

    //! \sect{Selection}
    //! The selection is a text::SelectionModel: the paragraph's own, its
    //! block path empty, unless setSelectionModel gives it its container's
    //! (a document's, holding every block's) and its block's path there. A
    //! position's offset is a byte of getText(). The paragraph draws the part
    //! of the selection in its block (selectionRects(), one per line) behind
    //! its glyphs; a press puts the selection's both ends at the byte nearest
    //! it (Shift: the focus alone) and a drag extends it, a double-click
    //! selects the word under it (text::wordAt), Ctrl+A selects all of it,
    //! Ctrl+C copies the selected text, as shown, through
    //! platform::setClipboardText, and Escape clears it. A press on a link that
    //! selects nothing by its release is the link's click. The paragraph takes
    //! the keyboard focus on a press, for those keys, and is not a Tab stop.
    //! Its own selection is cleared when its spans change.

    //! A container's model and this paragraph's block in it; null gives the
    //! paragraph its own model back
    void setSelectionModel(text::SelectionModel *model, std::vector<size_t> block={});
    text::SelectionModel &selectionModel() { return m_selection?*m_selection:m_ownSelection; }
    const text::SelectionModel &selectionModel() const { return m_selection?*m_selection:m_ownSelection; }
    const std::vector<size_t> &selectionBlock() const { return m_selectionBlock; }
    //! The model changed: the paragraph draws its part of the selection
    //! again (a container calls it for each block whose part it changed)
    void selectionChanged();

    //! The bytes of getText() selected in this paragraph, [first, second);
    //! false for none
    bool selectedRange(std::pair<size_t, size_t> &bytes) const;
    bool hasSelection() const;
    gweni::String getSelectedText() const;
    //! Selects the bytes [first, last) of getText() (focus at last)
    void select(size_t first, size_t last);
    void selectAll();
    void clearSelection();
    //! Puts the selected text on the clipboard; false (nothing done) for none
    bool copySelection();

    //! The byte of getText() nearest \p local (this control's coordinates): on
    //! the line whose box holds its y (the first above them, the last below),
    //! the codepoint boundary nearest its x (a combining mark kept with its
    //! base), the line's visible end past its last piece
    size_t positionAt(const gweni::Point &local);
    //! The selection's part in this paragraph, one rect per line it reaches,
    //! in the layout's coordinates (contentOrigin() their origin): its line's
    //! box from the x of its first selected byte to that of its last, and a
    //! break (the spaces or '\n' ending the line) selected widening it by
    //! breakSelectionWidth()
    std::vector<gweni::Rect> selectionRects();
    //! How far a selected line break reaches past the line's last piece
    static int breakSelectionWidth(const text::RichLine &line) { return std::max(1, line.height/3); }

    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseLeave() override;
    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseDoubleClickLeft(int x, int y) override;
    bool onKeyEscape(bool down) override;

protected:

    void onCopy(event::Info);
    void onSelectAll(event::Info);
    //! The x, from the layout's origin, of byte \p offset of getText() on line
    //! \p line: in a piece at its caret, before a piece (in the gap a code
    //! span's padding leaves) at the piece's box, past them all at the right
    //! edge of the last one's box (0 on a line without pieces)
    int xAt(const text::RichLine &line, size_t offset) const;
    //! The byte the selection at \p offset is at: this paragraph's block
    text::SelectionPosition at(size_t offset) const
    {
        return text::SelectionPosition(m_selectionBlock, offset);
    }

    //! The link holding span \p index: its first and last span ([-1, -1] for none)
    std::pair<int, int> linkSpans(int index) const;
    //! Hovers the link holding span \p index (-1: none), marking the spans
    //! that change look and setting the cursor
    void setHoveredLink(int index);

    //! The layout's spans, from m_spans: fonts, url flags, code padding
    std::vector<text::RichLayoutSpan> layoutSpans();
    //! Measures the spans again if they, the font or the renderer's text
    //! layout changed since they were last measured
    void ensureMeasured();
    //! Breaks the measured spans into lines at width (no measures but those of
    //! pieces that start inside a span), unless they are laid out at it
    void layoutAt(int width);
    //! The spans changed: lay out and redraw
    void spansChanged();

    std::vector<RichSpan> m_spans;
    gweni::Font m_font;
    bool m_hasFont=false;

    text::RichLayout m_layout;
    bool m_measureStale=true;
    unsigned m_measureSerial=0;
    //! the width m_layout is broken at, -1 for none
    int m_layoutWidth=-1;
    //! the width of the text on one line, -1 until measured
    int m_naturalWidth=-1;
    size_t m_measureCount=0;

    //! the hovered link's first and last span, -1 for none
    int m_hoveredFirst=-1;
    int m_hoveredLast=-1;
    //! the first span of the link a left press is on, -1 for none
    int m_pressedLink=-1;

    text::SelectionModel m_ownSelection;
    //! a container's model, null for m_ownSelection
    text::SelectionModel *m_selection=nullptr;
    std::vector<size_t> m_selectionBlock;
    //! a left press's drag is extending the selection
    bool m_selecting=false;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_richtext_h_
