/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni::Gradient (gweni/gradient.h), the colour ramp a GradientEditor edits:
 *  stops sorted by clamped position (stably, NaN at 0) and never fewer than
 *  one; Linear evaluate() per 8-bit channel as a + (b-a)*u rounded to the
 *  nearest level, alpha straight, the first stop's colour before it and the
 *  last's from it on, the later stop's at a shared position, a single stop
 *  everywhere; Constant holding each stop's colour to the next; addStop going
 *  after the stops at its position and addStop(t) taking evaluate(t)'s colour
 *  (so the ramp evaluates the same everywhere); moveStop keeping the index
 *  between its neighbours and passing as few stops as it must otherwise;
 *  removeStop refusing the last stop and a bad index.
 */
#include <gweni/gradient.h>

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>

using gweni::Color;
using gweni::Gradient;
using gweni::GradientStop;

namespace doctest
{
template<> struct StringMaker<gweni::Color>
{
    static String convert(const gweni::Color &c)
    {
        return (std::to_string(c.r)+","+std::to_string(c.g)+","+std::to_string(c.b)+","
            +std::to_string(c.a)).c_str();
    }
};
}

namespace
{

// The colours of the stops in order, as one string, to check an order at once.
std::string order(const Gradient &gradient)
{
    std::string text;

    for(const GradientStop &stop:gradient.stops())
        text+=std::to_string(stop.color.r)+"@"+std::to_string((int)std::lround(stop.position*100))+" ";
    return text;
}

Color grey(unsigned char level) { return Color(level, level, level, 255); }

}//namespace

TEST_CASE("Gradient: stops are sorted, clamped and never none")
{
    Gradient gradient({{0.75f, grey(3)}, {1.5f, grey(4)}, {0.25f, grey(1)}, {-1.0f, grey(0)},
        {0.25f, grey(2)}, {std::numeric_limits<float>::quiet_NaN(), grey(9)}});

    CHECK(order(gradient) == "0@0 9@0 1@25 2@25 3@75 4@100 ");

    Gradient none(std::vector<GradientStop>{});

    REQUIRE(none.stopCount() == 1);
    CHECK(none.stop(0).position == 0.0f);
    CHECK(none.stop(0).color == Color(0, 0, 0, 255));

    Gradient defaults;

    CHECK(defaults.stopCount() == 2);
    CHECK(defaults.evaluate(0.5f) == Color(128, 128, 128, 255));
}

TEST_CASE("Gradient: Linear evaluate is each 8-bit channel at a + (b-a)*u, rounded")
{
    Gradient gradient({{0.0f, Color(0, 255, 10, 255)}, {1.0f, Color(255, 0, 20, 255)}});

    CHECK(gradient.evaluate(0.0f) == Color(0, 255, 10, 255));
    CHECK(gradient.evaluate(1.0f) == Color(255, 0, 20, 255));
    // 127.5 and 15 exactly: half rounds up.
    CHECK(gradient.evaluate(0.5f) == Color(128, 128, 15, 255));
    CHECK(gradient.evaluate(0.25f) == Color(64, 191, 13, 255));

    for(int i=0; i <= 1000; ++i)
    {
        float t=(float)i/1000.0f;
        Color c=gradient.evaluate(t);

        CHECK(c.r == (unsigned char)std::floor(255.0f*t+0.5f));
        CHECK(c.g == (unsigned char)std::floor(255.0f-255.0f*t+0.5f));
    }

    // u is the place between the two stops around t, not t itself.
    Gradient three({{0.2f, grey(0)}, {0.6f, grey(200)}, {1.0f, grey(0)}});

    CHECK(three.evaluate(0.4f) == grey(100));
    CHECK(three.evaluate(0.8f) == grey(100));
    CHECK(three.evaluate(0.6f) == grey(200));
}

TEST_CASE("Gradient: before the first stop and from the last on, their colours")
{
    Gradient gradient({{0.3f, grey(50)}, {0.7f, grey(150)}});

    CHECK(gradient.evaluate(0.0f) == grey(50));
    CHECK(gradient.evaluate(0.29f) == grey(50));
    CHECK(gradient.evaluate(-5.0f) == grey(50));
    CHECK(gradient.evaluate(0.7f) == grey(150));
    CHECK(gradient.evaluate(1.0f) == grey(150));
    CHECK(gradient.evaluate(7.0f) == grey(150));
    CHECK(gradient.evaluate(std::numeric_limits<float>::quiet_NaN()) == grey(50));

    gradient.setInterpolation(Gradient::Interpolation::Constant);
    CHECK(gradient.evaluate(0.1f) == grey(50));
    CHECK(gradient.evaluate(0.9f) == grey(150));
}

TEST_CASE("Gradient: a single stop is its colour everywhere")
{
    Gradient gradient({{0.4f, Color(10, 20, 30, 40)}});

    for(float t:{-1.0f, 0.0f, 0.39f, 0.4f, 0.41f, 1.0f, 2.0f})
    {
        CHECK(gradient.evaluate(t) == Color(10, 20, 30, 40));
        gradient.setInterpolation(Gradient::Interpolation::Constant);
        CHECK(gradient.evaluate(t) == Color(10, 20, 30, 40));
        gradient.setInterpolation(Gradient::Interpolation::Linear);
    }
}

TEST_CASE("Gradient: translucent stops interpolate alpha straight, not premultiplied")
{
    // Opaque red to transparent blue: straight, the middle keeps half of each
    // colour (premultiplied, the blue would weigh nothing at alpha 0 and the
    // middle would be pure red).
    Gradient gradient({{0.0f, Color(255, 0, 0, 255)}, {1.0f, Color(0, 0, 255, 0)}});

    CHECK(gradient.evaluate(0.5f) == Color(128, 0, 128, 128));
    CHECK(gradient.evaluate(0.25f) == Color(191, 0, 64, 191));
    CHECK(gradient.evaluate(1.0f) == Color(0, 0, 255, 0));
}

