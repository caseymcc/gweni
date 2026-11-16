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
 * The cursor: a click in either column places it, Shift+click and a drag
 * (past the rows too, clamped) extend it, a gap places nothing; every cursor
 * key with and without Shift, clamped at both ends and at a partial last
 * row, scrolled into view vertically and sideways. Ctrl+C copies the
 * selection as spaced hex only while the view has the focus; addresses
 * parse as hex or decimal, goto moves the cursor and the box sits above the
 * rows. Typing writes a byte nibble by nibble (the pending one shown, Escape
 * or a move dropping it) or as an ASCII character, through the write
 * callback and onEdited; a read-only view takes nothing. Cursor, selection
 * and highlight marks change only the rows whose own pieces moved, capped at
 * HexRowMarks::MaxHighlights a row. The cursor keys and typing are the
 * view's only while it has the focus (Down in the goto box or on the endian
 * button moves nothing), a half-typed byte goes with the focus, and a drag
 * inside one byte or a key at a limit lays nothing out.
 */
#include "headlessFixture.h"

#include <gweni/controls/hexView.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

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
using controls::HexRowMarks;
using controls::TextBox;

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

    double clock=10.0;
    std::vector<std::tuple<size_t, uint8_t>> writes;

    explicit Scene(size_t size, size_t bytesPerRow=16, bool gotoBox=false, bool preview=false): sourceSize(size)
    {
        platform::setTimeSource([this]() { return clock; });
        skin.setDefaultMonoFont("test-mono", 11.f);
        canvas=std::make_unique<controls::Canvas>(&skin);
        canvas->setSize(1400, 600);
        view=canvas->newChild<HexView>();
        view->setBounds(11, 13, 1300, 180);
        view->setBytesPerRow(bytesPerRow);
        view->setGotoVisible(gotoBox);
        view->setPreviewVisible(preview);
        view->setSource(size, reader());
        frame();
    }

    ~Scene()
    {
        input::onMouseClicked(canvas.get(), 0, false);
        input::onKeyEvent(canvas.get(), keys::Shift, false);
        input::onKeyEvent(canvas.get(), keys::Control, false);
        platform::setTimeSource(nullptr);
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

    HexView::Write writer()
    {
        return [this](size_t offset, uint8_t byte)
            {
                writes.push_back({offset, byte});
                overrides[offset]=byte;
            };
    }

    //! The canvas point of a byte's first cell (its first hex digit's cell
    //! or its ASCII cell), a unit in and at its row's middle.
    Point bytePoint(size_t offset, HexView::Column column) const
    {
        const Rect viewport=view->getViewport();
        const size_t bpr=view->getBytesPerRow();
        const int64_t y=static_cast<int64_t>(offset/bpr)*view->getRowHeight()
            -view->getVerticalScrollOffset()+view->getRowHeight()/2;
        const int x=view->byteX(offset%bpr, column)-view->getHorizontalScrollOffset()+1;

        return view->localPosToCanvas(Point(viewport.x+x, viewport.y+static_cast<int>(y)));
    }

    void move(Point at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
    }

    //! A press and release a second after the last, so never a double-click.
    void click(Point at, bool shift=false)
    {
        clock+=1.0;
        move(at);
        if(shift)
            input::onKeyEvent(canvas.get(), keys::Shift, true);
        input::onMouseClicked(canvas.get(), 0, true);
        input::onMouseClicked(canvas.get(), 0, false);
        if(shift)
            input::onKeyEvent(canvas.get(), keys::Shift, false);
        frame();
    }

    void key(int key, bool shift=false, bool control=false)
    {
        if(shift)
            input::onKeyEvent(canvas.get(), keys::Shift, true);
        if(control)
            input::onKeyEvent(canvas.get(), keys::Control, true);
        input::onKeyEvent(canvas.get(), key, true);
        input::onKeyEvent(canvas.get(), key, false);
        if(control)
            input::onKeyEvent(canvas.get(), keys::Control, false);
        if(shift)
            input::onKeyEvent(canvas.get(), keys::Shift, false);
        frame();
    }

    void type(const char *text)
    {
        for(const char *c=text; *c; ++c)
            canvas->inputCharacter(static_cast<UnicodeChar>(*c));
        frame();
    }

    void ctrlChar(char c)
    {
        canvas->inputModifierKey(keys::Control, true);
        canvas->inputCharacter(static_cast<UnicodeChar>(c));
        canvas->inputModifierKey(keys::Control, false);
        frame();
    }
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

TEST_CASE("HexView: clicks place the cursor and Shift-click and drag extend it")
{
    Scene s(4096);
    using Column=HexView::Column;
    CHECK(s.view->getCursor() == 0);
    CHECK(s.view->getSelectionBegin() == 0);
    CHECK(s.view->getSelectionEnd() == 1);

    s.click(s.bytePoint(37, Column::Hex));
    CHECK(s.view->getCursor() == 37);
    CHECK(s.view->getAnchor() == 37);
    CHECK(s.view->getCursorColumn() == Column::Hex);
    CHECK(KeyboardFocus == s.view);

    s.click(s.bytePoint(18, Column::Ascii), true);
    CHECK(s.view->getCursor() == 18);
    CHECK(s.view->getAnchor() == 37);
    CHECK(s.view->getCursorColumn() == Column::Ascii);
    CHECK(s.view->getSelectionBegin() == 18);
    CHECK(s.view->getSelectionEnd() == 38);

    // a press on a gap between two bytes places nothing
    Point gap=s.bytePoint(5, Column::Hex);
    gap.x+=static_cast<int>(2*s.view->getCellWidth());
    s.click(gap);
    CHECK(s.view->getCursor() == 18);
    CHECK(s.view->getAnchor() == 37);

    // a drag from byte 3: over the gap after byte 9 (nearest, byte 9 or 10),
    // onto byte 40, then below the last shown row (clamped to the data)
    s.clock+=1.0;
    s.move(s.bytePoint(3, Column::Hex));
    input::onMouseClicked(s.canvas.get(), 0, true);
    s.frame();
    CHECK(s.view->getCursor() == 3);
    CHECK(s.view->getAnchor() == 3);
    s.move(s.bytePoint(40, Column::Hex));
    s.frame();
    CHECK(s.view->getCursor() == 40);
    CHECK(s.view->getAnchor() == 3);
    const Rect viewport=s.view->getViewport();
    const Point below=s.view->localPosToCanvas(Point(viewport.x+s.view->getHexX(), viewport.y+viewport.h+50));
    s.move(below);
    s.frame();
    const size_t shown=static_cast<size_t>((viewport.h+50)/s.view->getRowHeight());
    CHECK(s.view->getCursor() == shown*16);
    CHECK(s.view->getAnchor() == 3);
    input::onMouseClicked(s.canvas.get(), 0, false);
    s.frame();
    // released: moving again selects nothing more
    s.move(s.bytePoint(1, Column::Hex));
    s.frame();
    CHECK(s.view->getCursor() == shown*16);
    // the drag scrolled the cursor's row into view
    CHECK(s.view->rowForOffset(s.view->getCursor()) != nullptr);
    s.checkOnePass();
}

TEST_CASE("HexView: cursor keys move clamp and extend with Shift")
{
    Scene s(1000);
    s.click(s.bytePoint(20, HexView::Column::Hex));
    REQUIRE(s.view->getCursor() == 20);
    const size_t page=s.view->pageRows();
    CHECK(page == static_cast<size_t>(s.view->getViewportHeight()/s.view->getRowHeight()));

    s.key(keys::Right);
    CHECK(s.view->getCursor() == 21);
    s.key(keys::Left);
    s.key(keys::Left);
    CHECK(s.view->getCursor() == 19);
    s.key(keys::Down);
    CHECK(s.view->getCursor() == 35);
    s.key(keys::Up);
    CHECK(s.view->getCursor() == 19);
    s.key(keys::Up);
    CHECK(s.view->getCursor() == 3);
    s.key(keys::Up);
    CHECK(s.view->getCursor() == 0);
    s.key(keys::Left);
    CHECK(s.view->getCursor() == 0);
    s.key(keys::PageDown);
    CHECK(s.view->getCursor() == page*16);
    s.key(keys::PageUp);
    CHECK(s.view->getCursor() == 0);
    s.view->setCursor(37);
    s.key(keys::End);
    CHECK(s.view->getCursor() == 47);
    s.key(keys::Home);
    CHECK(s.view->getCursor() == 32);
    CHECK(s.view->getAnchor() == 32);

    // the last row holds 1000-992 = 8 bytes: End stops at the data's end
    s.key(keys::End, false, true);
    CHECK(s.view->getCursor() == 999);
    CHECK(s.view->rowForOffset(999) != nullptr);
    s.view->setCursor(994);
    s.key(keys::End);
    CHECK(s.view->getCursor() == 999);
    s.key(keys::Down);
    CHECK(s.view->getCursor() == 999);
    s.key(keys::Right);
    CHECK(s.view->getCursor() == 999);
    s.key(keys::PageDown);
    CHECK(s.view->getCursor() == 999);
    s.key(keys::Home, false, true);
    CHECK(s.view->getCursor() == 0);
    CHECK(s.view->getVerticalScrollOffset() == 0);

    // Shift keeps the anchor with every key
    s.view->setCursor(100);
    s.key(keys::Right, true);
    s.key(keys::Down, true);
    CHECK(s.view->getAnchor() == 100);
    CHECK(s.view->getCursor() == 117);
    s.key(keys::End, true);
    CHECK(s.view->getCursor() == 127);
    s.key(keys::Home, true);
    CHECK(s.view->getCursor() == 112);
    s.key(keys::Up, true);
    s.key(keys::Left, true);
    CHECK(s.view->getCursor() == 95);
    CHECK(s.view->getSelectionBegin() == 95);
    CHECK(s.view->getSelectionEnd() == 101);
    s.key(keys::PageDown, true);
    CHECK(s.view->getCursor() == 95+page*16);
    s.key(keys::PageUp, true);
    CHECK(s.view->getCursor() == 95);
    s.key(keys::End, true, true);
    CHECK(s.view->getCursor() == 999);
    CHECK(s.view->getAnchor() == 100);
    s.key(keys::Home, true, true);
    CHECK(s.view->getCursor() == 0);
    CHECK(s.view->getAnchor() == 100);
    // a key without Shift collapses the selection
    s.key(keys::Right);
    CHECK(s.view->getCursor() == 1);
    CHECK(s.view->getAnchor() == 1);
    s.checkOnePass();
}

TEST_CASE("HexView: the cursor is scrolled into view vertically and sideways")
{
    Scene s(16*1024*1024);
    s.view->setSize(300, 120);
    s.frame();
    s.view->focus();
    s.view->setCursor(5000*16+15);
    s.frame();
    REQUIRE(s.view->rowForOffset(5000*16+15) != nullptr);
    const Rect viewport=s.view->getViewport();
    const int rowTop=static_cast<int>(5000*static_cast<int64_t>(s.view->getRowHeight())
        -s.view->getVerticalScrollOffset());
    CHECK(rowTop >= 0);
    CHECK(rowTop+s.view->getRowHeight() <= viewport.h);
    const int hscroll=s.view->getHorizontalScrollOffset();
    CHECK(s.view->byteX(15, HexView::Column::Hex) >= hscroll);
    // byte 15's two digits end at cell 3*15+1+2 = 48
    CHECK(s.view->getHexX()+static_cast<int>(std::floor(48*7.25)) <= hscroll+viewport.w);
    s.key(keys::Home);
    CHECK(s.view->getHorizontalScrollOffset() <= s.view->byteX(0, HexView::Column::Hex));
    s.checkOnePass();
}

TEST_CASE("HexView: Ctrl+C copies the selection as spaced hex and only when focused")
{
    Scene s(300);
    s.overrides[0x11]=0xab;
    s.view->dataChanged(0x11, 0x12);
    platform::setClipboardText("before");
    // not focused: the accelerator walk must not reach the view
    s.ctrlChar('c');
    CHECK(platform::getClipboardText() == "before");

    s.click(s.bytePoint(0x10, HexView::Column::Hex));
    s.ctrlChar('c');
    CHECK(platform::getClipboardText() == "10");
    s.click(s.bytePoint(0x13, HexView::Column::Ascii), true);
    CHECK(s.view->getSelectedHex() == "10 AB 12 13");
    s.ctrlChar('c');
    CHECK(platform::getClipboardText() == "10 AB 12 13");

    // more than one read's worth, backwards from the anchor
    s.view->selectRange(0, 300);
    const String all=s.view->getSelectedHex();
    CHECK(all.size() == 300*3-1);
    CHECK(all.substr(0, 9) == "00 01 02 ");
    CHECK(all.substr(all.size()-5) == "2A 2B");

    Scene empty(0);
    CHECK(empty.view->getCursor() == HexView::NoByte);
    CHECK(empty.view->getSelectedHex() == "");
    platform::setClipboardText("kept");
    empty.view->copy();
    CHECK(platform::getClipboardText() == "kept");
}

TEST_CASE("HexView: addresses parse as hex or decimal and goto moves the cursor")
{
    uint64_t a=0;
    CHECK(HexView::parseAddress("0x1F", a));
    CHECK(a == 0x1f);
    CHECK(HexView::parseAddress("$ff", a));
    CHECK(a == 0xff);
    CHECK(HexView::parseAddress("  1a ", a));
    CHECK(a == 0x1a);
    CHECK(HexView::parseAddress("100", a));
    CHECK(a == 100);
    CHECK(HexView::parseAddress("0xFFFFFFFFFFFFFFFF", a));
    CHECK(a == UINT64_MAX);
    CHECK(HexView::parseAddress("18446744073709551615", a));
    CHECK(a == UINT64_MAX);
    for(const char *bad : {"", "  ", "0x", "$", "12g", "-1", "0x10000000000000000", "18446744073709551616"})
    {
        INFO(bad);
        CHECK_FALSE(HexView::parseAddress(bad, a));
    }

    Scene s(4096, 16, true);
    CHECK(s.view->isGotoVisible());
    TextBox *box=s.view->getGotoBox();
    // the box above the rows, in the view's top padding
    CHECK(box->getY() == HexView::GotoGap);
    CHECK(box->getX() == s.view->getAddressX());
    CHECK(box->getHeight() > 0);
    CHECK(s.view->getViewport().y >= box->bottom()+HexView::GotoGap);
    s.view->setBaseAddress(0x1000);
    s.frame();
    CHECK(s.view->gotoAddress("0x1800"));
    CHECK(s.view->getCursor() == 0x800);
    s.frame();
    CHECK(s.view->rowForOffset(0x800) != nullptr);
    CHECK(s.view->gotoAddress("4096"));
    CHECK(s.view->getCursor() == 0);
    CHECK_FALSE(s.view->gotoAddress("0xfff"));
    CHECK_FALSE(s.view->gotoAddress("0x2000"));
    CHECK_FALSE(s.view->gotoAddress("zz"));
    CHECK(s.view->getCursor() == 0);

    // typed into the box, Return jumps and gives the view the focus
    s.click(box->localPosToCanvas(Point(4, box->getHeight()/2)));
    CHECK(KeyboardFocus == box);
    s.type("0x1A05");
    s.key(keys::Return);
    CHECK(s.view->getCursor() == 0xa05);
    CHECK(KeyboardFocus == s.view);
    s.checkOnePass();

    s.view->setGotoVisible(false);
    s.frame();
    CHECK(s.view->getViewport().y < box->bottom());
}

TEST_CASE("HexView: hex digits edit a byte a nibble at a time and Escape drops one")
{
    Scene s(64);
    s.view->setSource(64, s.reader(), s.writer());
    s.frame();
    std::vector<std::tuple<size_t, size_t, int, int>> edits;
    struct Sink: event::Handler
    {
        std::vector<std::tuple<size_t, size_t, int, int>> *edits;
        void edited(event::Info info)
        {
            auto *view=static_cast<HexView *>(info.controlCaller);
            CHECK(view->getLastEdit().offset == info.integer);
            edits->push_back({info.integer, view->getLastEdit().newValue, info.point.x, info.point.y});
        }
    } sink;
    sink.edits=&edits;
    s.view->onEdited.add(&sink, &Sink::edited);

    s.click(s.bytePoint(17, HexView::Column::Hex));
    s.type("a");
    CHECK(s.view->getPendingNibble() == 10);
    CHECK(s.writes.empty());
    CHECK(s.view->getCursor() == 17);
    CHECK(s.row(1)->getHexText().substr(3, 2) == "A1");
    CHECK(s.row(1)->getMarks().pendingNibble);
    s.type("B");
    CHECK(s.view->getPendingNibble() == -1);
    CHECK(s.writes == std::vector<std::tuple<size_t, uint8_t>>{{17, 0xab}});
    CHECK(edits == std::vector<std::tuple<size_t, size_t, int, int>>{{17, 0xab, 0x11, 0xab}});
    CHECK(s.view->getCursor() == 18);
    CHECK(s.row(1)->getHexText().substr(3, 5) == "AB 12");
    CHECK_FALSE(s.row(1)->getMarks().pendingNibble);

    // not hex, and Escape with nothing half-typed, are not taken
    CHECK_FALSE(static_cast<controls::Base *>(s.view)->onChar('g'));
    CHECK_FALSE(static_cast<controls::Base *>(s.view)->onKeyEscape(true));
    s.type("7");
    CHECK(s.row(1)->getHexText().substr(6, 2) == "72");
    s.key(keys::Escape);
    CHECK(s.view->getPendingNibble() == -1);
    CHECK(s.row(1)->getHexText().substr(6, 2) == "12");
    CHECK(s.view->getCursor() == 18);
    // a cursor move drops it too
    s.type("c");
    s.key(keys::Right);
    CHECK(s.view->getPendingNibble() == -1);
    CHECK(s.row(1)->getHexText().substr(6, 2) == "12");
    CHECK(s.writes.size() == 1);

    // the last byte is written and the cursor stays on it
    s.view->setCursor(63);
    s.type("0f");
    CHECK(s.writes.back() == std::tuple<size_t, uint8_t>{63, 0x0f});
    CHECK(s.view->getCursor() == 63);

    // the ASCII column writes a typed character
    s.click(s.bytePoint(32, HexView::Column::Ascii));
    s.type("Hi");
    CHECK(s.writes.size() == 4);
    CHECK(s.writes[2] == std::tuple<size_t, uint8_t>{32, 'H'});
    CHECK(s.writes[3] == std::tuple<size_t, uint8_t>{33, 'i'});
    CHECK(s.row(2)->getAsciiText().substr(0, 2) == "Hi");
    CHECK(s.row(2)->getHexText().substr(0, 5) == "48 69");
    CHECK(s.view->getCursor() == 34);
    CHECK(edits.size() == 4);
    s.checkOnePass();
}

TEST_CASE("HexView: a view with no write callback refuses typing")
{
    Scene s(64);
    REQUIRE(s.view->isReadOnly());
    int fired=0;
    struct Sink: event::Handler
    {
        int *fired;
        void edited(event::Info) { ++*fired; }
    } sink;
    sink.fired=&fired;
    s.view->onEdited.add(&sink, &Sink::edited);
    s.click(s.bytePoint(5, HexView::Column::Hex));
    const String hex=s.row(0)->getHexText();
    CHECK_FALSE(static_cast<controls::Base *>(s.view)->onChar('a'));
    s.canvas->inputCharacter('a');
    s.frame();
    CHECK(s.view->getPendingNibble() == -1);
    CHECK(s.row(0)->getHexText() == hex);
    s.click(s.bytePoint(5, HexView::Column::Ascii));
    CHECK_FALSE(static_cast<controls::Base *>(s.view)->onChar('Z'));
    s.canvas->inputCharacter('Z');
    s.frame();
    CHECK(s.view->getCursor() == 5);
    CHECK(fired == 0);
    CHECK(s.writes.empty());
}

TEST_CASE("HexView: cursor selection and highlight marks touch only rows they change")
{
    Scene s(4096);
    using controls::StateChange_Background;
    s.view->setCursor(20);
    s.frame();
    const auto marked=[&s]()
        {
            std::set<size_t> rows;
            for(auto *row : s.view->getRows())
            {
                if(!row->hidden() && (row->getStateChange() & StateChange_Background))
                    rows.insert(s.view->itemForRow(row));
            }
            return rows;
        };

    CHECK(s.row(1)->getMarks().cursor == 4);
    // a one-byte selection is the cursor's box alone
    CHECK(s.row(1)->getMarks().selectionBegin == 0);
    CHECK(s.row(1)->getMarks().selectionEnd == 0);

    // within a row: that row alone
    s.view->setCursor(22);
    CHECK(marked() == std::set<size_t>{1});
    s.frame();
    // to another row: the two rows
    s.view->setCursor(70);
    CHECK(marked() == std::set<size_t>{1, 4});
    s.frame();
    CHECK(s.row(1)->getMarks().cursor == -1);
    CHECK(s.row(4)->getMarks().cursor == 6);
    // extending down three rows marks those whose pieces changed
    s.view->setCursor(118, true);
    CHECK(marked() == std::set<size_t>{4, 5, 6, 7});
    s.frame();
    CHECK(s.row(5)->getMarks().selectionBegin == 0);
    CHECK(s.row(5)->getMarks().selectionEnd == 16);
    CHECK(s.row(7)->getMarks().selectionEnd == 7);
    // one byte further: only the cursor's row
    s.view->setCursor(119, true);
    CHECK(marked() == std::set<size_t>{7});
    s.frame();
    // nothing changed: nothing marked, and a settled frame does nothing
    s.view->setCursor(119, true);
    CHECK(marked().empty());
    s.frame();
    CHECK(controls::getChangeStats().controls == 0);

    // highlights: clipped per row, at most MaxHighlights, the earliest kept
    s.view->addHighlight(30, 40, 1);
    CHECK(marked() == std::set<size_t>{1, 2});
    s.frame();
    CHECK(s.row(1)->getMarks().highlightCount == 1);
    CHECK(s.row(1)->getMarks().highlights[0].begin == 14);
    CHECK(s.row(1)->getMarks().highlights[0].end == 16);
    CHECK(s.row(1)->getMarks().highlights[0].surface == 1);
    CHECK(s.row(2)->getMarks().highlights[0].end == 8);
    for(unsigned i=0; i < 5; ++i)
        s.view->addHighlight(48+i, 50+i, i+6);
    s.frame();
    CHECK(s.row(3)->getMarks().highlightCount == HexRowMarks::MaxHighlights);
    CHECK(s.row(3)->getMarks().highlights[0].surface == 2);
    CHECK(s.row(3)->getMarks().highlights[3].begin == 3);
    s.view->clearHighlights();
    CHECK(marked() == std::set<size_t>{1, 2, 3});
    s.frame();
    CHECK(s.row(3)->getMarks().highlightCount == 0);
    s.checkOnePass();
}

TEST_CASE("HexView: preview values read every type in both byte orders")
{
    using T=HexView::PreviewType;
    const uint8_t bytes[8]={0xff, 0x01, 0x80, 0x3f, 0x00, 0x00, 0xf0, 0xbf};
    const auto value=[&bytes](T type, bool big, size_t available=8)
        {
            return HexView::previewValue(type, bytes, available, big);
        };

    CHECK(value(T::Int8, false) == "-1");
    CHECK(value(T::UInt8, false) == "255");
    CHECK(value(T::Int8, true) == "-1");
    CHECK(value(T::Int16, false) == "511");
    CHECK(value(T::UInt16, false) == "511");
    CHECK(value(T::Int16, true) == "-255");
    CHECK(value(T::UInt16, true) == "65281");
    CHECK(value(T::Int32, false) == "1065353727");
    CHECK(value(T::UInt32, true) == "4278288447");
    CHECK(value(T::Int32, true) == "-16678849");
    CHECK(value(T::UInt64, false) == "13830554456720146943");
    CHECK(value(T::Int64, false) == "-4616189616989404673");
    CHECK(value(T::UInt64, true) == "18375108962719690943");
    CHECK(value(T::Int64, true) == "-71635110989860673");
    // 0x3f8001ff little endian is 1.00005996, 0xbff00000_3f8001ff a double
    CHECK(value(T::Float, false) == "1.00006092");
    CHECK(value(T::Double, false) == "-1.0000002365560474");
    const uint8_t one[8]={0x3f, 0xf0, 0, 0, 0, 0, 0, 0};
    CHECK(HexView::previewValue(T::Double, one, 8, true) == "1");
    CHECK(HexView::previewValue(T::Float, one, 8, true) == "1.875");
    // the data ends first
    CHECK(value(T::Int8, false, 0) == "n/a");
    CHECK(value(T::UInt8, false, 1) == "255");
    CHECK(value(T::Int16, false, 1) == "n/a");
    CHECK(value(T::Float, true, 3) == "n/a");
    CHECK(value(T::Int64, false, 7) == "n/a");
    CHECK(value(T::Double, true, 7) == "n/a");
    CHECK(std::string(HexView::previewName(T::UInt32)) == "uint32");
    CHECK(HexView::previewSize(T::Double) == 8);
}

TEST_CASE("HexView: the preview follows the cursor, the data's end and the endian toggle")
{
    using T=HexView::PreviewType;
    Scene s(20, 16, false, true);
    s.overrides={{16, 0x34}, {17, 0x12}, {18, 0xff}, {19, 0x7f}};
    s.view->dataChanged(16, 20);
    s.frame();
    CHECK(s.view->isPreviewVisible());
    controls::internal::HexViewPreview *preview=s.view->getPreview();
    // in the view's bottom padding, under the rows
    CHECK(preview->getY() >= s.view->getViewport().y+s.view->getViewport().h);
    CHECK(preview->bottom() == s.view->getHeight()-HexView::PreviewGap);
    CHECK(preview->getX() == s.view->getAddressX());
    CHECK(s.view->getPreviewText(T::UInt8) == "0");
    CHECK(s.view->getPreviewText(T::UInt32) == "50462976");

    s.view->setCursor(16);
    s.frame();
    CHECK(s.view->getPreviewText(T::UInt16) == "4660");
    CHECK(s.view->getPreviewText(T::Int32) == "2147422772");
    CHECK(s.view->getPreviewText(T::Int64) == "n/a");
    CHECK(s.view->getPreviewText(T::Double) == "n/a");
    // a move whose values keep their strings marks none of them
    s.view->setCursor(18);
    s.frame();
    CHECK(s.view->getPreviewText(T::Int8) == "-1");
    s.view->setCursor(19);
    s.frame();
    CHECK(s.view->getPreviewText(T::Int16) == "n/a");
    s.view->setCursor(18);
    s.frame();
    s.overrides[19]=0x7f;
    s.view->dataChanged(0, 1);
    for(size_t i=0; i < HexView::PreviewTypes; ++i)
        CHECK(preview->getValueControl(i)->getStateChange() == controls::StateChange_Nothing);
    s.view->setCursor(16);
    // only the values whose strings changed are marked: int64 stays n/a
    CHECK(preview->getValueControl(size_t(T::Int64))->getStateChange() == controls::StateChange_Nothing);
    CHECK(preview->getValueControl(size_t(T::UInt16))->getStateChange() != controls::StateChange_Nothing);
    s.frame();

    // the toggle reads big endian; an edit of the cursor's bytes reads again
    s.click(preview->getEndianButton()->localPosToCanvas(Point(3, 3)));
    CHECK(s.view->isBigEndian());
    CHECK(preview->getEndianButton()->getText() == "Big endian");
    CHECK(s.view->getPreviewText(T::UInt16) == "13330");
    CHECK(s.view->getPreviewText(T::UInt32) == "873660287");
    s.overrides[17]=0x00;
    s.view->dataChanged(17, 18);
    s.frame();
    CHECK(s.view->getPreviewText(T::UInt16) == "13312");
    s.click(preview->getEndianButton()->localPosToCanvas(Point(3, 3)));
    CHECK_FALSE(s.view->isBigEndian());
    CHECK(s.view->getPreviewText(T::UInt16) == "52");

    // hidden, its room goes and it reads nothing; shown, it reads again
    s.view->setPreviewVisible(false);
    s.frame();
    CHECK(s.view->getPadding().bottom == 0);
    s.clearReads();
    s.view->setCursor(0);
    CHECK(s.reads.empty());
    s.view->setPreviewVisible(true);
    CHECK(s.view->getPreviewText(T::UInt8) == "0");
    s.frame();
    s.checkOnePass();
}

TEST_CASE("HexView: the hovered byte marks its row in both columns and follows a scroll")
{
    Scene s(4096);
    s.move(s.bytePoint(0x23, HexView::Column::Ascii));
    s.frame();
    CHECK(s.view->getHoveredByte() == 0x23);
    CHECK(s.row(2)->getMarks().hover == 3);
    CHECK(s.row(1)->getMarks().hover == -1);
    // a gap hovers nothing
    Point gap=s.bytePoint(0x23, HexView::Column::Hex);
    gap.x+=static_cast<int>(2*s.view->getCellWidth());
    s.move(gap);
    CHECK(s.view->getHoveredByte() == HexView::NoByte);
    CHECK((s.row(2)->getStateChange() & controls::StateChange_Background) != 0);
    s.frame();
    // another row's byte: only those two rows marked
    s.move(s.bytePoint(0x23, HexView::Column::Hex));
    s.frame();
    s.move(s.bytePoint(0x45, HexView::Column::Hex));
    std::set<size_t> marked;
    for(auto *row : s.view->getRows())
    {
        if(!row->hidden() && (row->getStateChange() & controls::StateChange_Background))
            marked.insert(s.view->itemForRow(row));
    }
    CHECK(marked == std::set<size_t>{2, 4});
    s.frame();
    // a scroll under the resting mouse: the byte under it now
    s.view->scrollByRows(1);
    s.frame();
    CHECK(s.view->getHoveredByte() == 0x55);
    CHECK(s.row(5)->getMarks().hover == 5);
    // leaving clears it
    s.move(Point(1399, 599));
    s.frame();
    CHECK(s.view->getHoveredByte() == HexView::NoByte);
    CHECK(s.row(5)->getMarks().hover == -1);
    s.checkOnePass();
}

TEST_CASE("HexView: keys only while focused, the pending nibble dropped with the focus")
{
    Scene s(4096, 16, true, true);
    s.view->setSource(4096, s.reader(), s.writer());
    s.frame();
    s.click(s.bytePoint(0, HexView::Column::Hex));
    REQUIRE(KeyboardFocus == s.view);
    REQUIRE(s.view->getCursor() == 0);

    // a single-line TextBox passes Up, Down, PageUp and PageDown on to its
    // parents; the view takes none of them from the goto box
    s.view->getGotoBox()->focus();
    s.frame();
    for(int key : {keys::Down, keys::PageDown, keys::Up, keys::PageUp})
    {
        s.key(key);
        CHECK(s.view->getCursor() == 0);
    }
    CHECK(KeyboardFocus == s.view->getGotoBox());
    // nor from the preview's endian button
    s.view->getPreview()->getEndianButton()->focus();
    s.frame();
    for(int key : {keys::Down, keys::Right, keys::End})
    {
        s.key(key);
        CHECK(s.view->getCursor() == 0);
    }

    // a half-typed byte is dropped as the focus leaves
    s.click(s.bytePoint(2, HexView::Column::Hex));
    s.type("4");
    REQUIRE(s.view->getPendingNibble() == 4);
    REQUIRE(s.row(0)->getHexText().substr(6, 2) == "42");
    s.view->getGotoBox()->focus();
    s.frame();
    CHECK(s.view->getPendingNibble() == -1);
    CHECK(s.row(0)->getHexText().substr(6, 2) == "02");
    CHECK_FALSE(s.row(0)->getMarks().pendingNibble);
    // and a digit typed once it is back starts a new byte
    s.view->focus();
    s.type("5");
    CHECK(s.view->getPendingNibble() == 5);
    CHECK(s.writes.empty());
}

TEST_CASE("HexView: a drag inside one byte and a key at a limit lay nothing out")
{
    Scene s(4096);
    s.click(s.bytePoint(32, HexView::Column::Hex));
    REQUIRE(s.view->getCursor() == 32);
    const Point at=s.bytePoint(32, HexView::Column::Hex);

    s.move(at);
    input::onMouseClicked(s.canvas.get(), 0, true);
    s.frame();
    s.frame();
    int passes=0;
    for(int i=0; i < 4; ++i)
    {
        s.canvas->inputMouseMoved(at.x+(i%2), at.y, 1, 0);
        s.frame();
        passes+=s.canvas->getLayoutStats().passes;
    }
    input::onMouseClicked(s.canvas.get(), 0, false);
    s.frame();
    CHECK(s.view->getCursor() == 32);
    CHECK(passes == 0);

    // Left at the first byte, End at the last, repeated
    s.view->setCursor(0);
    s.frame();
    s.frame();
    s.key(keys::Left);
    CHECK(s.canvas->getLayoutStats().passes == 0);
    s.view->setCursor(4095);
    s.frame();
    s.frame();
    REQUIRE(s.view->getCursor() == 4095);
    s.key(keys::End);
    CHECK(s.canvas->getLayoutStats().passes == 0);
    s.key(keys::Right);
    CHECK(s.canvas->getLayoutStats().passes == 0);

    // the cursor scrolled out of view: a key that moves nothing brings it back
    s.view->setVerticalScrollOffset(0);
    s.frame();
    s.frame();
    REQUIRE(s.view->rowForItem(4095/16) == nullptr);
    s.key(keys::End);
    s.frame();
    CHECK(s.view->rowForItem(4095/16) != nullptr);
}
