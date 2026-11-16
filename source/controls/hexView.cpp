/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */

#include <gweni/controls/hexView.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <utility>

namespace gweni
{
namespace controls
{

namespace
{

constexpr char HexDigits[]="0123456789ABCDEF";
constexpr size_t CellSample=256;

String addressText(uint64_t address, unsigned digits)
{
    String text(digits, '0');

    for(unsigned i=digits; i > 0; --i)
    {
        text[i-1]=HexDigits[address & 15];
        address >>= 4;
    }
    return text;
}

int hexDigit(gweni::UnicodeChar c)
{
    if(c >= '0' && c <= '9')
        return static_cast<int>(c-'0');
    if(c >= 'a' && c <= 'f')
        return static_cast<int>(c-'a'+10);
    if(c >= 'A' && c <= 'F')
        return static_cast<int>(c-'A'+10);
    return -1;
}

}//namespace

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(HexViewText)
{
}

void HexViewText::setColorSpans(std::vector<renderer::ColorSpan> &spans)
{
    if(spans == m_spans)
        return;

    m_spans.swap(spans);
    addStateChange(StateChange_Text);
}

GWENI_CONTROL_CONSTRUCTOR(HexViewPreview)
{
    m_endian=newChild<Button>();
    m_endian->setPlacement(Placement::Owner);
    m_endian->setIsToggle(true);
    m_endian->setText("Little endian");
    m_endian->setTabable(false);
    for(size_t i=0; i < m_names.size(); ++i)
    {
        m_names[i]=newChild<Text>();
        m_values[i]=newChild<Text>();
        m_names[i]->setText(HexView::previewName(static_cast<HexView::PreviewType>(i)));
        for(Text *text : {m_names[i], m_values[i]})
        {
            text->setPlacement(Placement::Owner);
            text->setSameSizeKeepsLayout(true);
            text->setPadding(Padding(0, 0, 0, 0));
        }
    }
}

// The toggle on the first line, then two name/value pairs a line: a name in
// NameCells, a value in ValueCells, a gap between the pairs; whole cells.
namespace
{
constexpr size_t NameCells=7;
constexpr size_t ValueCells=24;
constexpr size_t PairGap=3;
constexpr size_t ToggleCells=14;
}

void HexViewPreview::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    if(!m_view)
        return;

    placeChild(m_endian, 0, m_endian->getY(),
        m_view->cellX(ToggleCells)+2*HexView::PreviewButtonPad, m_endian->getHeight());
    for(size_t i=0; i < m_names.size(); ++i)
    {
        const size_t column=(i%2)*(NameCells+1+ValueCells+PairGap);

        placeChild(m_names[i], m_view->cellX(column), m_names[i]->getY(),
            m_view->cellX(NameCells), m_names[i]->getHeight());
        placeChild(m_values[i], m_view->cellX(column+NameCells+1), m_values[i]->getY(),
            m_view->cellX(ValueCells), m_values[i]->getHeight());
    }
}

void HexViewPreview::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    if(!m_view)
        return;

    const int line=std::max(1, m_view->m_gotoLineHeight);
    const int top=line+HexView::PreviewButtonPad+HexView::PreviewGap;

    placeChild(m_endian, m_endian->getX(), 0, m_endian->getWidth(), line+HexView::PreviewButtonPad);
    for(size_t i=0; i < m_names.size(); ++i)
    {
        const int y=top+static_cast<int>(i/2)*line;

        placeChild(m_names[i], m_names[i]->getX(), y, m_names[i]->getWidth(), line);
        placeChild(m_values[i], m_values[i]->getX(), y, m_values[i]->getWidth(), line);
    }
}

GWENI_CONTROL_CONSTRUCTOR(HexViewGotoBox)
{
}

// On the release, as a TextBox commits: the release then finds the key up
// and goes nowhere. onReturnPressed still fires, after the goto.
bool HexViewGotoBox::onKeyReturn(bool down)
{
    if(down)
        return true;

    if(m_view && m_view->gotoAddress(getText()))
        m_view->focus();
    else
        onSelectAll(this);

    onEnter();
    return true;
}

}//namespace internal

bool HexRowMarks::operator==(const HexRowMarks &o) const
{
    if(cursor != o.cursor || hover != o.hover || selectionBegin != o.selectionBegin || selectionEnd != o.selectionEnd
        || highlightCount != o.highlightCount)
        return false;
    // where the box is only matters for a row holding the cursor
    if(cursor >= 0 && (cursorInAscii != o.cursorInAscii || pendingNibble != o.pendingNibble))
        return false;
    for(size_t i=0; i < highlightCount; ++i)
    {
        if(!(highlights[i] == o.highlights[i]))
            return false;
    }
    return true;
}

GWENI_CONTROL_CONSTRUCTOR(HexViewRow)
{
    setMouseInputEnabled(false);
    m_address=newChild<internal::Text>();
    m_hex=newChild<internal::HexViewText>();
    m_ascii=newChild<internal::HexViewText>();
    for(internal::Text *text : std::initializer_list<internal::Text *>{m_address, m_hex, m_ascii})
    {
        text->setPlacement(Placement::Owner);
        text->setSameSizeKeepsLayout(true);
        text->setPadding(Padding(0, 0, 0, 0));
    }
}

