/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  renderer::Base::drawLine without a GL context: the default a backend without
 *  a line primitive gets (one filled rect along the segment's longer axis,
 *  round(thickness) units across, centred on its middle) and the StubRenderer's
 *  record of every call, which control tests check their segments against.
 *  OpenGLCore's capsule (style 3) is checked pixel by pixel in tests/lines.cpp.
 *
 *  The default's rect starts across where OpenGLCore's stroke starts, its
 *  fractional endpoints are rounded, a zero-length segment is one unit long,
 *  and the neighbour points change nothing in it.
 *
 *  SlotRenderer's packing of a segment (with the OpenGL core renderer, which
 *  SlotRenderer is part of; a SlotRenderer whose backend hooks do nothing):
 *  without neighbours, a slot's vertexes hold what they did before neighbours
 *  existed (radius, bx/by 0, dx/dy the second endpoint, the negated first);
 *  a neighbour is its point placed as an endpoint at the segment's stroke, in
 *  half px from the quad's centre, with its LineHasPrev/LineHasNext bit in
 *  radius, and one 16383.5 px or more from the centre is left out. Drawing a
 *  segment again with the same neighbours writes nothing, and changing,
 *  adding or dropping a neighbour rewrites that slot alone.
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

bool sameRect(const Rect &a, const Rect &b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

//! Records the filled rects renderer::Base's default drawLine draws.
class RectRecorder: public test::StubRenderer
{
public:
    struct Fill
    {
        size_t id;
        Rect rect;
        int zIndex;
    };

    void drawFilledRect(size_t primitiveId, Rect rect, int zIndex) override
    {
        fills.push_back({primitiveId, rect, zIndex});
    }

    //! The base class's drawLine, which the StubRenderer overrides to record.
    Fill defaultLine(PointF p0, PointF p1, float thickness, int zIndex=7)
    {
        fills.clear();
        renderer::Base::drawLine(42, p0, p1, thickness, zIndex);
        REQUIRE(fills.size() == 1);
        CHECK(fills[0].id == 42);
        CHECK(fills[0].zIndex == zIndex);
        return fills[0];
    }

    std::vector<Fill> fills;
};

}//namespace

TEST_CASE("renderer::Base::drawLine default: a filled rect along the longer axis")
{
    RectRecorder renderer;

    SUBCASE("a horizontal segment is its rect, the stroke starting at its row")
    {
        CHECK(sameRect(renderer.defaultLine(PointF(10, 20), PointF(50, 20), 1.0f).rect, Rect(10, 20, 40, 1)));
        CHECK(sameRect(renderer.defaultLine(PointF(50, 20), PointF(10, 20), 1.0f).rect, Rect(10, 20, 40, 1)));
        // 3 across: one row each side of the middle one
        CHECK(sameRect(renderer.defaultLine(PointF(10, 20), PointF(50, 20), 3.0f).rect, Rect(10, 19, 40, 3)));
        // 2 across: the rows either side of y
        CHECK(sameRect(renderer.defaultLine(PointF(10, 20), PointF(50, 20), 2.0f).rect, Rect(10, 19, 40, 2)));
    }

    SUBCASE("a vertical segment is its rect")
    {
        CHECK(sameRect(renderer.defaultLine(PointF(30, 5), PointF(30, 45), 1.0f).rect, Rect(30, 5, 1, 40)));
        CHECK(sameRect(renderer.defaultLine(PointF(30, 45), PointF(30, 5), 3.0f).rect, Rect(29, 5, 3, 40)));
    }

    SUBCASE("a slanted segment is approximated along its longer axis, centred on its middle")
    {
        // longer in x: its x extent, 2 across at the middle y (5)
        CHECK(sameRect(renderer.defaultLine(PointF(0, 0), PointF(40, 10), 2.0f).rect, Rect(0, 4, 40, 2)));
        CHECK(sameRect(renderer.defaultLine(PointF(40, 10), PointF(0, 0), 2.0f).rect, Rect(0, 4, 40, 2)));
        // longer in y
        CHECK(sameRect(renderer.defaultLine(PointF(10, 0), PointF(20, 30), 1.0f).rect, Rect(15, 0, 1, 30)));
        // 45 degrees counts as horizontal
        CHECK(sameRect(renderer.defaultLine(PointF(0, 0), PointF(10, 10), 1.0f).rect, Rect(0, 5, 10, 1)));
    }

    SUBCASE("fractional endpoints round to the nearest unit, the thickness to at least 1")
    {
        CHECK(sameRect(renderer.defaultLine(PointF(10.4f, 20.0f), PointF(49.6f, 20.0f), 1.0f).rect,
            Rect(10, 20, 40, 1)));
        CHECK(sameRect(renderer.defaultLine(PointF(10, 20), PointF(50, 20), 0.2f).rect, Rect(10, 20, 40, 1)));
        CHECK(sameRect(renderer.defaultLine(PointF(10, 20), PointF(50, 20), 2.6f).rect, Rect(10, 19, 40, 3)));
    }

    SUBCASE("a zero-length segment is one unit long")
    {
        CHECK(sameRect(renderer.defaultLine(PointF(10, 20), PointF(10, 20), 1.0f).rect, Rect(10, 20, 1, 1)));
    }

    SUBCASE("the neighbours change nothing")
    {
        const PointF prev(0, 0);
        const PointF next(60, 25);

        renderer.fills.clear();
        renderer.renderer::Base::drawLine(42, PointF(0, 0), PointF(40, 10), 2.0f, 7, &prev, &next);
        REQUIRE(renderer.fills.size() == 1);
        CHECK(sameRect(renderer.fills[0].rect, Rect(0, 4, 40, 2)));
    }
}

