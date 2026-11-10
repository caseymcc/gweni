/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  drawRoundedRect's edge mask (renderer::RoundedEdge) without a GL context.
 *
 *  renderer::Base's default for the masked call draws the unmasked one with the
 *  same arguments, the mask dropped, so a backend without it draws today's
 *  rounded rect; the StubRenderer records each id's last mask (0 for a call
 *  without one) and still counts the draw through the call without a mask.
 *
 *  SlotRenderer's packing (with the OpenGL core renderer, which SlotRenderer is
 *  part of; a SlotRenderer whose backend hooks do nothing): Vertex::radius holds
 *  the radius in device px in its low byte, at most 255, and the mask in its
 *  high byte, on all four vertexes. A call without a mask, a mask of 0 and
 *  drawBilinearRect/drawCheckerRect leave the high byte 0, and a mask of 0 is the
 *  unmasked call's slot byte for byte. Drawing the same mask again writes
 *  nothing; changing only the mask rewrites that slot alone. The pixels are
 *  checked on every backend in tests/roundedEdges.cpp.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

#include "mirrorOnlyRenderer.h"

#include <cstdint>
#include <cstring>
#include <vector>

using namespace gweni;

namespace
{

//! Records the unmasked draws renderer::Base's masked default makes.
class UnmaskedRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        Rect rect;
        float radius, border;
        Color top, bottom, borderColor;
        int zIndex;
    };

    using test::StubRenderer::drawRoundedRect;

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex) override
    {
        draws.push_back({primitiveId, rect, radius, border, top, bottom, borderColor, zIndex});
        test::StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom,
            borderColor, zIndex);
    }

    std::vector<Draw> draws;
};

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

}//namespace

TEST_CASE("renderer::Base's masked drawRoundedRect draws the unmasked one, the mask dropped")
{
    UnmaskedRecorder renderer;
    const Color top(10, 20, 30, 255);
    const Color bottom(40, 50, 60, 255);
    const Color border(70, 80, 90, 255);
    const uint8_t edges=gweni::renderer::SquareBottomLeft | gweni::renderer::SquareBottomRight
        | gweni::renderer::NoBorderBottom;

    renderer.gweni::renderer::Base::drawRoundedRect(7, Rect(1, 2, 30, 20), 6.0f, 1.0f, top, bottom,
        border, 4, edges);
    REQUIRE(renderer.draws.size() == 1);

    const UnmaskedRecorder::Draw &draw=renderer.draws[0];

    CHECK(draw.id == 7);
    CHECK(draw.rect.x == 1);
    CHECK(draw.rect.y == 2);
    CHECK(draw.rect.w == 30);
    CHECK(draw.rect.h == 20);
    CHECK(draw.radius == 6.0f);
    CHECK(draw.border == 1.0f);
    CHECK(sameColor(draw.top, top));
    CHECK(sameColor(draw.bottom, bottom));
    CHECK(sameColor(draw.borderColor, border));
    CHECK(draw.zIndex == 4);
}

TEST_CASE("StubRenderer records each id's edge mask and counts the masked draw")
{
    UnmaskedRecorder renderer;
    const uint8_t edges=gweni::renderer::SquareTopLeft | gweni::renderer::NoBorderLeft;

    renderer.drawRoundedRect(3, Rect(0, 0, 10, 10), 2.0f, 1.0f, Color(), Color(), Color(), 1,
        edges);
    CHECK(renderer.roundedEdges.at(3) == edges);
    // through the call without a mask, which a test's subclass records
    CHECK(renderer.draws.size() == 1);
    renderer.end();
    CHECK(renderer.frameStats().primitiveSlotsWritten == 1);

    renderer.drawRoundedRect(3, Rect(0, 0, 10, 10), 2.0f, 1.0f, Color(), Color(), Color(), 1);
    CHECK(renderer.roundedEdges.at(3) == 0);
}

#ifdef GWENI_TEST_SLOT_RENDERER

namespace
{

//! The mirror of a SlotRenderer whose backend hooks do nothing.
class MirrorOnly: public test::MirrorOnlyRenderer
{
public:
    //! A live slot's bytes as the mirror holds them (snapshotMirror()).
    std::vector<uint8_t> slotBytes(size_t id) const
    {
        const MirrorSnapshot snapshot=snapshotMirror();

        return snapshot.primitives.at(id);
    }

    //! Vertex i's radius field (the 9th of its 16 16-bit fields).
    unsigned radius(size_t id, size_t i) const
    {
        const std::vector<uint8_t> bytes=slotBytes(id);
        uint16_t field[16];

        REQUIRE(bytes.size() == 4*sizeof(field));
        std::memcpy(field, bytes.data()+i*sizeof(field), sizeof(field));
        return field[8];
    }