void HexViewRow::bind(size_t offset)
{
    m_offset=offset;
    m_count=std::min(m_view->m_bytesPerRow, m_view->m_size-offset);
    m_view->m_read(offset, m_bytes.data(), m_count);

    String hex;
    String ascii;

    hex.reserve(m_view->hexCells());
    ascii.reserve(m_count);
    for(size_t i=0; i < m_count; ++i)
    {
        if(i > 0)
        {
            hex+=' ';
            if(i%8 == 0)
                hex+=' ';
        }
        const uint8_t byte=m_bytes[i];

        // a half-typed byte shows its typed (high) nibble
        const bool pending=m_view->m_pendingNibble >= 0 && offset+i == m_view->m_cursor;

        hex+=HexDigits[pending?m_view->m_pendingNibble:(byte >> 4)];
        hex+=HexDigits[byte & 15];
        ascii+=(byte >= 0x20 && byte <= 0x7e)?static_cast<char>(byte):'.';
    }

    const String address=addressText(m_view->m_baseAddress+offset, m_view->m_addressDigits);

    // Kept strings do not mark their Text, even when a row was rebound.
    if(m_address->getText() != address)
        m_address->setText(address);
    if(m_hex->getText() != hex)
        m_hex->setText(hex);
    if(m_ascii->getText() != ascii)
        m_ascii->setText(ascii);
    updateSpans();
    updateMarks();
}

// The zero bytes' digits and the non-printable bytes' dots in their colours,
// each run of them one span; the rest in the run's own colour.
void HexViewRow::updateSpans()
{
    skin::Base *skin=getSkin();

    if(!skin || m_offset == VirtualList::NoItem)
        return;

    const auto &colors=skin->Colors.HexView;
    std::vector<renderer::ColorSpan> hex;
    std::vector<renderer::ColorSpan> ascii;

    for(size_t i=0; i < m_count; ++i)
    {
        const uint8_t byte=m_bytes[i];

        if(byte == 0 && !(m_view->m_pendingNibble >= 0 && m_offset+i == m_view->m_cursor))
        {
            const size_t begin=3*i+i/8;

            if(!hex.empty() && hex.back().end+1+(i%8 == 0) == begin)
                hex.back().end=begin+2;
            else
                hex.push_back({begin, begin+2, colors.ZeroByte});
        }
        if(byte < 0x20 || byte > 0x7e)
        {
            if(!ascii.empty() && ascii.back().end == i)
                ascii.back().end=i+1;
            else
                ascii.push_back({i, i+1, colors.NonPrintable});
        }
    }
    m_hex->setColorSpans(hex);
    m_ascii->setColorSpans(ascii);
}

void HexViewRow::updateColors()
{
    ParentClass::updateColors();

    skin::Base *skin=getSkin();

    if(!skin)
        return;

    const auto &colors=skin->Colors.HexView;

    m_address->setTextColor(colors.Address);
    m_hex->setTextColor(colors.Byte);
    m_ascii->setTextColor(colors.Ascii);
    updateSpans();
}

// The row's part of the cursor, the selection and the highlights, compared
// with what it last drew: only a row whose own marks moved is marked.
void HexViewRow::updateMarks()
{
    HexRowMarks marks;
    const HexView &view=*m_view;

    if(m_offset != VirtualList::NoItem && m_count > 0)
    {
        const size_t end=m_offset+m_count;

        if(view.m_cursor != HexView::NoByte && view.m_cursor >= m_offset && view.m_cursor < end)
        {
            marks.cursor=static_cast<int>(view.m_cursor-m_offset);
            marks.cursorInAscii=view.m_cursorColumn == HexView::Column::Ascii;
            marks.pendingNibble=view.m_pendingNibble >= 0;
        }
        if(view.m_hover != HexView::NoByte && view.m_hover >= m_offset && view.m_hover < end)
            marks.hover=static_cast<int>(view.m_hover-m_offset);

        const size_t selBegin=std::max(view.getSelectionBegin(), m_offset);
        const size_t selEnd=std::min(view.getSelectionEnd(), end);

        // a one-byte selection is the cursor's byte, which its box shows: no
        // piece, so a plain cursor move writes the cursor's slots alone
        if(view.m_cursor != HexView::NoByte && selBegin < selEnd
            && view.getSelectionEnd()-view.getSelectionBegin() > 1)
        {
            marks.selectionBegin=static_cast<uint8_t>(selBegin-m_offset);
            marks.selectionEnd=static_cast<uint8_t>(selEnd-m_offset);
        }

        for(const HexView::Highlight &highlight : view.m_highlights)
        {
            if(marks.highlightCount == HexRowMarks::MaxHighlights)
                break;

            const size_t begin=std::max(highlight.begin, m_offset);
            const size_t stop=std::min(highlight.end, end);

            if(begin >= stop)
                continue;

            HexRowMarks::Piece &piece=marks.highlights[marks.highlightCount++];

            piece.begin=static_cast<uint8_t>(begin-m_offset);
            piece.end=static_cast<uint8_t>(stop-m_offset);
            piece.surface=static_cast<uint8_t>(highlight.surface%4);
        }
    }

    if(marks != m_marks)
    {
        m_marks=marks;
        addStateChange(StateChange_Background);
    }
}