TEST_CASE("StubRenderer records every drawLine call with the draw colour it was made in")
{
    test::StubRenderer renderer;
    const Color red(255, 0, 0, 255);
    const Color green(0, 255, 0, 128);

    renderer.setDrawColor(red);
    renderer.drawLine(3, PointF(1.5f, 2.25f), PointF(10.0f, 20.0f), 1.5f, 11);
    renderer.setDrawColor(green);
    // a Point converts to a PointF
    renderer.drawLine(4, Point(5, 6), Point(7, 8), 1.0f, 12);

    REQUIRE(renderer.lines.size() == 2);

    const test::StubRenderer::LineDraw &first=renderer.lines[0];

    CHECK(first.id == 3);
    CHECK((first.p0 == PointF(1.5f, 2.25f)));
    CHECK((first.p1 == PointF(10.0f, 20.0f)));
    CHECK(first.thickness == 1.5f);
    CHECK(first.zIndex == 11);
    CHECK((first.color == red));

    const test::StubRenderer::LineDraw &second=renderer.lines[1];

    CHECK(second.id == 4);
    CHECK((second.p0 == PointF(5, 6)));
    CHECK((second.p1 == PointF(7, 8)));
    CHECK(second.zIndex == 12);
    CHECK((second.color == green));
    CHECK(!first.prev);
    CHECK(!first.next);

    // the neighbours, as given
    const PointF prev(0.5f, 1.0f);
    const PointF next(30.0f, 40.0f);

    renderer.drawLine(5, PointF(5, 6), PointF(7, 8), 1.0f, 12, &prev, &next);
    renderer.drawLine(6, PointF(7, 8), PointF(30, 40), 1.0f, 12, &prev);
    renderer.drawLine(7, PointF(7, 8), PointF(30, 40), 1.0f, 12, nullptr, &next);
    REQUIRE(renderer.lines.size() == 5);
    CHECK((renderer.lines[2].prev == prev));
    CHECK((renderer.lines[2].next == next));
    CHECK((renderer.lines[3].prev == prev));
    CHECK(!renderer.lines[3].next);
    CHECK(!renderer.lines[4].prev);
    CHECK((renderer.lines[4].next == next));
    // no fill is drawn for it, nor any id taken
    CHECK(renderer.events.empty());
}

#ifdef GWENI_TEST_SLOT_RENDERER

namespace
{

//! The mirror of a SlotRenderer whose backend hooks do nothing, read field by field.
class MirrorOnly: public test::MirrorOnlyRenderer
{
public:
    //! A slot's vertex i as the mirror holds it (snapshotMirror(), a live id): the
    //! 16-bit fields from lx on, in Vertex's order.
    struct Fields
    {
        int lx, ly;
        unsigned radius, border;
        int cx, cy, bx, by, dx, dy;
    };

