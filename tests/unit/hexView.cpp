/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * MIT License, see LICENSE
 *
 * HexView's source-backed fixed row pool: exact address/hex/ASCII strings,
 * 8/16/32-byte rows and partial tails, wide and wrapping addresses, both
 * byte columns' cell edges and gaps (including fractional mono advances),
 * scrolling and viewport clipping. Reads stay inside the source, replacing
 * it refreshes kept rows, and dataChanged reads only intersecting shown rows.
 * Unchanged strings leave all three Texts clean, a changed byte marks only
 * its hex and ASCII runs, and idle frames read/render/layout nothing. The
 * mono role and metric serial drive the columns and row height. At 4 KiB
 * and 16 MiB the pool/controls/ids and one-row read work are the same, rows
 * still showing their item keep it, and deleting the view releases its ids.
 */
#include "headlessFixture.h"

#include <gweni/controls/hexView.h>

#include <doctest/doctest.h>

#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

using namespace gweni;
using controls::HexView;
using controls::HexViewRow;

namespace
{

struct MonoRenderer: test::StubRenderer
{
    float advance=7.25f;
    int height=14;

    Point measureText(const Font &, const String &text) override
    {
        ++measures;
        return Point(static_cast<int>(std::ceil(text.size()*static_cast<double>(advance))), height);
    }

    int lineHeight(const Font &) override
    {
        ++lineHeights;
        return height;
    }
};

//! Actual Text controls hold text ids as they do under the textured skin,
//! making the pool/teardown comparisons exercise both id banks.
struct HexSkin: test::StubSkin
{
    using test::StubSkin::StubSkin;

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == controls::internal::Text::getTypeNameStatic())
            return std::make_unique<test::StubControl>(this, true, std::move(typeName));
        return test::StubSkin::getControlRenderer(std::move(typeName));
    }
};

struct Scene
{
    MonoRenderer renderer;
    HexSkin skin{&renderer};
    std::unique_ptr<controls::Canvas> canvas;
    HexView *view=nullptr;
    std::vector<std::pair<size_t, size_t>> reads;
    std::map<size_t, uint8_t> overrides;
    size_t sourceSize=0;
    int maxPasses=0;
    int requests=0;
    bool renderLayout=false;

    explicit Scene(size_t size, size_t bytesPerRow=16): sourceSize(size)
    {
        skin.setDefaultMonoFont("test-mono", 11.f);
        canvas=std::make_unique<controls::Canvas>(&skin);
        canvas->setSize(1400, 600);
        view=canvas->newChild<HexView>();
        view->setBounds(11, 13, 1300, 180);
        view->setBytesPerRow(bytesPerRow);
        view->setSource(size, reader());
        frame();
    }

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
    }

    HexView::Read reader()
    {
        return [this](size_t offset, uint8_t *buffer, size_t count)
            {
                reads.push_back({offset, count});
                REQUIRE(offset <= sourceSize);
                REQUIRE(count <= sourceSize-offset);
                REQUIRE(count <= view->getBytesPerRow());
                for(size_t i=0; i < count; ++i)
                {
                    auto found=overrides.find(offset+i);
                    buffer[i]=found == overrides.end()?static_cast<uint8_t>(offset+i):found->second;
                }
            };
    }

    void frame()
    {
        controls::resetChangeStats();
        canvas->updateCanvas();
        canvas->renderCanvas();
        const auto &layout=canvas->getLayoutStats();
        maxPasses=std::max(maxPasses, layout.passes);
        requests+=layout.requests;
        renderLayout=renderLayout || layout.renderLayout;
    }

    void checkOnePass() const
    {
        CHECK(maxPasses <= 1);
        CHECK(requests == 0);
        CHECK(!renderLayout);
    }

    HexViewRow *row(size_t index) const
    {
        auto *row=gweni_cast<HexViewRow>(view->rowForItem(index));
        REQUIRE(row != nullptr);
        return row;
    }

    void clearReads() { reads.clear(); }
};

}//namespace