void HexViewRow::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    if(!m_view)
        return;

    placeChild(m_address, m_view->getAddressX(), m_address->getY(),
        m_view->cellX(m_view->m_addressDigits), m_address->getHeight());
    placeChild(m_hex, m_view->getHexX(), m_hex->getY(),
        m_view->cellX(m_view->hexCells()), m_hex->getHeight());
    placeChild(m_ascii, m_view->getAsciiX(), m_ascii->getY(),
        m_view->cellX(m_view->m_bytesPerRow), m_ascii->getHeight());
}

void HexViewRow::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    const int height=std::max(1, getHeight()-2*HexView::RowPadY);

    for(internal::Text *text : std::initializer_list<internal::Text *>{m_address, m_hex, m_ascii})
        placeChild(text, text->getX(), HexView::RowPadY, text->getWidth(), height);
}

GWENI_CONTROL_CONSTRUCTOR(HexView)
{
    setScroll(true, true);
    m_innerPanel->setMouseInputEnabled(false);
    setRowFactory([this](Base *parent) -> Base *
        {
            HexViewRow *row=parent->newChild<HexViewRow>();

            row->m_view=this;
            for(internal::Text *text : std::initializer_list<internal::Text *>{row->m_address, row->m_hex, row->m_ascii})
                text->setFont(getFont());
            row->updateColors();
            return row;
        });
    setRowBinder([](Base *row, size_t item)
        {
            auto *hexRow=static_cast<HexViewRow *>(row);

            hexRow->bind(item*hexRow->m_view->m_bytesPerRow);
        });

    setMouseInputEnabled(true);
    setKeyboardInputEnabled(true);
    setTabable(true);
    addAccelerator("Ctrl + C", &HexView::onCopy);

    // the goto box and the preview are the view's own children, beside the
    // inner panel (which a new child would otherwise go into, and scroll and
    // clip to the viewport with the rows)
    Base *inner=m_innerPanel;

    m_innerPanel=nullptr;
    {
        PartScope part;

        m_goto=newChild<internal::HexViewGotoBox>();
        m_preview=newChild<internal::HexViewPreview>();
    }
    m_innerPanel=inner;
    m_goto->setPlacement(Placement::Owner);
    m_goto->setPlaceholder("Go to address");
    m_goto->m_view=this;

    m_preview->m_view=this;
    m_preview->setPlacement(Placement::Owner);
    m_preview->getEndianButton()->onToggleCaller.add(this, &HexView::onEndianToggled);
    updatePreview();
}

const Font &HexView::getFont() const
{
    return const_cast<HexView *>(this)->getSkin()->getDefaultMonoFont();
}

void HexView::setSource(size_t size, Read read, Write write)
{
    m_read=std::move(read);
    m_write=std::move(write);
    m_size=m_read?size:0;
    m_pendingNibble=-1;
    // the cursor and the anchor kept where the new data still has them
    if(m_size == 0)
        m_cursor=m_anchor=NoByte;
    else
    {
        m_cursor=(m_cursor == NoByte)?0:std::min(m_cursor, m_size-1);
        m_anchor=(m_anchor == NoByte)?m_cursor:std::min(m_anchor, m_size-1);
    }
    // the highlights are clipped to the data as they are drawn
    updateRows();
    updatePreview();
}

void HexView::setBaseAddress(uint64_t address)
{
    if(m_baseAddress == address)
        return;

    m_baseAddress=address;
    updateAddressDigits();
    refresh();
}

void HexView::setBytesPerRow(size_t bytes)
{
    if((bytes != 8 && bytes != 16 && bytes != 32) || bytes == m_bytesPerRow)
        return;

    m_bytesPerRow=bytes;
    columnsMoved();
    updateRows();
}

// A row's boxes are drawn at the columns' x, which can move while its bounds
// stay (a view wider than its content): every pooled row is marked, as
// CodeEditor::readMetrics marks its rows for a new cell width.
void HexView::columnsMoved()
{
    for(size_t i=0; i < poolSize(); ++i)
    {
        poolRow(i)->invalidate();
        poolRow(i)->addStateChange(StateChange_Background);
    }
}

void HexView::updateRows()
{
    updateMarks();
    updateAddressDigits();
    // No addition of bytesPerRow-1: size may be SIZE_MAX.
    setItemCount(m_size/m_bytesPerRow+(m_size%m_bytesPerRow != 0));
    refresh();
}

void HexView::updateAddressDigits()
{
    uint64_t last=m_baseAddress;

    if(m_size > 0)
    {
        const uint64_t span=static_cast<uint64_t>(m_size-1);

        // A range crossing UINT64_MAX needs every address digit.
        last=span > std::numeric_limits<uint64_t>::max()-last
            ?std::numeric_limits<uint64_t>::max():last+span;
    }
    unsigned digits=1;

    while((last >>= 4) != 0)
        ++digits;

    if(m_addressDigits == digits)
        return;

    m_addressDigits=digits;
    // The address width moves both following columns of every pooled row.
    columnsMoved();
    invalidate();
}

