#ifndef _gweni_hexView_example_h_
#define _gweni_hexView_example_h_

#include <gweni/controls/base.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gweni
{
namespace controls
{
class HexView;
}

class HexViewDemo: public controls::Base
{
public:
    GWENI_CONTROL(HexViewDemo, controls::Base);

public:
    //! The generated buffer's size, and its view's base address: both sizes
    //! a test runs at (4 KiB, 16 MiB) take 8 address digits from it.
    static constexpr size_t DataSize=16*1024*1024;
    static constexpr uint64_t BaseAddress=0x10000000;
    static constexpr size_t EditSize=256;

    //! The generated buffer's byte at \p offset: a small header (a magic,
    //! three fields, a double), then rows of text, zero runs and hashed bytes.
    static uint8_t generatedByte(size_t offset);

    //! The generated view's source, \p size bytes of it (the highlights and
    //! the cursor kept where the data still has them).
    void setDataSize(size_t size);

    controls::HexView *generated() const { return m_view; }
    controls::HexView *editable() const { return m_edit; }
    const std::array<uint8_t, EditSize> &editData() const { return m_data; }
    //! The generated view's read calls and bytes so far (a test takes deltas).
    size_t readCalls() const { return m_reads; }
    size_t readBytes() const { return m_readBytes; }

private:
    controls::HexView *m_view=nullptr;
    controls::HexView *m_edit=nullptr;
    std::array<uint8_t, EditSize> m_data{};
    size_t m_reads=0;
    size_t m_readBytes=0;
};

}//namespace gweni

#endif//_gweni_hexView_example_h_
