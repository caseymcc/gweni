/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/richText.h>
#include <gweni/gweni.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/text/cells.h>

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
    setCursor(cursor::Beam);
    // the focus for Ctrl+A and Ctrl+C, taken by a press, never by Tab
    setKeyboardInputEnabled(true);
    setTabable(false);
    addAccelerator("Ctrl + C", &RichText::onCopy);
    addAccelerator("Ctrl + A", &RichText::onSelectAll);
}

void RichText::spansChanged()
{
    // a link a span changed under is hovered again from the next move
    if(m_hoveredFirst >= 0)
        setHoveredLink(-1);
    m_pressedLink=-1;
    m_selecting=false;
    // its own selection's bytes are those of a text that is gone
    if(!m_selection && !m_ownSelection.empty())
        m_ownSelection.clear();
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

    if(isLinkHovered(index))
        return getSkin()->Colors.RichText.LinkHover;
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

std::pair<int, int> RichText::linkSpans(int index) const
{
    if((index < 0) || (static_cast<size_t>(index) >= m_spans.size()) || !m_spans[index].isLink())
        return { -1, -1 };

    const String &url=m_spans[index].url;
    int first=index, last=index;

    while((first > 0) && (m_spans[first-1].url == url))
        --first;
    while((static_cast<size_t>(last+1) < m_spans.size()) && (m_spans[last+1].url == url))
        ++last;
    return { first, last };
}

bool RichText::isLinkHovered(size_t index) const
{
    return (m_hoveredFirst >= 0) && (static_cast<int>(index) >= m_hoveredFirst)
        && (static_cast<int>(index) <= m_hoveredLast);
}

int RichText::linkAt(const Point &local)
{
    const Point at(local.x-contentOrigin().x, local.y-contentOrigin().y);
    const std::vector<text::RichPiece> &pieces=m_layout.pieces();

    for(const text::RichLine &line:m_layout.lines())
    {
        if((at.y < line.top) || (at.y >= line.top+line.height))
            continue;
        for(size_t i=line.firstPiece; i < line.firstPiece+line.pieceCount; ++i)
        {
            const Rect box=pieces[i].box();

            if((at.x >= box.x) && (at.x < box.x+box.w))
                return linkSpans(static_cast<int>(pieces[i].span)).first;
        }
        return -1;
    }
    return -1;
}

void RichText::setHoveredLink(int index)
{
    const std::pair<int, int> link=linkSpans(index);

    if(link.first == m_hoveredFirst)
        return;

    // the spans of both links change colour: the renderer draws their runs
    // and lines again, and no other
    m_hoveredFirst=link.first;
    m_hoveredLast=link.second;
    addStateChange(StateChange_Text);
    setCursor((link.first >= 0)?cursor::Finger:cursor::Beam);
}

void RichText::setSelectionModel(text::SelectionModel *model, std::vector<size_t> block)
{
    m_selection=model;
    m_selectionBlock=model?std::move(block):std::vector<size_t>();
    m_selecting=false;
    selectionChanged();
}

void RichText::selectionChanged()
{
    // the selection's slots are drawn again; its runs are unchanged
    addStateChange(StateChange_Background);
}

bool RichText::selectedRange(std::pair<size_t, size_t> &bytes) const
{
    return selectionModel().range(m_selectionBlock, m_layout.textLength(), bytes);
}

bool RichText::hasSelection() const
{
    std::pair<size_t, size_t> bytes;

    return selectedRange(bytes);
}

String RichText::getSelectedText() const
{
    std::pair<size_t, size_t> bytes;

    if(!selectedRange(bytes))
        return String();
    return getText().substr(bytes.first, bytes.second-bytes.first);
}

void RichText::select(size_t first, size_t last)
{
    const unsigned serial=selectionModel().serial();

    selectionModel().select(at(first), at(last));
    if(selectionModel().serial() != serial)
        selectionChanged();
}

void RichText::selectAll()
{
    select(0, getText().size());
}

void RichText::clearSelection()
{
    if(!hasSelection())
        return;

    const size_t focus=selectionModel().focus().offset;

    select(focus, focus);
}

bool RichText::copySelection()
{
    if(!hasSelection())
        return false;

    platform::setClipboardText(getSelectedText());
    return true;
}

void RichText::onCopy(event::Info)
{
    copySelection();
}

void RichText::onSelectAll(event::Info)
{
    selectAll();
}

bool RichText::onKeyEscape(bool down)
{
    if(!hasSelection())
        return false;
    if(down)
        clearSelection();
    return true;
}

size_t RichText::positionAt(const Point &local)
{
    const Point at(local.x-contentOrigin().x, local.y-contentOrigin().y);
    const std::vector<text::RichLine> &lines=m_layout.lines();
    const std::vector<text::RichPiece> &pieces=m_layout.pieces();

    if(lines.empty())
        return 0;

    size_t index=0;

    while((index+1 < lines.size()) && (at.y >= lines[index].top+lines[index].height))
        ++index;

    const text::RichLine &line=lines[index];

    if(line.pieceCount == 0)
        return line.begin;

    for(size_t i=line.firstPiece; i < line.firstPiece+line.pieceCount; ++i)
    {
        const text::RichPiece &piece=pieces[i];

        if(at.x >= piece.pos.x+piece.width)
            continue;
        if(at.x <= piece.pos.x)
            return piece.offset;

        // the boundary nearest x, its byte by walking the codepoints to it
        const int x=at.x-piece.pos.x;
        size_t caret=1;

        while((caret+1 < piece.carets.size()) && (piece.carets[caret] < x))
            ++caret;
        if(caret < piece.carets.size() && (x-piece.carets[caret-1] <= piece.carets[caret]-x))
            --caret;

        size_t byte=0;

        for(size_t c=0; (c < caret) && (byte < piece.text.size()); ++c)
            text::utf8CodepointAt(piece.text, byte, &byte);
        return piece.offset+text::caretStopAt(piece.text, byte);
    }

    const text::RichPiece &last=pieces[line.firstPiece+line.pieceCount-1];

    return last.offset+last.text.size();
}

int RichText::xAt(const text::RichLine &line, size_t offset) const
{
    const std::vector<text::RichPiece> &pieces=m_layout.pieces();

    if(line.pieceCount == 0)
        return 0;

    for(size_t i=line.firstPiece; i < line.firstPiece+line.pieceCount; ++i)
    {
        const text::RichPiece &piece=pieces[i];

        if(offset < piece.offset)
            return piece.box().x;
        if(offset > piece.offset+piece.text.size())
            continue;

        size_t caret=0;

        for(size_t byte=0; byte < offset-piece.offset; ++caret)
            text::utf8CodepointAt(piece.text, byte, &byte);
        return piece.pos.x+((caret < piece.carets.size())?piece.carets[caret]:piece.width);
    }

    const Rect box=pieces[line.firstPiece+line.pieceCount-1].box();

    return box.x+box.w;
}

std::vector<Rect> RichText::selectionRects()
{
    std::vector<Rect> rects;
    std::pair<size_t, size_t> bytes;

    if(!selectedRange(bytes))
        return rects;

    const std::vector<text::RichPiece> &pieces=m_layout.pieces();

    for(const text::RichLine &line:m_layout.lines())
    {
        if((bytes.second <= line.begin) || (bytes.first >= line.end))
            continue;

        // what the line shows ends at its last piece; past that, its break
        const size_t shown=(line.pieceCount == 0)?line.begin
            :pieces[line.firstPiece+line.pieceCount-1].offset+pieces[line.firstPiece+line.pieceCount-1].text.size();
        const int left=xAt(line, std::max(bytes.first, line.begin));
        int right=xAt(line, std::min(bytes.second, shown));

        if(bytes.second > shown)
            right=xAt(line, shown)+breakSelectionWidth(line);
        if(right > left)
            rects.emplace_back(left, line.top, right-left, line.height);
    }
    return rects;
}

void RichText::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    const Point local=canvasPosToLocal(Point(x, y));

    setHoveredLink(linkAt(local));
    if(m_selecting && (MouseFocus == this))
    {
        const unsigned serial=selectionModel().serial();

        selectionModel().extendTo(at(positionAt(local)));
        if(selectionModel().serial() != serial)
            selectionChanged();
    }
}