void HexView::dataChanged(size_t begin, size_t end)
{
    end=std::min(end, m_size);
    if(begin >= end)
        return;

    const size_t first=begin/m_bytesPerRow;
    const size_t last=(end-1)/m_bytesPerRow;

    // the preview reads up to 8 bytes from the cursor
    if(m_cursor != NoByte && begin < m_cursor+8 && end > m_cursor)
        updatePreview();

    for(size_t i=0; i < poolSize(); ++i)
    {
        const size_t item=poolItem(i);

        if(item != NoItem && item >= first && item <= last)
            refreshItem(item);
    }
}

size_t HexView::hexCells() const
{
    return 3*m_bytesPerRow-1+(m_bytesPerRow-1)/8;
}

int HexView::cellX(size_t cells) const
{
    return static_cast<int>(std::floor(static_cast<double>(cells)*m_cellWidth));
}

int HexView::getAddressX() const { return cellX(1); }
int HexView::getHexX() const { return cellX(1+m_addressDigits+2); }
int HexView::getAsciiX() const { return cellX(1+m_addressDigits+2+hexCells()+2); }
int HexView::getContentWidth() const { return getAsciiX()+cellX(m_bytesPerRow+1); }

int HexView::byteX(size_t byte, Column column) const
{
    return column == Column::Hex?getHexX()+cellX(3*byte+byte/8):getAsciiX()+cellX(byte);
}

HexView::ByteHit HexView::hitTestByte(Point local) const
{
    const Rect &viewport=getViewport();

    if(m_cellWidth <= 0.f || m_size == 0 || local.x < viewport.x || local.y < viewport.y
        || local.x >= viewport.x+viewport.w || local.y >= viewport.y+viewport.h)
        return {};

    const int x=local.x-viewport.x+getHorizontalScrollOffset();
    const size_t item=static_cast<size_t>((static_cast<int64_t>(local.y-viewport.y)
        +getVerticalScrollOffset())/getRowHeight());
    const size_t offset=item*m_bytesPerRow;
    size_t byte=0;
    Column column;

    // Inverts floor(cell*width) exactly at integer pixel coordinates, including
    // fractional cell advances. Both text placement and hits use those edges.
    const auto cellAt=[this](int position)
        {
            return static_cast<size_t>(std::ceil((position+1)/static_cast<double>(m_cellWidth))-1);
        };

    if(x >= getHexX() && x < getHexX()+cellX(hexCells()))
    {
        const size_t cell=cellAt(x-getHexX());
        const size_t inGroup=cell%25;

        if(inGroup >= 24 || inGroup%3 == 2)
            return {};
        byte=(cell/25)*8+inGroup/3;
        column=Column::Hex;
    }
    else if(x >= getAsciiX() && x < getAsciiX()+cellX(m_bytesPerRow))
    {
        byte=cellAt(x-getAsciiX());
        column=Column::Ascii;
    }
    else
        return {};

    if(byte >= m_bytesPerRow || offset >= m_size || byte >= m_size-offset)
        return {};
    return {offset+byte, column};
}

HexViewRow *HexView::rowForOffset(size_t offset) const
{
    return offset < m_size?static_cast<HexViewRow *>(rowForItem(offset/m_bytesPerRow)):nullptr;
}

void HexView::calculateSize(skin::Base *skin, Dim dim)
{
    const renderer::Base *renderer=skin?skin->getRenderer():nullptr;
    const Font &font=getFont();
    const unsigned serial=renderer?renderer->textLayoutSerial():0;

    if(dim == Dim::X && (m_cellWidth <= 0.f || font != m_metricsFont
        || renderer != m_metricsRenderer || serial != m_metricsSerial))
    {
        PlacementScope placement;

        m_metricsFont=font;
        m_metricsRenderer=renderer;
        m_metricsSerial=serial;
        m_cellWidth=std::max(1, measureText(font, String(CellSample, '0')).x)
            /static_cast<float>(CellSample);
        m_gotoLineHeight=std::max(1, measureLineHeight(font));
        setRowHeight(m_gotoLineHeight+2*RowPadY);
        m_goto->setFont(font);
        for(size_t i=0; i < PreviewTypes; ++i)
        {
            for(internal::Text *text : {m_preview->m_names[i], m_preview->m_values[i]})
            {
                text->setFont(font);
                text->invalidate();
            }
        }
        m_preview->invalidate();
        for(size_t i=0; i < poolSize(); ++i)
        {
            auto *row=static_cast<HexViewRow *>(poolRow(i));

            for(internal::Text *text : std::initializer_list<internal::Text *>{row->m_address, row->m_hex, row->m_ascii})
            {
                text->setFont(font);
                text->textChanged();
                text->addStateChange(StateChange_Text);
                text->invalidate();
            }
        }
        columnsMoved();
    }
    // the goto box's room above the rows, for this pass
    if(dim == Dim::X)
    {
        const int top=m_goto->hidden()?0:gotoHeight()+2*GotoGap;
        const int bottom=m_preview->hidden()?0:previewHeight()+2*PreviewGap;

        if(getPadding().top != top || getPadding().bottom != bottom)
        {
            PlacementScope placement;
            Padding padding=getPadding();

            padding.top=top;
            padding.bottom=bottom;
            setPadding(padding);
        }
    }
    ParentClass::calculateSize(skin, dim);
}