TEST_CASE("HexView: rows format full and partial data in 8 16 and 32 bytes")
{
    Scene s(35);
    CHECK(s.view->getBytesPerRow() == 16);
    CHECK(s.view->getAddressDigits() == 2);
    CHECK(s.row(0)->getAddressText() == "00");
    CHECK(s.row(0)->getHexText() == "00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F");
    CHECK(s.row(0)->getAsciiText() == "................");
    CHECK(s.row(2)->getOffset() == 32);
    CHECK(s.row(2)->getByteCount() == 3);
    CHECK(s.row(2)->getAddressText() == "20");
    CHECK(s.row(2)->getHexText() == "20 21 22");
    CHECK(s.row(2)->getAsciiText() == " !\"");

    s.view->setBytesPerRow(8);
    s.frame();
    CHECK(s.view->getItemCount() == 5);
    CHECK(s.row(1)->getAddressText() == "08");
    CHECK(s.row(1)->getHexText() == "08 09 0A 0B 0C 0D 0E 0F");
    CHECK(s.row(4)->getHexText() == "20 21 22");
    s.view->setBytesPerRow(32);
    s.frame();
    CHECK(s.view->getItemCount() == 2);
    CHECK(s.row(0)->getHexText() ==
        "00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  "
        "10 11 12 13 14 15 16 17  18 19 1A 1B 1C 1D 1E 1F");
    CHECK(s.row(0)->getAsciiText() == String(32, '.'));
    CHECK(s.row(1)->getHexText() == "20 21 22");

    // ASCII is locale independent, inclusive only of 0x20..0x7e.
    s.overrides={{0, 0x1f}, {1, 0x20}, {2, 0x7e}, {3, 0x7f}, {4, 0x80}, {5, 0xff}};
    s.view->dataChanged(0, 6);
    s.frame();
    CHECK(s.row(0)->getAsciiText().substr(0, 6) == ". ~...");
    s.checkOnePass();
}

TEST_CASE("HexView: address digits fit the source and base address")
{
    Scene s(17);
    s.view->setBaseAddress(UINT64_C(0x123456789ABCDEF0));
    s.frame();
    CHECK(s.view->getAddressDigits() == 16);
    CHECK(s.row(0)->getAddressText() == "123456789ABCDEF0");
    CHECK(s.row(1)->getAddressText() == "123456789ABCDF00");

    s.view->setBaseAddress(0xf0);
    s.frame();
    CHECK(s.view->getAddressDigits() == 3);
    CHECK(s.row(0)->getAddressText() == "0F0");
    CHECK(s.row(1)->getAddressText() == "100");
    s.view->setBaseAddress(UINT64_MAX-15);
    s.frame();
    CHECK(s.row(1)->getAddressText() == "0000000000000000");
    s.view->setBaseAddress(0);
    s.frame();
    CHECK(s.view->getAddressDigits() == 2);
    CHECK(s.row(0)->getAddressText() == "00");
    s.checkOnePass();
}

