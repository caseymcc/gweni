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

bool HexRowMarks::operator==(const HexRowMarks &o) const
{
    if(cursor != o.cursor || selectionBegin != o.selectionBegin || selectionEnd != o.selectionEnd
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
    m_hex=newChild<internal::Text>();
    m_ascii=newChild<internal::Text>();
    for(internal::Text *text : {m_address, m_hex, m_ascii})
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
    updateMarks();
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

        const size_t selBegin=std::max(view.getSelectionBegin(), m_offset);
        const size_t selEnd=std::min(view.getSelectionEnd(), end);

        if(view.m_cursor != HexView::NoByte && selBegin < selEnd)
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

    for(internal::Text *text : {m_address, m_hex, m_ascii})
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
            for(internal::Text *text : {row->m_address, row->m_hex, row->m_ascii})
                text->setFont(getFont());
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

    m_goto=newChild<TextBox>();
    m_goto->setPlacement(Placement::Owner);
    m_goto->setPlaceholder("Go to address");
    m_goto->onReturnPressedCaller.add(this, &HexView::onGotoReturn);
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
    for(size_t i=0; i < poolSize(); ++i)
        poolRow(i)->invalidate();
    updateRows();
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
    for(size_t i=0; i < poolSize(); ++i)
        poolRow(i)->invalidate();
    invalidate();
}

void HexView::dataChanged(size_t begin, size_t end)
{
    end=std::min(end, m_size);
    if(begin >= end)
        return;

    const size_t first=begin/m_bytesPerRow;
    const size_t last=(end-1)/m_bytesPerRow;

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
        for(size_t i=0; i < poolSize(); ++i)
        {
            auto *row=static_cast<HexViewRow *>(poolRow(i));

            for(internal::Text *text : {row->m_address, row->m_hex, row->m_ascii})
            {
                text->setFont(font);
                text->textChanged();
                text->addStateChange(StateChange_Text);
                text->invalidate();
            }
            row->invalidate();
        }
    }
    // the goto box's room above the rows, for this pass
    if(dim == Dim::X)
    {
        const int top=m_goto->hidden()?0:gotoHeight()+GotoGap;

        if(getPadding().top != top)
        {
            PlacementScope placement;
            Padding padding=getPadding();

            padding.top=top;
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
    scrollCursorIntoView();
    if(!moved)
        return;

    updateMarks();
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

void HexView::onGotoReturn(event::Info)
{
    if(gotoAddress(m_goto->getText()))
        focus();
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

    if(MouseFocus != this)
        return;

    const ByteHit hit=byteNear(canvasPosToLocal(Point(x, y)));

    if(hit)
        setCursor(hit.offset, true);
}

bool HexView::onChar(gweni::UnicodeChar c)
{
    if(m_cursor == NoByte || isReadOnly() || isDisabled())
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
    if(down)
        moveBy(-1, input::isShiftDown());
    return true;
}

bool HexView::onKeyRight(bool down)
{
    if(down)
        moveBy(1, input::isShiftDown());
    return true;
}

bool HexView::onKeyUp(bool down)
{
    if(down)
        moveBy(-static_cast<int64_t>(m_bytesPerRow), input::isShiftDown());
    return true;
}

bool HexView::onKeyDown(bool down)
{
    if(down)
        moveBy(static_cast<int64_t>(m_bytesPerRow), input::isShiftDown());
    return true;
}

bool HexView::onKeyPageUp(bool down)
{
    if(down)
        moveBy(-static_cast<int64_t>(pageRows()*m_bytesPerRow), input::isShiftDown());
    return true;
}

bool HexView::onKeyPageDown(bool down)
{
    if(down)
        moveBy(static_cast<int64_t>(pageRows()*m_bytesPerRow), input::isShiftDown());
    return true;
}

bool HexView::onKeyHome(bool down)
{
    if(down && m_cursor != NoByte)
        setCursor(input::isControlDown()?0:m_cursor-m_cursor%m_bytesPerRow, input::isShiftDown());
    return true;
}

bool HexView::onKeyend(bool down)
{
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
    if(m_pendingNibble < 0)
        return false;
    if(down)
        dropPendingNibble();
    return true;
}

// The goto box sits in the view's top padding, above the rows.
void HexView::contentsPlaced(skin::Base *skin)
{
    ParentClass::contentsPlaced(skin);

    if(m_goto->hidden())
        return;

    const Padding &padding=m_goto->getPadding();
    const int width=std::min(getWidth(), cellX(20)+padding.left+padding.right);

    placeChild(m_goto, 0, 0, width, gotoHeight());
}

Size HexView::contentSize() const
{
    Size content=ParentClass::contentSize();

    content.width=getContentWidth();
    return content;
}

}//namespace controls
}//namespace gweni