    Fields fields(size_t id, size_t i) const
    {
        const std::vector<uint8_t> &bytes=snapshotMirror().primitives.at(id);
        uint16_t field[16];

        REQUIRE(bytes.size() == 4*sizeof(field));
        std::memcpy(field, bytes.data()+i*sizeof(field), sizeof(field));

        auto s=[&](int n) { return (int)(int16_t)field[n]; };

        return {s(6), s(7), field[8], field[9], s(10), s(11), s(12), s(13), s(14), s(15)};
    }

    static constexpr unsigned hasPrev=LineHasPrev;
    static constexpr unsigned hasNext=LineHasNext;

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

}//namespace

TEST_CASE("SlotRenderer::drawLine packs the neighbours beside the segment, and without them the slot is as before")
{
    MirrorOnly renderer;
    LiveIds ids(1);
    const size_t id=ids[0];

    renderer.setScale(1.0f);
    renderer.setDrawColor(Color(255, 0, 0, 128));

    // a 2 px stroke from (10,20) to (40,30): each endpoint's stroke starts at
    // floor(v - 0.5), so (9,19) and (39,29), the quad's centre at (25,25) px and
    // the endpoints -30,-10 and 30,10 half px from it
    const PointF p0(10, 20);
    const PointF p1(40, 30);

    SUBCASE("no neighbours: radius and bx/by 0, dx/dy the second endpoint")
    {
        renderer.frame([&] { renderer.drawLine(id, p0, p1, 2.0f, 1); });
        for(size_t i=0; i<4; ++i)
        {
            const MirrorOnly::Fields f=renderer.fields(id, i);

            CAPTURE(i);
            CHECK(f.radius == 0);
            CHECK(f.border == 2);
            CHECK(f.cx == -30);
            CHECK(f.cy == -10);
            CHECK(f.bx == 0);
            CHECK(f.by == 0);
            CHECK(f.dx == 30);
            CHECK(f.dy == 10);
        }
    }

    SUBCASE("a neighbour is its point placed as an endpoint, in half px from the centre")
    {
        // (0,0) starts its stroke at (-1,-1): its middle (0,0) is -50,-50 half px
        // from the centre; (60,25) starts at (59,24), middle (60,25): 70,0
        const PointF prev(0, 0);
        const PointF next(60, 25);

        renderer.frame([&] { renderer.drawLine(id, p0, p1, 2.0f, 1, &prev, &next); });
        for(size_t i=0; i<4; ++i)
        {
            const MirrorOnly::Fields f=renderer.fields(id, i);

            CAPTURE(i);
            CHECK(f.radius == (MirrorOnly::hasPrev | MirrorOnly::hasNext));
            CHECK(f.cx == -30);
            CHECK(f.cy == -10);
            CHECK(f.bx == -50);
            CHECK(f.by == -50);
            CHECK(f.dx == 70);
            CHECK(f.dy == 0);
        }

        // the next one alone: bx/by stay 0
        renderer.frame([&] { renderer.drawLine(id, p0, p1, 2.0f, 1, nullptr, &next); });
        CHECK(renderer.fields(id, 0).radius == MirrorOnly::hasNext);
        CHECK(renderer.fields(id, 0).bx == 0);
        CHECK(renderer.fields(id, 0).dx == 70);

        // the previous one alone: dx/dy the second endpoint
        renderer.frame([&] { renderer.drawLine(id, p0, p1, 2.0f, 1, &prev); });
        CHECK(renderer.fields(id, 0).radius == MirrorOnly::hasPrev);
        CHECK(renderer.fields(id, 0).bx == -50);
        CHECK(renderer.fields(id, 0).dx == 30);
        CHECK(renderer.fields(id, 0).dy == 10);
    }

    SUBCASE("at a scale, the neighbour is snapped as the endpoints are")
    {
        // 1.5: a 3 px stroke (snap(2) = 3); p0 (15,30) px starts at (14,29), p1
        // (60,45) at (59,44): centre (38,38), p0 -45,-15 half px from it; prev
        // (3,4.5) px starts at (2,3): its middle (3.5,4.5) is -69,-67 half px
        renderer.setScale(1.5f);

        const PointF prev(2, 3);

        renderer.frame([&] { renderer.drawLine(id, p0, p1, 2.0f, 1, &prev); });

        const MirrorOnly::Fields f=renderer.fields(id, 0);

        CHECK(f.border == 3);
        CHECK(f.cx == -45);
        CHECK(f.cy == -15);
        CHECK(f.bx == -69);
        CHECK(f.by == -67);
    }

    SUBCASE("a neighbour 16383.5 px or more from the centre is left out")
    {
        // the middle of next's stroke at 16408 + 1 px, 16384 px from the centre:
        // 32768 half px, which int16_t does not hold
        const PointF far(16408.5f, 25.0f);
        const PointF near(16407.5f, 25.0f);

        renderer.frame([&] { renderer.drawLine(id, p0, p1, 2.0f, 1, nullptr, &far); });
        CHECK(renderer.fields(id, 0).radius == 0);
        CHECK(renderer.fields(id, 0).dx == 30);
        CHECK(renderer.fields(id, 0).dy == 10);

        renderer.frame([&] { renderer.drawLine(id, p0, p1, 2.0f, 1, nullptr, &near); });
        CHECK(renderer.fields(id, 0).radius == MirrorOnly::hasNext);
        CHECK(renderer.fields(id, 0).dx == 32766);

        // so on the other side, and on y: a stroke starting at -16360, its middle
        // -16359, 16384 px from the centre, is left out, one starting at -16359 kept
        const PointF farLeft(-16359.5f, 25.0f);
        const PointF nearLeft(-16358.5f, 25.0f);
        const PointF farUp(10.0f, -16359.5f);

        renderer.frame([&] { renderer.drawLine(id, p0, p1, 2.0f, 1, &farLeft); });
        CHECK(renderer.fields(id, 0).radius == 0);
        CHECK(renderer.fields(id, 0).bx == 0);
        renderer.frame([&] { renderer.drawLine(id, p0, p1, 2.0f, 1, &nearLeft); });
        CHECK(renderer.fields(id, 0).radius == MirrorOnly::hasPrev);
        CHECK(renderer.fields(id, 0).bx == -32766);
        renderer.frame([&] { renderer.drawLine(id, p0, p1, 2.0f, 1, &farUp); });
        CHECK(renderer.fields(id, 0).radius == 0);
    }
}

TEST_CASE("SlotRenderer::drawLine: the same neighbours write nothing, another neighbour rewrites the slot")
{
    MirrorOnly renderer;
    LiveIds ids(3);
    const PointF points[]={PointF(15, 45), PointF(50, 10), PointF(85, 48), PointF(105, 12)};
    PointF prev=points[0];
    PointF next=points[3];

    renderer.setScale(1.5f);
    renderer.setDrawColor(Color(255, 0, 0, 128));

    auto draw=[&](const PointF *before, const PointF *after)
    {
        return renderer.frame([&]
            {
                renderer.drawLine(ids[0], points[0], points[1], 2.0f, 1, nullptr, &points[2]);
                renderer.drawLine(ids[1], points[1], points[2], 2.0f, 1, before, after);
                renderer.drawLine(ids[2], points[2], points[3], 2.0f, 1, &points[1]);
            });
    };

    CHECK(draw(&prev, &next) == 3);
    CHECK(draw(&prev, &next) == 0);

    // a neighbour moved
    prev=PointF(16, 45);
    CHECK(draw(&prev, &next) == 1);
    CHECK(renderer.writtenPrimitiveIds() == std::vector<size_t>{ids[1]});
    CHECK(draw(&prev, &next) == 0);
    next=PointF(105, 13);
    CHECK(draw(&prev, &next) == 1);

    // a neighbour dropped, then given back
    CHECK(draw(nullptr, &next) == 1);
    CHECK(draw(nullptr, &next) == 0);
    CHECK(draw(&prev, &next) == 1);

    // a neighbour that moves within the pixel its stroke starts in changes no byte
    prev=PointF(16.1f, 45.1f);
    CHECK(draw(&prev, &next) == 0);
}

#endif
