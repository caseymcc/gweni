/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_richtext_h_
#define _gweni_controls_richtext_h_

#include <gweni/controls/base.h>
#include <gweni/text/richLayout.h>

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

protected:

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
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_richtext_h_
