/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */

#include "demo/hexView.h"
#include "gweni/controls/hexView.h"
#include "gweni/controls/label.h"

#include <cstring>

namespace gweni
{

namespace
{

// The header the generated buffer starts with, highlighted in the view: a
// four-byte magic, a version (uint16), flags (uint16), an entry count
// (uint32), a scale (float 1.5) and a time (double 1.0e9), little endian.
const uint8_t Header[24]={
    'G', 'W', 'N', 'I',
    0x02, 0x00, 0x30, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x3f,
    0x00, 0x00, 0x00, 0x00, 0x65, 0xcd, 0xcd, 0x41};

const char Text[]="gweni hex view: only the rows on screen are read. ";

}//namespace

uint8_t HexViewDemo::generatedByte(size_t offset)
{
    if(offset < sizeof(Header))
        return Header[offset];

    const size_t row=offset/16;

    // every fifth row text, every seventh zeros, the rest a hash of the offset
    if(row%5 == 2)
        return static_cast<uint8_t>(Text[offset%(sizeof(Text)-1)]);
    if(row%7 == 3)
        return 0;

    uint32_t h=static_cast<uint32_t>(offset)*2654435761u;

    h^=h >> 15;
    h*=2246822519u;
    h^=h >> 13;
    return static_cast<uint8_t>(h);
}

// The generated 16 MiB buffer in a view filling the demo, with the goto box
// and the preview, its header's three structures highlighted; under it a
// 256-byte view the keyboard edits (hex digits in the hex column, characters
// in the ASCII one), without the goto box or the preview.
GWENI_CONTROL_CONSTRUCTOR(HexViewDemo)
{
    setDock(DockPosition::Center);

    controls::Label *title=newChild<controls::Label>();

    title->setText("16 MiB generated, its header's three structures highlighted");
    title->setDock(DockPosition::Top);
    title->setMargin(Margin(10, 6, 10, 2));

    for(size_t i=0; i < m_data.size(); ++i)
        m_data[i]=static_cast<uint8_t>(i < 32?"Edit me: hex digits or ASCII.   "[i]:i*7);

    m_edit=newChild<controls::HexView>();
    m_edit->setDock(DockPosition::Bottom);
    m_edit->setHeight(120);
    m_edit->setMargin(Margin(10, 2, 10, 10));
    m_edit->setGotoVisible(false);
    m_edit->setPreviewVisible(false);
    m_edit->setSource(m_data.size(),
        [this](size_t offset, uint8_t *buffer, size_t count)
        {
            std::memcpy(buffer, m_data.data()+offset, count);
        },
        [this](size_t offset, uint8_t byte) { m_data[offset]=byte; });

    controls::Label *editTitle=newChild<controls::Label>();

    editTitle->setText("256 bytes, editable");
    editTitle->setDock(DockPosition::Bottom);
    editTitle->setMargin(Margin(10, 6, 10, 2));

    m_view=newChild<controls::HexView>();
    m_view->setDock(DockPosition::Center);
    m_view->setMargin(Margin(10, 2, 10, 0));
    m_view->setBaseAddress(BaseAddress);
    m_view->addHighlight(0, 4, 0);
    m_view->addHighlight(4, 12, 1);
    m_view->addHighlight(12, 24, 2);
    setDataSize(DataSize);
}

void HexViewDemo::setDataSize(size_t size)
{
    m_view->setSource(size, [this](size_t offset, uint8_t *buffer, size_t count)
        {
            ++m_reads;
            m_readBytes+=count;
            for(size_t i=0; i < count; ++i)
                buffer[i]=generatedByte(offset+i);
        });
}

}//namespace gweni