    //! One frame drawing `draw`, and the slots it wrote.
    template<typename Draw>
    size_t frame(Draw &&draw)
    {
        begin();
        draw();
        end();
        return getFrameStats().primitiveSlotsWritten;
    }
};

//! Primitive ids taken from the process's bank for one case, and given back.
struct LiveIds
{
    explicit LiveIds(size_t count)
    {
        for(size_t i=0; i<count; ++i)
            ids.push_back(getPrimitiveId(this));
    }

    ~LiveIds()
    {
        for(size_t id : ids)
            releasePrimitiveId(id);
    }

    size_t operator[](size_t i) const { return ids[i]; }

    std::vector<size_t> ids;
};

//! Every vertex of the slot holds `expected` in its radius field.
void checkRadius(const MirrorOnly &renderer, size_t id, unsigned expected)
{
    for(size_t i=0; i<4; ++i)
    {
        CAPTURE(i);
        CHECK(renderer.radius(id, i) == expected);
    }
}

}//namespace

TEST_CASE("SlotRenderer packs the edge mask in Vertex::radius's high byte, over a radius of at most 255 px")
{
    MirrorOnly renderer;
    LiveIds ids(2);
    const Rect rect(10, 10, 60, 30);
    const Color fill(30, 40, 50, 255);
    const Color border(90, 100, 110, 255);
    const uint8_t edges=gweni::renderer::SquareBottomLeft | gweni::renderer::SquareBottomRight
        | gweni::renderer::NoBorderBottom;

    renderer.setScale(1.0f);

    SUBCASE("without a mask the high byte is 0")
    {
        renderer.frame([&] { renderer.drawRoundedRect(ids[0], rect, 6.0f, 1.0f, fill, fill, border, 1); });
        checkRadius(renderer, ids[0], 6);
    }

    SUBCASE("a mask is the high byte, the radius the low one")
    {
        renderer.frame([&] { renderer.drawRoundedRect(ids[0], rect, 6.0f, 1.0f, fill, fill, border, 1, edges); });
        checkRadius(renderer, ids[0], (unsigned(edges) << 8) | 6u);

        renderer.frame([&] { renderer.drawRoundedRect(ids[0], rect, 6.0f, 1.0f, fill, fill, border, 1, 0xff); });
        checkRadius(renderer, ids[0], 0xff06u);
    }

    SUBCASE("the radius is in device px, at most 255, under the mask")
    {
        renderer.setScale(2.0f);
        renderer.frame([&] { renderer.drawRoundedRect(ids[0], rect, 6.0f, 1.0f, fill, fill, border, 1, edges); });
        checkRadius(renderer, ids[0], (unsigned(edges) << 8) | 12u);

        renderer.setScale(1.0f);
        renderer.frame([&] { renderer.drawRoundedRect(ids[0], rect, 300.0f, 1.0f, fill, fill, border, 1, edges); });
        checkRadius(renderer, ids[0], (unsigned(edges) << 8) | 255u);
        renderer.frame([&] { renderer.drawRoundedRect(ids[0], rect, 300.0f, 1.0f, fill, fill, border, 1); });
        checkRadius(renderer, ids[0], 255u);
    }

    SUBCASE("a mask of 0 is the unmasked slot, byte for byte")
    {
        renderer.frame([&]
        {
            renderer.drawRoundedRect(ids[0], rect, 6.0f, 1.0f, fill, fill, border, 1);
            renderer.drawRoundedRect(ids[1], rect, 6.0f, 1.0f, fill, fill, border, 1, 0);
        });
        CHECK(renderer.slotBytes(ids[0]) == renderer.slotBytes(ids[1]));
    }

    SUBCASE("the bilinear and checker rects have no mask")
    {
        renderer.frame([&]
        {
            renderer.drawBilinearRect(ids[0], rect, 1, fill, fill, fill, fill, 6.0f, 1.0f, border);
            renderer.drawCheckerRect(ids[1], rect, 1, fill, fill, fill, 6.0f, 1.0f, border);
        });
        checkRadius(renderer, ids[0], 6);
        checkRadius(renderer, ids[1], 6);
    }

    SUBCASE("the same mask again writes nothing; another mask rewrites that slot alone")
    {
        auto draw=[&](uint8_t first)
        {
            renderer.drawRoundedRect(ids[0], rect, 6.0f, 1.0f, fill, fill, border, 1, first);
            renderer.drawRoundedRect(ids[1], rect, 6.0f, 1.0f, fill, fill, border, 1, edges);
        };

        CHECK(renderer.frame([&] { draw(edges); }) == 2);
        CHECK(renderer.frame([&] { draw(edges); }) == 0);
        CHECK(renderer.frame([&] { draw(gweni::renderer::NoBorderTop); }) == 1);
        CHECK(renderer.writtenPrimitiveIds() == std::vector<size_t>{ids[0]});
    }
}

#endif//GWENI_TEST_SLOT_RENDERER
