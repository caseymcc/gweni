/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A pixel a rounded rect (styles 4, 6 and 7) leaves uncovered, outside its
 *  corner arcs, writes no depth: a slot behind it drawn after it still shows
 *  there, as a node graph's link made after the pin it ends on shows round the
 *  pin's circle. Each style is drawn as a circle (a radius of half its side)
 *  at z 5, then a fill behind it at z 2 over the same square:
 *
 *    corners  the square's corner pixels, outside the arc, are the fill's;
 *    inside   the circle's middle is the circle's, the fill rejected there.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 255, 255);
const Color circle(255, 0, 0, 255);
const Color behind(0, 255, 0, 255);

Color pixelAt(const std::vector<uint8_t> &pixels, int x, int y)
{
    const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

    return Color(p[0], p[1], p[2], p[3]);
}

}//namespace

TEST_CASE("Rounded rects: a pixel outside the corner arcs writes no depth")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    std::vector<size_t> ids(2);

    for(size_t &id : ids)
        renderer.generatePrimitive(id);
    std::sort(ids.begin(), ids.end());

    const Rect square(20, 20, 40, 40);
    const std::vector<std::pair<const char *, std::function<void()>>> styles{
        {"rounded rect", [&]() { renderer.drawRoundedRect(ids[0], square, 20.0f, 0.0f, circle, circle,
            circle, 5); }},
        {"bilinear", [&]() { renderer.drawBilinearRect(ids[0], square, 5, circle, circle, circle, circle,
            20.0f, 0.0f, circle); }},
        {"checker", [&]() { renderer.drawCheckerRect(ids[0], square, 5, circle, circle, circle, 20.0f, 0.0f,
            circle); }}};

    for(const auto &style : styles)
    {
        CAPTURE(style.first);
        gweni::test::clearView(background);
        renderer.begin();
        style.second();
        renderer.drawRoundedRect(ids[1], square, 0.0f, 0.0f, behind, behind, behind, 2);
        renderer.end();

        std::vector<uint8_t> pixels;

        gweni::test::readView(pixels);
        // the square's four corner pixels, well outside the arc
        CHECK((pixelAt(pixels, 20, 20) == behind));
        CHECK((pixelAt(pixels, 59, 20) == behind));
        CHECK((pixelAt(pixels, 20, 59) == behind));
        CHECK((pixelAt(pixels, 59, 59) == behind));
        // the middle, the circle's
        CHECK((pixelAt(pixels, 40, 40) == circle));
    }

    for(size_t id : ids)
        renderer.releasePrimitive(id);
    renderer.begin();
    renderer.end();
}
