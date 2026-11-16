/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */

#include <gweni/controls/hexView.h>
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

}//namespace

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

        hex+=HexDigits[byte >> 4];
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
        setRowHeight(std::max(1, measureLineHeight(font))+2*RowPadY);
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
    ParentClass::calculateSize(skin, dim);
}

Size HexView::contentSize() const
{
    Size content=ParentClass::contentSize();

    content.width=getContentWidth();
    return content;
}

}//namespace controls
}//namespace gweni