TEST_CASE("HexView: byte hits use both columns and reject their gaps")
{
    for(size_t bpr : {size_t(8), size_t(16), size_t(32)})
    {
        Scene s(bpr+3, bpr);
        const Rect viewport=s.view->getViewport();
        const int y=viewport.y+3;
        const int hexX=viewport.x+s.view->getHexX();
        const int asciiX=viewport.x+s.view->getAsciiX();
        const auto edge=[](size_t cell) { return static_cast<int>(std::floor(cell*7.25)); };

        // Every pixel of each byte, including each first pixel, and every
        // separator cell. Expectations are the string's cell layout.
        for(size_t i=0; i < bpr; ++i)
        {
            INFO("bytes per row ", bpr, " byte ", i);
            const size_t cell=3*i+i/8;
            CHECK(s.view->byteX(i, HexView::Column::Hex) == s.view->getHexX()+edge(cell));
            CHECK(s.view->byteX(i, HexView::Column::Ascii) == s.view->getAsciiX()+edge(i));
            for(int x=edge(cell); x < edge(cell+2); ++x)
            {
                auto hit=s.view->hitTestByte(Point(hexX+x, y));
                CHECK(hit.offset == i);
                CHECK(hit.column == HexView::Column::Hex);
            }
            for(int x=edge(i); x < edge(i+1); ++x)
            {
                auto hit=s.view->hitTestByte(Point(asciiX+x, y));
                CHECK(hit.offset == i);
                CHECK(hit.column == HexView::Column::Ascii);
            }
            if(i+1 < bpr)
            {
                const size_t next=3*(i+1)+(i+1)/8;
                for(int x=edge(cell+2); x < edge(next); ++x)
                    CHECK_FALSE(s.view->hitTestByte(Point(hexX+x, y)));
            }
        }
        CHECK_FALSE(s.view->hitTestByte(Point(viewport.x+s.view->getAddressX(), y)));
        CHECK_FALSE(s.view->hitTestByte(Point(asciiX-1, y)));
        CHECK_FALSE(s.view->hitTestByte(Point(viewport.x-1, y)));
        CHECK_FALSE(s.view->hitTestByte(Point(viewport.x+viewport.w, y)));
        CHECK_FALSE(s.view->hitTestByte(Point(hexX, viewport.y-1)));
        CHECK_FALSE(s.view->hitTestByte(Point(hexX, viewport.y+viewport.h)));
        const int lastY=y+s.view->getRowHeight();
        CHECK(s.view->hitTestByte(Point(asciiX+edge(2), lastY)).offset == bpr+2);
        CHECK_FALSE(s.view->hitTestByte(Point(asciiX+edge(3), lastY)));
        CHECK_FALSE(s.view->hitTestByte(Point(hexX+edge(9), lastY)));
        CHECK_FALSE(s.view->hitTestByte(Point(hexX, y+2*s.view->getRowHeight())));
        s.checkOnePass();
    }

    Scene s(4096);
    s.view->setSize(300, 100);
    s.view->scrollByRows(3);
    s.frame();
    s.view->setHorizontalScrollOffset(s.view->getMaxHorizontalScrollOffset());
    s.frame();
    const Rect viewport=s.view->getViewport();
    const int x=viewport.x+s.view->getAsciiX()-s.view->getHorizontalScrollOffset();
    CHECK(s.view->hitTestByte(Point(x, viewport.y+3)).offset == 48);
    s.view->setVerticalScrollOffset(3*s.view->getRowHeight()+7);
    s.frame();
    CHECK(s.view->hitTestByte(Point(x, viewport.y+s.view->getRowHeight()-7)).offset == 64);
    s.checkOnePass();
}

TEST_CASE("HexView: source replacement and row size changes refresh kept rows")
{
    Scene s(4096);
    CHECK(s.view->isReadOnly());
    s.view->scrollByRows(10);
    s.frame();
    s.clearReads();
    HexViewRow *kept=s.row(10);
    s.overrides[160]='A';
    s.view->setSource(s.sourceSize, s.reader(), [](size_t, uint8_t) {});
    s.frame();
    CHECK_FALSE(s.view->isReadOnly());
    CHECK(s.row(10) == kept);
    CHECK(s.row(10)->getHexText().substr(0, 2) == "41");
    CHECK(s.row(10)->getAsciiText().substr(0, 1) == "A");
    CHECK(s.reads.size() == static_cast<size_t>(s.view->getViewportHeight()/s.view->getRowHeight()));

    s.sourceSize=1;
    s.view->setSource(1, s.reader());
    s.frame();
    CHECK(s.view->isReadOnly());
    CHECK(s.view->getVerticalScrollOffset() == 0);
    CHECK(s.view->getItemCount() == 1);
    CHECK(s.row(0)->getHexText() == "00");
    const int asciiX=s.view->getAsciiX();
    s.view->setBytesPerRow(8);
    s.frame();
    CHECK(s.view->getAsciiX() < asciiX);
    CHECK(s.row(0)->getAsciiControl()->getX() == s.view->getAsciiX());
    for(size_t invalid : {size_t(0), size_t(7), size_t(64)})
        s.view->setBytesPerRow(invalid);
    CHECK(s.view->getBytesPerRow() == 8);

    s.view->setSource(100, {});
    s.frame();
    CHECK(s.view->getDataSize() == 0);
    CHECK(s.view->getItemCount() == 0);
    CHECK(s.view->rowForOffset(0) == nullptr);
    for(auto *row : s.view->getRows())
        CHECK(row->hidden());
    s.checkOnePass();
}

