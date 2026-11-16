/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * MIT License, see LICENSE
 *
 * The textured skin's HexView look: each shown row's fixed slots per column
 * (cursor, hover, selection, MaxHighlights highlights), drawn at the view's
 * cell arithmetic in the row's units, highlight < selection < hover < cursor
 * in depth (z+0..z+3) and all behind the row's Texts, each in its theme
 * surface, the absent pieces hidden; a pending nibble narrows the hex cursor
 * to the low digit. The hex and ASCII runs carry colour spans for the zero
 * bytes and the non-printable bytes in Colors.HexView, which a theme edit
 * (updateColors) reaches, and the three runs take the column colours.
 */
#include "headlessFixture.h"

#include <gweni/controls/hexView.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <map>
#include <set>
#include <vector>

using namespace gweni;
namespace tn=skin::textured::tokyonight;
using controls::HexView;
using controls::HexViewRow;

namespace
{

//! Each slot's last rounded rect, its fill and z, and whether its last call
//! hid it.
class SlotRecorder: public test::StubRenderer
{
public:
    using test::StubRenderer::drawRoundedRect;

    struct Drawn
    {
        Rect rect;
        Color top;
        Color border;
        int z=0;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex) override
    {
        drawn[primitiveId]={rect, top, borderColor, zIndex};
        hidden.erase(primitiveId);
        test::StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom,
            borderColor, zIndex);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.insert(primitiveId);
        test::StubRenderer::hidePrimitive(primitiveId);
    }

    std::map<size_t, Drawn> drawn;
    std::set<size_t> hidden;
};

bool same(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

struct Scene
{
    SlotRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    HexView *view=nullptr;
    std::vector<uint8_t> data;

    explicit Scene(size_t size)
    {
        data.resize(size);
        for(size_t i=0; i < size; ++i)
            data[i]=static_cast<uint8_t>(i);
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1000, 500);
        view=canvas->newChild<HexView>();
        view->setBounds(10, 10, 900, 300);
        view->setGotoVisible(false);
        view->setPreviewVisible(false);
        view->setSource(size, [this](size_t offset, uint8_t *buffer, size_t count)
            {
                std::copy(data.begin()+offset, data.begin()+offset+count, buffer);
            });
        frame();
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    HexViewRow *row(size_t item) const
    {
        auto *row=gweni_cast<HexViewRow>(view->rowForItem(item));
        REQUIRE(row != nullptr);
        return row;
    }

    //! The row's slot for a piece: column 0 hex, 1 ASCII; piece 0 cursor,
    //! 1 hover, 2 selection, 3.. highlights.
    size_t slot(HexViewRow *row, size_t column, size_t piece) const
    {
        return row->getSkinControl()->getPrimitives().at(
            column*skin::textured::HexViewRow::SlotsPerColumn+piece);
    }

    //! Checks the slot is drawn over the row's bytes [begin, end) of the
    //! column, at the row's z plus depth, in the surface.
    void checkPiece(HexViewRow *row, size_t column, size_t piece, size_t begin, size_t end,
        const tn::Surface &surface, int depth)
    {
        const size_t id=slot(row, column, piece);
        const auto col=column == 0?HexView::Column::Hex:HexView::Column::Ascii;
        int x=0;
        int width=0;

        view->byteSpan(begin, end, col, x, width);
        INFO("column ", column, " piece ", piece);
        REQUIRE(renderer.drawn.count(id) == 1);
        CHECK(renderer.hidden.count(id) == 0);
        const SlotRecorder::Drawn &drawn=renderer.drawn[id];
        const Rect bounds=row->getRenderBounds();
        CHECK(drawn.rect.x == bounds.x+x);
        CHECK(drawn.rect.w == width);
        CHECK(drawn.rect.y == bounds.y);
        CHECK(drawn.rect.h == bounds.h);
        CHECK(drawn.z == row->getZIndex()+depth);
        CHECK(same(drawn.top, surface.top));
    }
};

}//namespace

TEST_CASE("HexView look: the row's pieces are its cells in their layers and surfaces")
{
    Scene s(4096);
    // a cursor at 0x12, the selection back to 0x0e, a highlight over the
    // row's bytes 4..9 and the mouse on byte 0x15
    s.view->addHighlight(0x14, 0x1a, 2);
    s.view->setCursor(0x0e);
    s.view->setCursor(0x12, true);
    s.frame();
    HexViewRow *row=s.row(1);
    const Point at=s.view->localPosToCanvas(Point(s.view->getViewport().x+s.view->byteX(5, HexView::Column::Hex)+1,
        s.view->getViewport().y+s.view->getRowHeight()+s.view->getRowHeight()/2));
    s.canvas->inputMouseMoved(at.x, at.y, 0, 0);
    s.frame();
    REQUIRE(s.view->getHoveredByte() == 0x15);

    for(size_t column=0; column < 2; ++column)
    {
        s.checkPiece(row, column, 0, 2, 3, tn::HexViewCursor, 3);
        s.checkPiece(row, column, 1, 5, 6, tn::HexViewHover, 2);
        s.checkPiece(row, column, 2, 0, 3, tn::HexViewSelection, 1);
        s.checkPiece(row, column, 3, 4, 10, tn::HexViewHighlight2, 0);
        for(size_t piece=4; piece < skin::textured::HexViewRow::SlotsPerColumn; ++piece)
            CHECK(s.renderer.hidden.count(s.slot(row, column, piece)) == 1);
    }
    // the previous row holds the selection's start and nothing else
    HexViewRow *above=s.row(0);
    CHECK(s.renderer.hidden.count(s.slot(above, 0, 0)) == 1);
    CHECK(s.renderer.hidden.count(s.slot(above, 0, 1)) == 1);
    s.checkPiece(above, 1, 2, 14, 16, tn::HexViewSelection, 1);
    // every row's Texts in front of its four layers
    for(controls::internal::Text *text : {row->getAddressControl(), row->getHexControl(), row->getAsciiControl()})
        CHECK(text->getZIndex() > row->getZIndex()+3);

    // the mouse off: the hover slots hidden
    s.canvas->inputMouseMoved(999, 499, 0, 0);
    s.frame();
    CHECK(s.view->getHoveredByte() == HexView::NoByte);
    CHECK(s.renderer.hidden.count(s.slot(row, 0, 1)) == 1);
    CHECK(s.renderer.hidden.count(s.slot(row, 1, 1)) == 1);
}

