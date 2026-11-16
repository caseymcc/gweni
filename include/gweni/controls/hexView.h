/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */

#ifndef _gweni_controls_hexview_h_
#define _gweni_controls_hexview_h_

#include <gweni/controls/button.h>
#include <gweni/controls/text.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/virtualList.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace gweni
{
namespace controls
{

class HexView;

namespace internal
{

//! A HexView row's hex or ASCII run: one Text drawn with colour spans (the
//! zero bytes' and the non-printable bytes' colours) over the run's colour,
//! by a skin renderer of its own (the textured skin's HexViewText).
class GWENI_EXPORT HexViewText: public Text
{
public:
    GWENI_CONTROL(HexViewText, Text);

public:
    const std::vector<renderer::ColorSpan> &getColorSpans() const { return m_spans; }
    //! Takes \p spans (swapped in), marking StateChange_Text only when they
    //! differ from the ones it had: the string is not laid out again.
    void setColorSpans(std::vector<renderer::ColorSpan> &spans);

private:
    std::vector<renderer::ColorSpan> m_spans;
};

//! The panel under a HexView's rows: an endian toggle and the value at the
//! cursor read as each type (HexView::PreviewType), a name and a value per
//! type in the mono face, two pairs a line. Made by the view.
class GWENI_EXPORT HexViewPreview: public Base
{
public:
    GWENI_CONTROL(HexViewPreview, Base);

public:
    GWENI_DRAWS_NOTHING

    Button *getEndianButton() const { return m_endian; }
    Text *getValueControl(size_t type) const { return m_values[type]; }
    Text *getNameControl(size_t type) const { return m_names[type]; }

private:
    friend class controls::HexView;

    HexView *m_view=nullptr;
    Button *m_endian=nullptr;
    std::array<Text *, 10> m_names{};
    std::array<Text *, 10> m_values{};

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;
};

}//namespace internal

//! What a shown row draws under its bytes (the textured renderer's slots):
//! the cursor's byte, the selection's bytes and up to MaxHighlights
//! highlight pieces, each a range of the row's bytes [begin, end), drawn in
//! both columns. A row compares them as they change and marks itself
//! (StateChange_Background) only when its own differ.
struct GWENI_EXPORT HexRowMarks
{
    static constexpr size_t MaxHighlights=4;

    struct Piece
    {
        uint8_t begin=0;
        uint8_t end=0;
        uint8_t surface=0;  //!< HexViewHighlight0..3

        bool operator==(const Piece &o) const
        {
            return begin == o.begin && end == o.end && surface == o.surface;
        }
    };

    int cursor=-1;              //!< the cursor's byte in the row, or -1
    int hover=-1;               //!< the hovered byte in the row, or -1
    bool cursorInAscii=false;   //!< the column typing goes to
    bool pendingNibble=false;   //!< a hex digit typed: the box is on the low digit
    //! the selected bytes, empty when equal, and while only the cursor's byte
    //! is selected (its box shows it)
    uint8_t selectionBegin=0;
    uint8_t selectionEnd=0;
    uint8_t highlightCount=0;
    std::array<Piece, MaxHighlights> highlights{};

    bool operator==(const HexRowMarks &o) const;
    bool operator!=(const HexRowMarks &o) const { return !(*this == o); }
};

//! A pooled HexView row. The view binds it to a byte offset, and places its
//! three mono Text runs in fixed columns. An app does not construct rows.
class GWENI_EXPORT HexViewRow: public Base
{
public:
    GWENI_CONTROL(HexViewRow, Base);

public:
    size_t getOffset() const { return m_offset; }
    size_t getByteCount() const { return m_count; }
    const String &getAddressText() const { return m_address->getText(); }
    const String &getHexText() const { return m_hex->getText(); }
    const String &getAsciiText() const { return m_ascii->getText(); }
    internal::Text *getAddressControl() const { return m_address; }
    internal::Text *getHexControl() const { return m_hex; }
    internal::Text *getAsciiControl() const { return m_ascii; }
    const HexRowMarks &getMarks() const { return m_marks; }
    HexView *getView() const { return m_view; }
    //! Its hex and ASCII runs' colour spans and the three runs' colours from
    //! the skin's (Colors.HexView), so a theme edit reaches them.
    void updateColors() override;

private:
    friend class HexView;

    void bind(size_t offset);
    void updateMarks();
    void updateSpans();

    HexView *m_view=nullptr;
    size_t m_offset=VirtualList::NoItem;
    size_t m_count=0;
    std::array<uint8_t, 32> m_bytes{};
    internal::Text *m_address=nullptr;
    internal::HexViewText *m_hex=nullptr;
    internal::HexViewText *m_ascii=nullptr;
    HexRowMarks m_marks;

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
    //! Units between the frame, the goto box and the rows, and the rows, the
    //! preview and the frame (each box is a gap in from the frame).
    static constexpr int GotoGap=4;
    static constexpr int PreviewGap=4;
    //! The endian toggle's height past the mono line height.
    static constexpr int PreviewButtonPad=6;
    //! The preview's lines of name/value pairs (two pairs a line).
    static constexpr int PreviewLines=5;

    //! The types the preview reads at the cursor, in its order.
    enum class PreviewType
    {
        Int8, UInt8, Int16, UInt16, Int32, UInt32, Int64, UInt64, Float, Double, Count
    };
    static constexpr size_t PreviewTypes=static_cast<size_t>(PreviewType::Count);
    static const char *previewName(PreviewType type);
    static size_t previewSize(PreviewType type);
    //! The value of \p type in \p bytes (\p available of them from the cursor),
    //! in decimal ("%.9g" for float, "%.17g" for double), or "n/a" when fewer
    //! than its size are there.
    static String previewValue(PreviewType type, const uint8_t *bytes, size_t available, bool bigEndian);

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

    //! The cursor's byte (NoByte for an empty source) and the selection's
    //! other end. The selection is every byte from the anchor to the cursor,
    //! both included, so it is never empty while there is a cursor.
    size_t getCursor() const { return m_cursor; }
    size_t getAnchor() const { return m_anchor; }
    Column getCursorColumn() const { return m_cursorColumn; }
    size_t getSelectionBegin() const { return std::min(m_anchor, m_cursor); }
    //! One past the last selected byte (0 with no cursor).
    size_t getSelectionEnd() const { return m_cursor == NoByte?0:std::max(m_anchor, m_cursor)+1; }
    //! Moves the cursor to \p offset (clamped to the data) and scrolls it into
    //! view; \p extend keeps the anchor, otherwise the anchor follows. Any
    //! half-typed byte is dropped.
    void setCursor(size_t offset, bool extend=false);
    void setCursorColumn(Column column);
    void selectRange(size_t begin, size_t end);
    //! The selection as space-separated hex ("41 42 43"), "" with no cursor.
    String getSelectedHex() const;
    //! platform::setClipboardText(getSelectedHex()), as Ctrl+C does.
    void copy();
    //! The rows a page key moves: the whole rows in the viewport, at least 1.
    size_t pageRows() const;

    //! Moves the cursor to the address (the base address included), parsed
    //! by parseAddress; false (nothing moved) for text that is no address of
    //! the data. Return in the goto box calls it with the box's text.
    bool gotoAddress(const String &text);
    //! "0x1F", "$1F" and text with a hex letter are hex, plain digits decimal;
    //! false for anything else, an empty text or one past 64 bits.
    static bool parseAddress(const String &text, uint64_t &address);
    //! The goto-address box above the rows (shown by default).
    TextBox *getGotoBox() const { return m_goto; }
    void setGotoVisible(bool visible);
    bool isGotoVisible() const { return !m_goto->hidden(); }

    //! Ranges the rows draw under their bytes in both columns, in the
    //! HexViewHighlight<surface % 4> surface. A row draws at most
    //! HexRowMarks::MaxHighlights of them, the earliest added; [begin, end)
    //! is clipped to the data. Each row bind scans them all, so they are for
    //! a few structures, not thousands.
    struct Highlight
    {
        size_t begin=0;
        size_t end=0;
        unsigned surface=0;
    };
    void addHighlight(size_t begin, size_t end, unsigned surface);
    void clearHighlights();
    const std::vector<Highlight> &getHighlights() const { return m_highlights; }

    //! The nibble typed into the cursor's byte (the high one), or -1.
    int getPendingNibble() const { return m_pendingNibble; }

    //! A byte written by typing (the write callback called first):
    //! info.integer the offset, info.point (old, new). getLastEdit() too.
    struct Edit
    {
        size_t offset=NoByte;
        uint8_t oldValue=0;
        uint8_t newValue=0;
    };
    const Edit &getLastEdit() const { return m_lastEdit; }
    gweni::event::Caller onEdited;
    //! The cursor or the selection moved (never from setSource's clamping).
    gweni::event::Caller onCursorChanged;

    //! The data preview under the rows (shown by default), read at the cursor.
    internal::HexViewPreview *getPreview() const { return m_preview; }
    void setPreviewVisible(bool visible);
    bool isPreviewVisible() const { return !m_preview->hidden(); }
    void setBigEndian(bool big);
    bool isBigEndian() const { return m_bigEndian; }
    //! The preview's value shown for \p type.
    const String &getPreviewText(PreviewType type) const;

    //! The hovered byte (both columns draw it), or NoByte.
    size_t getHoveredByte() const { return m_hover; }

    //! A row's span of bytes [begin, end) in \p column, in the row's units:
    //! x and width (the hex column from the first byte's first digit to the
    //! last one's second, the gaps between them included).
    void byteSpan(size_t begin, size_t end, Column column, int &x, int &width) const;
    //! The cursor box in a row, for its byte in \p column: the byte's cells,
    //! or only the low digit's while a nibble is pending.
    void cursorSpan(size_t byte, Column column, bool pending, int &x, int &width) const;

    bool AccelOnlyFocus() override { return true; }
    void updateColors() override;

protected:
    Size contentSize() const override;
    void contentsPlaced(skin::Base *skin) override;

    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseLeave() override;
    bool onChar(gweni::UnicodeChar c) override;
    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    bool onKeyPageUp(bool down) override;
    bool onKeyPageDown(bool down) override;
    bool onKeyEscape(bool down) override;
    void onLostKeyboardFocus() override;

private:
    friend class HexViewRow;
    friend class internal::HexViewPreview;

    void onCopy(event::Info);
    void onGotoReturn(event::Info);
    void moveBy(int64_t bytes, bool extend);
    void scrollCursorIntoView();
    bool cursorInView();
    void columnsMoved();
    void updateMarks();
    void writeByte(size_t offset, uint8_t value);
    void dropPendingNibble();
    ByteHit byteNear(Point local) const;
    int gotoHeight() const;
    int previewHeight() const;
    void updatePreview();
    void updateHover(Point local);
    void setHover(size_t byte);
    void onEndianToggled(event::Info);

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

    size_t m_cursor=NoByte;
    size_t m_anchor=NoByte;
    Column m_cursorColumn=Column::Hex;
    int m_pendingNibble=-1;
    std::vector<Highlight> m_highlights;
    Edit m_lastEdit;
    TextBox *m_goto=nullptr;
    int m_gotoLineHeight=0;
    internal::HexViewPreview *m_preview=nullptr;
    bool m_bigEndian=false;
    size_t m_hover=NoByte;
    bool m_hoverKnown=false;
    Point m_hoverPoint;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_hexview_h_
