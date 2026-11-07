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
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

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
    // no fill is drawn for it, nor any id taken
    CHECK(renderer.events.empty());
}