TEST_CASE("HexView look: a pending nibble's cursor box is the low digit's cell")
{
    Scene s(64);
    s.view->setSource(64, [&s](size_t offset, uint8_t *buffer, size_t count)
        {
            std::copy(s.data.begin()+offset, s.data.begin()+offset+count, buffer);
        }, [&s](size_t offset, uint8_t byte) { s.data[offset]=byte; });
    s.view->setCursor(3);
    s.view->focus();
    s.frame();
    s.canvas->inputCharacter('a');
    s.frame();
    HexViewRow *row=s.row(0);
    const size_t id=s.slot(row, 0, 0);
    int x=0;
    int width=0;
    s.view->cursorSpan(3, HexView::Column::Hex, true, x, width);
    CHECK(s.renderer.drawn[id].rect.x == row->getRenderBounds().x+x);
    CHECK(s.renderer.drawn[id].rect.w == width);
    int wholeX=0;
    int wholeWidth=0;
    s.view->byteSpan(3, 4, HexView::Column::Hex, wholeX, wholeWidth);
    CHECK(x > wholeX);
    CHECK(x+width == wholeX+wholeWidth);
    // the ASCII column's box stays the byte's
    s.checkPiece(row, 1, 0, 3, 4, tn::HexViewCursor, 3);
}

TEST_CASE("HexView look: zero and non-printable bytes are colour spans in the skin's colours")
{
    Scene s(64);
    s.data[0]=0;
    s.data[1]=0;
    s.data[2]='A';
    s.data[7]=0;
    s.data[8]=0;
    s.data[9]=0x7f;
    s.data[10]='.';
    s.view->dataChanged(0, 16);
    s.frame();
    HexViewRow *row=s.row(0);
    const auto &colors=s.skin.Colors.HexView;
    auto *hex=static_cast<controls::internal::HexViewText *>(row->getHexControl());
    auto *ascii=static_cast<controls::internal::HexViewText *>(row->getAsciiControl());

    // bytes 0-1 (cells 0..5), byte 7 and 8 across the group gap (cells 21..27)
    const auto &hexSpans=hex->getColorSpans();
    REQUIRE(hexSpans.size() == 2);
    CHECK(hexSpans[0].begin == 0);
    CHECK(hexSpans[0].end == 5);
    CHECK(hexSpans[1].begin == 21);
    CHECK(hexSpans[1].end == 27);
    CHECK(same(hexSpans[0].color, colors.ZeroByte));
    CHECK(hex->getText().substr(21, 6) == "00  00");
    // every byte but 'A' and '.' is non-printable here (0x03..0x06 too)
    const auto &asciiSpans=ascii->getColorSpans();
    REQUIRE(asciiSpans.size() == 3);
    CHECK(asciiSpans[0].begin == 0);
    CHECK(asciiSpans[0].end == 2);
    CHECK(asciiSpans[1].begin == 3);
    CHECK(asciiSpans[1].end == 10);
    CHECK(asciiSpans[2].begin == 11);
    CHECK(asciiSpans[2].end == 16);
    CHECK(same(asciiSpans[0].color, colors.NonPrintable));
    // the runs' own colours
    CHECK(same(row->getAddressControl()->getTextColor(), colors.Address));
    CHECK(same(hex->getTextColor(), colors.Byte));
    CHECK(same(ascii->getTextColor(), colors.Ascii));
    // the renderer drew the spans
    const size_t textId=hex->getSkinControl()->getPrimitives().at(0);
    CHECK(s.renderer.textSpans[textId] == hexSpans);

    // a theme edit: the spans take the new colour
    s.skin.Colors.HexView.ZeroByte=Color(1, 2, 3);
    row->updateColors();
    CHECK(same(hex->getColorSpans()[0].color, Color(1, 2, 3)));
    CHECK((hex->getStateChange() & controls::StateChange_Text) != 0);
    s.frame();
    CHECK(same(s.renderer.textSpans[textId][0].color, Color(1, 2, 3)));
}