void HexView::updateMarks()
{
    for(size_t i=0; i < poolSize(); ++i)
        static_cast<HexViewRow *>(poolRow(i))->updateMarks();
}

void HexView::setCursor(size_t offset, bool extend)
{
    if(m_size == 0)
        return;

    offset=std::min(offset, m_size-1);
    const size_t anchor=extend?m_anchor:offset;

    if(m_pendingNibble >= 0)
        dropPendingNibble();

    const bool moved=offset != m_cursor || anchor != m_anchor;

    m_cursor=offset;
    m_anchor=anchor;
    // a drag inside one byte, a key repeated at a limit: no scroll, so no
    // layout, unless the cursor was scrolled out of view
    if(!moved)
    {
        if(!cursorInView())
            scrollCursorIntoView();
        return;
    }
    scrollCursorIntoView();

    updateMarks();
    updatePreview();
    onCursorChanged.call(this);
}

void HexView::setCursorColumn(Column column)
{
    if(m_cursorColumn == column)
        return;

    dropPendingNibble();
    m_cursorColumn=column;
    updateMarks();
}

void HexView::selectRange(size_t begin, size_t end)
{
    end=std::min(end, m_size);
    if(begin >= end)
        return;

    setCursor(begin);
    setCursor(end-1, true);
}

String HexView::getSelectedHex() const
{
    if(m_cursor == NoByte)
        return String();

    const size_t begin=getSelectionBegin();
    const size_t end=getSelectionEnd();
    String text;
    uint8_t buffer[32];

    // a row's worth a read, as the rows read
    text.reserve(3*(end-begin));
    for(size_t at=begin; at < end;)
    {
        const size_t count=std::min(m_bytesPerRow, end-at);

        m_read(at, buffer, count);
        for(size_t i=0; i < count; ++i)
        {
            if(!text.empty())
                text+=' ';
            text+=HexDigits[buffer[i] >> 4];
            text+=HexDigits[buffer[i] & 15];
        }
        at+=count;
    }
    return text;
}

void HexView::copy()
{
    if(m_cursor != NoByte)
        platform::setClipboardText(getSelectedHex());
}

void HexView::onCopy(event::Info)
{
    copy();
}

size_t HexView::pageRows() const
{
    const int rowHeight=getRowHeight();

    if(rowHeight <= 0)
        return 1;
    return static_cast<size_t>(std::max(1, getViewportHeight()/rowHeight));
}

bool HexView::parseAddress(const String &text, uint64_t &address)
{
    size_t begin=0;
    size_t end=text.size();

    while(begin < end && (text[begin] == ' ' || text[begin] == '\t'))
        ++begin;
    while(end > begin && (text[end-1] == ' ' || text[end-1] == '\t'))
        --end;

    bool hex=false;

    if(end-begin > 2 && text[begin] == '0' && (text[begin+1] == 'x' || text[begin+1] == 'X'))
    {
        hex=true;
        begin+=2;
    }
    else if(end-begin > 1 && text[begin] == '$')
    {
        hex=true;
        begin+=1;
    }

    if(begin == end)
        return false;

    for(size_t i=begin; i < end; ++i)
    {
        const int digit=hexDigit(static_cast<unsigned char>(text[i]));

        if(digit < 0)
            return false;
        if(digit > 9)
            hex=true;
    }

    const uint64_t radix=hex?16:10;
    uint64_t value=0;

    for(size_t i=begin; i < end; ++i)
    {
        const uint64_t digit=static_cast<uint64_t>(hexDigit(static_cast<unsigned char>(text[i])));

        if(value > (std::numeric_limits<uint64_t>::max()-digit)/radix)
            return false;
        value=value*radix+digit;
    }
    address=value;
    return true;
}

bool HexView::gotoAddress(const String &text)
{
    uint64_t address=0;

    if(m_size == 0 || !parseAddress(text, address) || address < m_baseAddress)
        return false;

    const uint64_t offset=address-m_baseAddress;

    if(offset >= static_cast<uint64_t>(m_size))
        return false;

    setCursor(static_cast<size_t>(offset));
    return true;
}

void HexView::setGotoVisible(bool visible)
{
    if(visible == !m_goto->hidden())
        return;

    m_goto->setHidden(!visible);
    invalidate();
}

int HexView::gotoHeight() const
{
    if(m_goto->hidden())
        return 0;

    const Padding &padding=m_goto->getPadding();

    return m_gotoLineHeight+padding.top+padding.bottom;
}

void HexView::addHighlight(size_t begin, size_t end, unsigned surface)
{
    if(begin >= end)
        return;

    m_highlights.push_back({begin, end, surface%4});
    updateMarks();
}

void HexView::clearHighlights()
{
    if(m_highlights.empty())
        return;

    m_highlights.clear();
    updateMarks();
}