void RichText::onMouseLeave()
{
    setHoveredLink(-1);
    ParentClass::onMouseLeave();
}

void RichText::onMouseClickLeft(int x, int y, bool down)
{
    const Point local=canvasPosToLocal(Point(x, y));
    const int link=linkAt(local);

    if(down)
    {
        const unsigned serial=selectionModel().serial();
        const text::SelectionPosition pressed=at(positionAt(local));

        if(input::isShiftDown())
            selectionModel().extendTo(pressed);
        else
            selectionModel().setCaret(pressed);
        if(selectionModel().serial() != serial)
            selectionChanged();

        m_pressedLink=link;
        m_selecting=true;
        MouseFocus=this;
        return;
    }

    const int pressed=m_pressedLink;

    m_pressedLink=-1;
    m_selecting=false;
    if(MouseFocus == this)
        MouseFocus=nullptr;
    // a drag that selected something is no click
    if((pressed < 0) || (link != pressed) || hasSelection())
        return;

    event::Info info(this);

    info.string=m_spans[pressed].url;
    info.integer=static_cast<size_t>(pressed);
    onLinkClicked.call(this, info);
}

// The word, blank run or punctuation run under the press
void RichText::onMouseDoubleClickLeft(int x, int y)
{
    const std::pair<size_t, size_t> word=text::wordAt(getText(), positionAt(canvasPosToLocal(Point(x, y))));

    m_pressedLink=-1;
    m_selecting=false;
    select(word.first, word.second);
}

}//namespace controls
}//namespace gweni