TEST_CASE("Gradient: Constant holds each stop's colour up to the next")
{
    Gradient gradient({{0.0f, grey(0)}, {0.5f, grey(100)}, {0.8f, grey(200)}},
        Gradient::Interpolation::Constant);

    CHECK(gradient.evaluate(0.0f) == grey(0));
    CHECK(gradient.evaluate(0.49f) == grey(0));
    CHECK(gradient.evaluate(0.5f) == grey(100));
    CHECK(gradient.evaluate(0.79f) == grey(100));
    CHECK(gradient.evaluate(0.8f) == grey(200));
    CHECK(gradient.evaluate(1.0f) == grey(200));
}

TEST_CASE("Gradient: at a position two stops share, the later stop's colour")
{
    Gradient gradient({{0.0f, grey(0)}, {0.5f, grey(100)}, {0.5f, grey(200)}, {1.0f, grey(255)}});

    CHECK(gradient.evaluate(0.25f) == grey(50));
    CHECK(gradient.evaluate(0.5f) == grey(200));
    CHECK(gradient.evaluate(0.75f) == Color(228, 228, 228, 255));
}

TEST_CASE("Gradient: addStop goes after the stops at its position; addStop(t) takes evaluate(t)")
{
    Gradient gradient({{0.0f, grey(0)}, {0.5f, grey(1)}, {1.0f, grey(2)}});

    CHECK(gradient.addStop(0.5f, grey(7)) == 2);
    CHECK(order(gradient) == "0@0 1@50 7@50 2@100 ");
    CHECK(gradient.addStop(-3.0f, grey(8)) == 1);
    CHECK(order(gradient) == "0@0 8@0 1@50 7@50 2@100 ");
    CHECK(gradient.addStop(2.0f, grey(9)) == 5);
    CHECK(gradient.stop(5).position == 1.0f);

    Gradient ramp({{0.0f, Color(0, 0, 0, 0)}, {1.0f, Color(200, 100, 50, 255)}});
    Gradient before=ramp;

    for(float t:{0.1f, 0.33f, 0.5f, 0.9f})
    {
        Color expected=ramp.evaluate(t);
        size_t index=ramp.addStop(t);

        CHECK(ramp.stop(index).position == t);
        CHECK(ramp.stop(index).color == expected);
    }
    CHECK(ramp.stopCount() == 6);
    // The added stops sit on the ramp: it evaluates within a level of before.
    for(int i=0; i <= 100; ++i)
    {
        float t=(float)i/100.0f;
        Color a=before.evaluate(t), b=ramp.evaluate(t);

        CHECK(std::abs((int)a.r-(int)b.r) <= 1);
        CHECK(std::abs((int)a.a-(int)b.a) <= 1);
    }

    Gradient constant({{0.0f, grey(10)}, {0.6f, grey(90)}}, Gradient::Interpolation::Constant);

    CHECK(constant.stop(constant.addStop(0.3f)).color == grey(10));
}

TEST_CASE("Gradient: a stop moved past another reorders, passing as few as it must")
{
    Gradient gradient({{0.0f, grey(0)}, {0.3f, grey(1)}, {0.6f, grey(2)}, {1.0f, grey(3)}});

    // Between its neighbours, up to their positions, it keeps its index.
    CHECK(gradient.moveStop(1, 0.6f) == 1);
    CHECK(order(gradient) == "0@0 1@60 2@60 3@100 ");
    CHECK(gradient.moveStop(1, 0.0f) == 1);
    CHECK(order(gradient) == "0@0 1@0 2@60 3@100 ");
    CHECK(gradient.moveStop(1, 0.3f) == 1);

    // Right past one.
    CHECK(gradient.moveStop(1, 0.8f) == 2);
    CHECK(order(gradient) == "0@0 2@60 1@80 3@100 ");
    // Right past two, landing on the last's position: before it.
    CHECK(gradient.moveStop(0, 1.0f) == 2);
    CHECK(order(gradient) == "2@60 1@80 0@100 3@100 ");
    // Left past two, landing on the first's position: after it.
    CHECK(gradient.moveStop(3, 0.6f) == 1);
    CHECK(order(gradient) == "2@60 3@60 1@80 0@100 ");
    // Clamped.
    CHECK(gradient.moveStop(3, 5.0f) == 3);
    CHECK(gradient.moveStop(2, -5.0f) == 0);
    CHECK(order(gradient) == "1@0 2@60 3@60 0@100 ");
    // A bad index changes nothing.
    Gradient copy=gradient;

    CHECK(gradient.moveStop(4, 0.5f) == 4);
    CHECK(gradient == copy);
}

TEST_CASE("Gradient: removeStop refuses the last stop and a bad index")
{
    Gradient gradient({{0.0f, grey(0)}, {0.5f, grey(1)}, {1.0f, grey(2)}});

    CHECK_FALSE(gradient.removeStop(3));
    CHECK(gradient.stopCount() == 3);
    CHECK(gradient.removeStop(1));
    CHECK(order(gradient) == "0@0 2@100 ");
    CHECK(gradient.removeStop(0));
    CHECK_FALSE(gradient.removeStop(0));
    REQUIRE(gradient.stopCount() == 1);
    CHECK(gradient.stop(0).color == grey(2));

    gradient.setStopColor(0, grey(5));
    gradient.setStopColor(1, grey(6));
    CHECK(gradient.stop(0).color == grey(5));
}