void HexView::moveBy(int64_t bytes, bool extend)
{
    if(m_cursor == NoByte)
        return;

    size_t target;

    if(bytes < 0)
        target=(static_cast<uint64_t>(-bytes) > m_cursor)?0:m_cursor-static_cast<size_t>(-bytes);
    else
        target=(static_cast<uint64_t>(bytes) >= m_size-m_cursor)?m_size-1:m_cursor+static_cast<size_t>(bytes);
    setCursor(target, extend);
}

// The cursor's row wholly in the viewport and its byte's cells too, with no
// scroll pending.
bool HexView::cursorInView()
{
    if(m_cursor == NoByte)
        return true;

    takeBarScroll();

    const Rect &viewport=getViewport();
    const int rowHeight=getRowHeight();

    if(getPendingItem() != NoItem || viewport.w <= 0 || viewport.h <= 0 || rowHeight <= 0 || m_cellWidth <= 0.f)
        return false;

    const int64_t top=static_cast<int64_t>(m_cursor/m_bytesPerRow)*rowHeight;
    const int64_t offset=getVerticalScrollOffset();

    if(top < offset || top+rowHeight > offset+viewport.h)
        return false;

    const size_t byte=m_cursor%m_bytesPerRow;
    const int left=byteX(byte, m_cursorColumn);
    const int right=(m_cursorColumn == Column::Hex)?getHexX()+cellX(3*byte+byte/8+2):getAsciiX()+cellX(byte+1);
    const int horizontal=getHorizontalScrollOffset();

    return left >= horizontal && right <= horizontal+viewport.w;
}

// Vertically as little as puts the cursor's row in view (by the next layout),
// horizontally as little as puts its byte's cells in the viewport.
void HexView::scrollCursorIntoView()
{
    if(m_cursor == NoByte)
        return;

    scrollToItem(m_cursor/m_bytesPerRow);

    const Rect &viewport=getViewport();

    if(viewport.w <= 0 || m_cellWidth <= 0.f)
        return;

    const size_t byte=m_cursor%m_bytesPerRow;
    const int left=byteX(byte, m_cursorColumn);
    const int right=(m_cursorColumn == Column::Hex)?getHexX()+cellX(3*byte+byte/8+2):getAsciiX()+cellX(byte+1);
    const int offset=getHorizontalScrollOffset();

    if(left < offset)
        setHorizontalScrollOffset(left);
    else if(right > offset+viewport.w)
        setHorizontalScrollOffset(std::min(right-viewport.w, getMaxHorizontalScrollOffset()));
}

void HexView::dropPendingNibble()
{
    if(m_pendingNibble < 0)
        return;

    m_pendingNibble=-1;
    if(m_cursor != NoByte)
        refreshItem(m_cursor/m_bytesPerRow);
    updateMarks();
}

void HexView::writeByte(size_t offset, uint8_t value)
{
    uint8_t old=0;

    m_read(offset, &old, 1);
    m_write(offset, value);
    m_lastEdit={offset, old, value};
    dataChanged(offset, offset+1);

    event::Info info(this);

    info.integer=offset;
    info.point=Point(old, value);
    onEdited.call(this, info);
}

// The byte under the point, or for a point off the bytes (a gap, a margin,
// above or below the rows) the nearest one in the column it is nearer: a drag
// keeps extending the selection wherever the mouse goes.
HexView::ByteHit HexView::byteNear(Point local) const
{
    if(m_size == 0 || m_cellWidth <= 0.f)
        return {};

    const Rect &viewport=getViewport();
    const int x=local.x-viewport.x+getHorizontalScrollOffset();
    const int64_t y=static_cast<int64_t>(local.y-viewport.y)+getVerticalScrollOffset();
    const size_t rows=getItemCount();
    const size_t item=(y < 0)?0:std::min(static_cast<size_t>(y/getRowHeight()), rows-1);
    const int hexEnd=getHexX()+cellX(hexCells());
    const Column column=(x >= (hexEnd+getAsciiX())/2)?Column::Ascii:Column::Hex;
    const size_t count=std::min(m_bytesPerRow, m_size-item*m_bytesPerRow);
    size_t byte=0;

    if(column == Column::Hex)
    {
        // the byte whose cells' middle is nearest, by the group's start
        for(size_t i=1; i < count; ++i)
        {
            if(x >= (byteX(i-1, column)+cellX(2)+byteX(i, column))/2)
                byte=i;
        }
    }
    else
    {
        for(size_t i=1; i < count; ++i)
        {
            if(x >= byteX(i, column))
                byte=i;
        }
    }
    return {item*m_bytesPerRow+byte, column};
}

void HexView::onMouseClickLeft(int x, int y, bool down)
{
    if(down)
    {
        const ByteHit hit=hitTestByte(canvasPosToLocal(Point(x, y)));

        if(!hit)
            return;
        setCursorColumn(hit.column);
        setCursor(hit.offset, input::isShiftDown());
        MouseFocus=this;
    }
    else if(MouseFocus == this)
        MouseFocus=nullptr;
}

