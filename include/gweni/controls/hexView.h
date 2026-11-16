/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */

#ifndef _gweni_controls_hexview_h_
#define _gweni_controls_hexview_h_

#include <gweni/controls/text.h>
#include <gweni/controls/virtualList.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace gweni
{
namespace controls
{

class HexView;

//! A pooled HexView row. The view binds it to a byte offset, and places its
//! three mono Text runs in fixed columns. An app does not construct rows.
class GWENI_EXPORT HexViewRow: public Base
{
public:
    GWENI_CONTROL(HexViewRow, Base);

public:
    GWENI_DRAWS_NOTHING

    size_t getOffset() const { return m_offset; }
    size_t getByteCount() const { return m_count; }
    const String &getAddressText() const { return m_address->getText(); }
    const String &getHexText() const { return m_hex->getText(); }
    const String &getAsciiText() const { return m_ascii->getText(); }
    internal::Text *getAddressControl() const { return m_address; }
    internal::Text *getHexControl() const { return m_hex; }
    internal::Text *getAsciiControl() const { return m_ascii; }

private:
    friend class HexView;

    void bind(size_t offset);

    HexView *m_view=nullptr;
    size_t m_offset=VirtualList::NoItem;
    size_t m_count=0;
    std::array<uint8_t, 32> m_bytes{};
    internal::Text *m_address=nullptr;
    internal::Text *m_hex=nullptr;
    internal::Text *m_ascii=nullptr;

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;
};

//! A memory view backed by callbacks, with only the visible rows held as
//! controls. Each row has an address, hex bytes (one extra space every eight)
//! and ASCII (0x20..0x7e, '.' otherwise), all in the skin's mono face.
//! Columns never shrink to fit; the view scrolls horizontally when needed.
//! VirtualList's fixed-height content is capped at INT_MAX canvas units.
class GWENI_EXPORT HexView: public VirtualList
{
public:
    GWENI_CONTROL(HexView, VirtualList);

public:
    GWENI_DRAWS_NOTHING

    //! Fills exactly count bytes starting at offset. The view requests only
    //! valid ranges, at most getBytesPerRow() bytes per row bind. Callbacks
    //! run on the UI thread and must not mutate the view during a read.
    using Read=std::function<void(size_t offset, uint8_t *buffer, size_t count)>;
    using Write=std::function<void(size_t offset, uint8_t byte)>;

    static constexpr size_t NoByte=VirtualList::NoItem;
    static constexpr int RowPadY=2;

    enum class Column { Hex, Ascii };
    struct ByteHit
    {
        size_t offset=NoByte;
        Column column=Column::Hex;
        explicit operator bool() const { return offset != NoByte; }
    };

    //! Replaces the source and rebinds the rows on screen, even at the same
    //! size. No read callback means an empty view. No write means read-only.
    //! The app owns the data and keeps it alive until the source is replaced.
    void setSource(size_t size, Read read, Write write={});
    size_t getDataSize() const { return m_size; }
    bool isReadOnly() const { return !m_write; }

    //! Added to row offsets for display only. Addresses are uint64_t and
    //! wrap modulo 2^64. The column fits the source's largest address.
    void setBaseAddress(uint64_t address);
    uint64_t getBaseAddress() const { return m_baseAddress; }
    unsigned getAddressDigits() const { return m_addressDigits; }

    //! 8, 16 (the default), or 32. Other values leave the view unchanged.
    void setBytesPerRow(size_t bytes);
    size_t getBytesPerRow() const { return m_bytesPerRow; }

    //! The app changed bytes in [begin, end), clipped to the source. Only
    //! rows on screen intersecting it are rebound by the next layout. Work
    //! is bounded by the row pool, even for a multi-megabyte range.
    void dataChanged(size_t begin, size_t end);

    const Font &getFont() const;
    float getCellWidth() const { return m_cellWidth; }
    int getAddressX() const;
    int getHexX() const;
    int getAsciiX() const;
    int getContentWidth() const;
    //! Byte positions in a row's local coordinates (byte < bytesPerRow).
    int byteX(size_t byte, Column column) const;
    //! View-local coordinates. Padding, scrollbars, inter-byte/group gaps,
    //! unused cells of a partial row and space below the data have no hit.
    ByteHit hitTestByte(Point local) const;

    HexViewRow *rowForOffset(size_t offset) const;
    void calculateSize(skin::Base *skin, Dim dim) override;

protected:
    Size contentSize() const override;

private:
    friend class HexViewRow;

    void updateRows();
    void updateAddressDigits();
    int cellX(size_t cells) const;
    size_t hexCells() const;

    Read m_read;
    Write m_write;
    size_t m_size=0;
    size_t m_bytesPerRow=16;
    uint64_t m_baseAddress=0;
    unsigned m_addressDigits=1;
    float m_cellWidth=0.f;
    Font m_metricsFont;
    const renderer::Base *m_metricsRenderer=nullptr;
    unsigned m_metricsSerial=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_hexview_h_
