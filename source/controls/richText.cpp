/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/richText.h>

#include <algorithm>
#include <utility>

namespace gweni
{
namespace controls
{

RichSpan RichSpan::plain(const String &text, unsigned style)
{
    RichSpan span;

    span.text=text;
    span.style=style;
    return span;
}

RichSpan RichSpan::colored(const String &text, const Color &color, unsigned style)
{
    RichSpan span=plain(text, style);

    span.color=color;
    return span;
}

RichSpan RichSpan::link(const String &text, const String &url, unsigned style)
{
    RichSpan span=plain(text, style);

    span.url=url;
    return span;
}

// A link whose text is the address itself, which a line may break inside
static bool looksLikeUrl(const String &text)
{
    return (text.find("://") != String::npos) || (text.compare(0, 4, "www.") == 0);
}

GWENI_CONTROL_CONSTRUCTOR(RichText)
{
    setSizeFlags(SizeFlags(SizeFlag::Elastic, SizeFlag::Elastic));
}

void RichText::spansChanged()
{
    m_measureStale=true;
    invalidate();
    addStateChange(StateChange_Text);
}

void RichText::setSpans(std::vector<RichSpan> spans)
{
    if(spans == m_spans)
        return;

    m_spans=std::move(spans);
    spansChanged();
}

void RichText::setSpan(size_t index, const RichSpan &span)
{
    if((index >= m_spans.size()) || (m_spans[index] == span))
        return;

    m_spans[index]=span;
    spansChanged();
}

void RichText::setSpanText(size_t index, const String &text)
{
    if((index >= m_spans.size()) || (m_spans[index].text == text))
        return;

    m_spans[index].text=text;
    spansChanged();
}

void RichText::appendSpan(const RichSpan &span)
{
    m_spans.push_back(span);
    spansChanged();
}

void RichText::clear()
{
    if(m_spans.empty())
        return;

    m_spans.clear();
    spansChanged();
}

String RichText::getText() const
{
    String text;

    for(const RichSpan &span:m_spans)
        text+=span.text;
    return text;
}

void RichText::setFont(const Font &font)
{
    if(m_hasFont && (m_font == font))
        return;

    m_font=font;
    m_hasFont=true;
    spansChanged();
}

const Font &RichText::getFont()
{
    return m_hasFont?m_font:getSkin()->getDefaultFont();
}

Font RichText::spanFont(size_t index)
{
    const RichSpan &span=m_spans[index];
    const Font &base=getFont();
    Font font=base;

    if(span.style&RichSpan::Code)
    {
        font=getSkin()->getDefaultMonoFont();
        font.size=base.size;
    }
    font.bold=(span.style&RichSpan::Bold) != 0;
    font.italic=(span.style&RichSpan::Italic) != 0;
    return font;
}

Color RichText::spanColor(size_t index)
{
    const RichSpan &span=m_spans[index];

    if(span.color.a != 0)
        return span.color;
    if(span.isLink())
        return getSkin()->Colors.RichText.Link;
    return getSkin()->Colors.Label.Default;
}

std::vector<text::RichLayoutSpan> RichText::layoutSpans()
{
    std::vector<text::RichLayoutSpan> spans(m_spans.size());

    for(size_t i=0; i < m_spans.size(); ++i)
    {
        spans[i].text=m_spans[i].text;
        spans[i].font=spanFont(i);
        spans[i].url=m_spans[i].isLink() && looksLikeUrl(m_spans[i].text);
        spans[i].padding=(m_spans[i].style&RichSpan::Code)?CodePadding:0;
    }
    return spans;
}

void RichText::ensureMeasured()
{
    const unsigned serial=getSkin()->getRenderer()->textLayoutSerial();

    // a new text layout (a text AA mode) or font: the spans' measures are stale
    // as their text's are
    if(!m_measureStale && (serial == m_measureSerial))
    {
        const std::vector<text::RichLayoutSpan> &measured=m_layout.spans();

        // the skin's default or mono font changed under a span that takes it
        bool same=(measured.size() == m_spans.size());

        for(size_t i=0; same && (i < m_spans.size()); ++i)
            same=(measured[i].font == spanFont(i));
        if(same)
            return;
    }

    text::RichMeasure measure;

    measure.carets=[this](const Font &font, const String &text, std::vector<int> &xs)
        {
            measureCarets(font, text, xs);
        };
    measure.metrics=[this](const Font &font) { return measureFontMetrics(font); };

    m_layout.setSpans(layoutSpans(), measure);
    m_measureStale=false;
    m_measureSerial=serial;
    m_layoutWidth=-1;
    m_naturalWidth=-1;
    ++m_measureCount;
}

void RichText::layoutAt(int width)
{
    ensureMeasured();
    if(width == m_layoutWidth)
        return;

    text::RichMeasure measure;

    measure.carets=[this](const Font &font, const String &text, std::vector<int> &xs)
        {
            measureCarets(font, text, xs);
        };
    m_layout.breakLines(width, measure);
    m_layoutWidth=width;
}

// As wide as the text on one line; at the width that gives it, as high as its
// lines there (the Y pass runs once every width is final)
void RichText::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::X)
    {
        ensureMeasured();
        if(m_naturalWidth < 0)
        {
            layoutAt(0);
            m_naturalWidth=m_layout.size().x;
        }
        m_preferredSize.width=preferredFromContent(m_naturalWidth+m_padding.left+m_padding.right, dim);
        return;
    }

    layoutAt(std::max(0, getBounds().w-m_padding.left-m_padding.right));
    m_preferredSize.height=preferredFromContent(m_layout.size().y+m_padding.top+m_padding.bottom, dim);
}

}//namespace controls
}//namespace gweni