void HexView::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    ParentClass::onMouseMoved(x, y, deltaX, deltaY);

    m_hoverPoint=canvasPosToLocal(Point(x, y));
    m_hoverKnown=true;
    updateHover(m_hoverPoint);

    if(MouseFocus != this)
        return;

    const ByteHit hit=byteNear(m_hoverPoint);

    if(hit)
        setCursor(hit.offset, true);
}

bool HexView::onChar(gweni::UnicodeChar c)
{
    if(m_cursor == NoByte || isReadOnly() || isDisabled() || KeyboardFocus != this)
        return false;

    if(m_cursorColumn == Column::Ascii)
    {
        if(c < 0x20 || c > 0x7e)
            return false;
        writeByte(m_cursor, static_cast<uint8_t>(c));
        if(m_cursor+1 < m_size)
            setCursor(m_cursor+1);
        return true;
    }

    const int digit=hexDigit(c);

    if(digit < 0)
        return false;

    if(m_pendingNibble < 0)
    {
        m_pendingNibble=digit;
        refreshItem(m_cursor/m_bytesPerRow);
        updateMarks();
        return true;
    }

    const uint8_t value=static_cast<uint8_t>((m_pendingNibble << 4) | digit);

    m_pendingNibble=-1;
    writeByte(m_cursor, value);
    if(m_cursor+1 < m_size)
        setCursor(m_cursor+1);
    else
        updateMarks();
    return true;
}

bool HexView::onKeyLeft(bool down)
{
    if(KeyboardFocus != this)
        return false;
    if(down)
        moveBy(-1, input::isShiftDown());
    return true;
}

bool HexView::onKeyRight(bool down)
{
    if(KeyboardFocus != this)
        return false;
    if(down)
        moveBy(1, input::isShiftDown());
    return true;
}

bool HexView::onKeyUp(bool down)
{
    if(KeyboardFocus != this)
        return false;
    if(down)
        moveBy(-static_cast<int64_t>(m_bytesPerRow), input::isShiftDown());
    return true;
}

bool HexView::onKeyDown(bool down)
{
    if(KeyboardFocus != this)
        return false;
    if(down)
        moveBy(static_cast<int64_t>(m_bytesPerRow), input::isShiftDown());
    return true;
}

bool HexView::onKeyPageUp(bool down)
{
    if(KeyboardFocus != this)
        return false;
    if(down)
        moveBy(-static_cast<int64_t>(pageRows()*m_bytesPerRow), input::isShiftDown());
    return true;
}

bool HexView::onKeyPageDown(bool down)
{
    if(KeyboardFocus != this)
        return false;
    if(down)
        moveBy(static_cast<int64_t>(pageRows()*m_bytesPerRow), input::isShiftDown());
    return true;
}

bool HexView::onKeyHome(bool down)
{
    if(KeyboardFocus != this)
        return false;
    if(down && m_cursor != NoByte)
        setCursor(input::isControlDown()?0:m_cursor-m_cursor%m_bytesPerRow, input::isShiftDown());
    return true;
}

bool HexView::onKeyend(bool down)
{
    if(KeyboardFocus != this)
        return false;
    if(down && m_cursor != NoByte)
    {
        const size_t rowEnd=std::min(m_cursor-m_cursor%m_bytesPerRow+m_bytesPerRow, m_size)-1;

        setCursor(input::isControlDown()?m_size-1:rowEnd, input::isShiftDown());
    }
    return true;
}

// Only a half-typed byte takes Escape; otherwise it goes on up (a dialog).
bool HexView::onKeyEscape(bool down)
{
    if(m_pendingNibble < 0 || KeyboardFocus != this)
        return false;
    if(down)
        dropPendingNibble();
    return true;
}

// A half-typed byte is not kept for a digit typed after the focus comes back.
void HexView::onLostKeyboardFocus()
{
    ParentClass::onLostKeyboardFocus();
    dropPendingNibble();
}

// The goto box sits in the view's top padding, above the rows.
void HexView::contentsPlaced(skin::Base *skin)
{
    ParentClass::contentsPlaced(skin);

    // a gap in from the frame, the preview a cell in, as the address column
    if(!m_preview->hidden())
        placeChild(m_preview, getAddressX(), getHeight()-PreviewGap-previewHeight(),
            std::max(0, getWidth()-2*getAddressX()), previewHeight());

    // the rows moved under a resting mouse: the byte under it now
    if(m_hoverKnown)
        updateHover(m_hoverPoint);

    if(m_goto->hidden())
        return;

    const Padding &padding=m_goto->getPadding();
    const int width=std::min(std::max(0, getWidth()-2*getAddressX()), cellX(20)+padding.left+padding.right);

    placeChild(m_goto, getAddressX(), GotoGap, width, gotoHeight());
}

void HexView::onMouseLeave()
{
    ParentClass::onMouseLeave();
    m_hoverKnown=false;
    setHover(NoByte);
}

void HexView::updateHover(Point local)
{
    const ByteHit hit=hitTestByte(local);

    setHover(hit?hit.offset:NoByte);
}

void HexView::setHover(size_t byte)
{
    if(m_hover == byte)
        return;

    m_hover=byte;
    updateMarks();
}