TEST_CASE("HexView: dataChanged rebinds only intersecting shown rows")
{
    Scene s(16*1024*1024);
    s.view->scrollByRows(100);
    s.frame();
    s.clearReads();
    s.view->dataChanged(0, 1600);
    s.view->dataChanged(1760, s.sourceSize);
    s.view->dataChanged(1605, 1605);
    s.view->dataChanged(1610, 1609);
    s.view->dataChanged(s.sourceSize, SIZE_MAX);
    s.frame();
    CHECK(s.reads.empty());

    s.view->dataChanged(1615, 1617); // end-exclusive: two rows across the edge
    s.view->dataChanged(1616, 1617); // already stale: still bound only once
    s.frame();
    CHECK(s.reads == std::vector<std::pair<size_t, size_t>>{{1600, 16}, {1616, 16}});
    s.clearReads();

    s.view->dataChanged(0, SIZE_MAX);
    s.frame();
    CHECK(s.reads.size() == 10);
    for(auto read : s.reads)
        CHECK(read.first >= 1600);
    s.clearReads();
    s.view->scrollByRows(100);
    s.frame();
    CHECK(s.row(200)->getOffset() == 3200);
    s.checkOnePass();
}

TEST_CASE("HexView: unchanged row strings and idle frames do no text work")
{
    Scene s(4096);
    s.clearReads();
    s.frame();
    CHECK(s.reads.empty());
    CHECK(s.canvas->getLayoutStats().passes == 0);
    CHECK(controls::getChangeStats().controls == 0);

    s.view->dataChanged(16, 17);
    s.canvas->updateCanvas();
    HexViewRow *row=s.row(1);
    CHECK(row->getAddressControl()->getStateChange() == controls::StateChange_Nothing);
    CHECK(row->getHexControl()->getStateChange() == controls::StateChange_Nothing);
    CHECK(row->getAsciiControl()->getStateChange() == controls::StateChange_Nothing);
    s.canvas->renderCanvas();

    s.overrides[16]='Z';
    s.view->dataChanged(16, 17);
    s.canvas->updateCanvas();
    CHECK(row->getAddressControl()->getStateChange() == controls::StateChange_Nothing);
    CHECK((row->getHexControl()->getStateChange() & controls::StateChange_Text) != 0);
    CHECK((row->getAsciiControl()->getStateChange() & controls::StateChange_Text) != 0);
    CHECK(s.row(0)->getHexControl()->getStateChange() == controls::StateChange_Nothing);
    CHECK(row->getHexText().substr(0, 2) == "5A");
    CHECK(row->getAsciiText().substr(0, 1) == "Z");
    s.canvas->renderCanvas();
    s.frame();
    CHECK(controls::getChangeStats().controls == 0);
    s.checkOnePass();
}