void HexView::setPreviewVisible(bool visible)
{
    if(visible == !m_preview->hidden())
        return;

    m_preview->setHidden(!visible);
    updatePreview();
    invalidate();
}

int HexView::previewHeight() const
{
    if(m_preview->hidden())
        return 0;

    const int line=std::max(1, m_gotoLineHeight);

    return line+PreviewButtonPad+PreviewGap+PreviewLines*line;
}

void HexView::setBigEndian(bool big)
{
    if(m_bigEndian == big)
        return;

    m_bigEndian=big;
    if(m_preview->getEndianButton()->getToggleState() != big)
        m_preview->getEndianButton()->setToggleState(big);
    m_preview->getEndianButton()->setText(big?"Big endian":"Little endian");
    updatePreview();
}

void HexView::onEndianToggled(event::Info)
{
    setBigEndian(m_preview->getEndianButton()->getToggleState());
}

const char *HexView::previewName(PreviewType type)
{
    static const char *const names[PreviewTypes]={
        "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64", "float", "double"};

    return names[static_cast<size_t>(type)];
}

size_t HexView::previewSize(PreviewType type)
{
    static const size_t sizes[PreviewTypes]={1, 1, 2, 2, 4, 4, 8, 8, 4, 8};

    return sizes[static_cast<size_t>(type)];
}

String HexView::previewValue(PreviewType type, const uint8_t *bytes, size_t available, bool bigEndian)
{
    const size_t size=previewSize(type);

    if(available < size)
        return "n/a";

    uint64_t bits=0;

    for(size_t i=0; i < size; ++i)
    {
        const uint8_t byte=bigEndian?bytes[i]:bytes[size-1-i];

        bits=(bits << 8) | byte;
    }

    char text[40];

    switch(type)
    {
    case PreviewType::Int8:
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(static_cast<int8_t>(bits)));
        break;
    case PreviewType::Int16:
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(static_cast<int16_t>(bits)));
        break;
    case PreviewType::Int32:
        std::snprintf(text, sizeof(text), "%ld", static_cast<long>(static_cast<int32_t>(bits)));
        break;
    case PreviewType::Int64:
        std::snprintf(text, sizeof(text), "%lld", static_cast<long long>(static_cast<int64_t>(bits)));
        break;
    case PreviewType::UInt8:
    case PreviewType::UInt16:
    case PreviewType::UInt32:
    case PreviewType::UInt64:
        std::snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(bits));
        break;
    case PreviewType::Float:
    {
        const uint32_t word=static_cast<uint32_t>(bits);
        float value;

        std::memcpy(&value, &word, sizeof(value));
        std::snprintf(text, sizeof(text), "%.9g", static_cast<double>(value));
        break;
    }
    case PreviewType::Double:
    {
        double value;

        std::memcpy(&value, &bits, sizeof(value));
        std::snprintf(text, sizeof(text), "%.17g", value);
        break;
    }
    case PreviewType::Count:
        return String();
    }
    return text;
}

// Each value read again, its Text set only when its string changed.
void HexView::updatePreview()
{
    // read again as it is shown (setPreviewVisible)
    if(m_preview->hidden())
        return;

    uint8_t bytes[8]={};
    size_t available=0;

    if(m_cursor != NoByte && m_read)
    {
        available=std::min<size_t>(8, m_size-m_cursor);
        m_read(m_cursor, bytes, available);
    }

    for(size_t i=0; i < PreviewTypes; ++i)
    {
        const String value=previewValue(static_cast<PreviewType>(i), bytes, available, m_bigEndian);
        internal::Text *text=m_preview->m_values[i];

        if(text->getText() != value)
            text->setText(value);
    }
}

const String &HexView::getPreviewText(PreviewType type) const
{
    return m_preview->m_values[static_cast<size_t>(type)]->getText();
}

void HexView::updateColors()
{
    ParentClass::updateColors();

    skin::Base *skin=getSkin();

    if(!skin)
        return;

    for(size_t i=0; i < PreviewTypes; ++i)
    {
        m_preview->m_names[i]->setTextColor(skin->Colors.HexView.PreviewName);
        m_preview->m_values[i]->setTextColor(skin->Colors.HexView.Preview);
    }
}

void HexView::byteSpan(size_t begin, size_t end, Column column, int &x, int &width) const
{
    if(column == Column::Hex)
    {
        const size_t last=end-1;

        x=byteX(begin, column);
        width=getHexX()+cellX(3*last+last/8+2)-x;
    }
    else
    {
        x=byteX(begin, column);
        width=getAsciiX()+cellX(end)-x;
    }
}

void HexView::cursorSpan(size_t byte, Column column, bool pending, int &x, int &width) const
{
    if(column == Column::Hex && pending)
    {
        const size_t cell=3*byte+byte/8;

        x=getHexX()+cellX(cell+1);
        width=getHexX()+cellX(cell+2)-x;
        return;
    }
    byteSpan(byte, byte+1, column, x, width);
}

Size HexView::contentSize() const
{
    Size content=ParentClass::contentSize();

    content.width=getContentWidth();
    return content;
}

}//namespace controls
}//namespace gweni