TEST_CASE("HexView: mono metrics place all columns and follow text layout changes")
{
    Scene s(4096);
    CHECK(s.view->getFont().facename == "test-mono");
    CHECK(s.view->getCellWidth() == 7.25f);
    CHECK(s.view->getRowHeight() == 18);
    for(auto *base : s.view->getRows())
    {
        auto *row=gweni_cast<HexViewRow>(base);
        REQUIRE(row != nullptr);
        CHECK((row->getAddressControl()->getFont() == s.skin.getDefaultMonoFont()));
        CHECK((row->getHexControl()->getFont() == s.skin.getDefaultMonoFont()));
        CHECK((row->getAsciiControl()->getFont() == s.skin.getDefaultMonoFont()));
        if(row->hidden())
            continue;
        CHECK(row->getAddressControl()->getX() == s.view->getAddressX());
        CHECK(row->getHexControl()->getX() == s.view->getHexX());
        CHECK(row->getAsciiControl()->getX() == s.view->getAsciiX());
        CHECK(row->getAsciiControl()->getY() == HexView::RowPadY);
    }
    s.renderer.advance=8.5f;
    s.renderer.height=20;
    s.renderer.changeTextLayout();
    s.frame();
    CHECK(s.view->getCellWidth() == 8.5f);
    CHECK(s.view->getRowHeight() == 24);
    CHECK(s.row(0)->getAsciiControl()->getX() == s.view->getAsciiX());
    CHECK(s.row(0)->getHexControl()->getHeight() == 20);
    s.checkOnePass();
}

TEST_CASE("HexView: one row scroll has a bounded pool and reads one incoming row")
{
    using Counts=std::tuple<size_t, size_t, size_t, size_t>;
    std::vector<Counts> counts;
    for(size_t size : {size_t(4096), size_t(16*1024*1024)})
    {
        Scene s(size);
        s.view->setBaseAddress(0x10000000); // equal address widths
        s.frame();
        const auto before=test::IdBankCounts::now();
        const size_t live=s.skin.liveControls;
        const auto rows=s.view->getRows();
        HexViewRow *kept=s.row(1);
        CHECK(rows.size() == 11);
        s.clearReads();
        s.view->scrollByRows(1);
        s.frame();
        CHECK(s.view->getRows() == rows);
        CHECK(s.row(1) == kept);
        CHECK(s.reads == std::vector<std::pair<size_t, size_t>>{{160, 16}});
        const auto after=test::IdBankCounts::now();
        CHECK(after.livePrimitives() == before.livePrimitives());
        CHECK(after.liveTexts() == before.liveTexts());
        CHECK(s.skin.liveControls == live);
        counts.emplace_back(rows.size(), live, after.livePrimitives(), after.liveTexts());

        // Offscreen app changes are read when a row enters view.
        s.overrides[3200]='Q';
        s.view->dataChanged(3200, 3201);
        s.view->scrollToItem(200);
        s.frame();
        CHECK(s.row(200)->getHexText().substr(0, 2) == "51");
        s.checkOnePass();
    }
    CHECK(counts[0] == counts[1]);
}

TEST_CASE("HexView: deletion releases the row subtree and ids")
{
    MonoRenderer renderer;
    HexSkin skin{&renderer};
    auto canvas=std::make_unique<controls::Canvas>(&skin);
    canvas->setSize(1000, 400);
    canvas->updateCanvas();
    canvas->renderCanvas();
    const auto before=test::IdBankCounts::now();
    const size_t controls=skin.liveControls;
    auto *view=canvas->newChild<HexView>();
    view->setSource(16*1024*1024, [](size_t, uint8_t *buffer, size_t count)
        { std::fill(buffer, buffer+count, 0); });
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(skin.liveControls > controls);
    delete view;
    canvas->updateCanvas();
    canvas->renderCanvas();
    const auto after=test::IdBankCounts::now();
    CHECK(after.livePrimitives() == before.livePrimitives());
    CHECK(after.liveTexts() == before.liveTexts());
    CHECK(skin.liveControls == controls);
    CHECK(renderer.doubleAcquired == 0);
    CHECK(renderer.doubleReleased == 0);
}
