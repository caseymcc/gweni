/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni::Curve (gweni/curve.h), the keyframed curve a CurveEditor edits:
 *  evaluate() per span shape against hand-computed values (Linear straight,
 *  Constant holding to the next key, Smooth and Free the cubic Hermite on the
 *  first key's out and the second's in tangent), the ends clamped, a curve of
 *  no key 0 and of one key its value; Smooth keys' Catmull-Rom tangents (flat
 *  at the ends) and Linear keys' chord tangents worked out again after an
 *  edit, Free keys' kept and joined unless broken, a handle set on a Smooth
 *  key making it Free; keys kept sorted (a key moved past a neighbour passes
 *  as few keys as it must, one moved within its neighbours keeps its index);
 *  tessellate()'s points; and lastChange(), the spans an edit changed as a
 *  splice, exact for each edit and, over seeded random edits, every span
 *  outside the splice drawing as it did and the splice's ends differing.
 */
#include <gweni/curve.h>

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using gweni::Curve;
using gweni::CurvePoint;
using gweni::CurveSpanChange;
using Mode=gweni::Curve::TangentMode;

namespace doctest
{
template<> struct StringMaker<CurveSpanChange>
{
    static String convert(const CurveSpanChange &c)
    {
        return ("{"+std::to_string(c.first)+", "+std::to_string(c.removed)+", "+std::to_string(c.inserted)
            +"}").c_str();
    }
};
}

namespace
{

bool operator==(const CurveSpanChange &a, const CurveSpanChange &b)
{
    return (a.first == b.first) && (a.removed == b.removed) && (a.inserted == b.inserted);
}

Curve::Key key(double time, double value, Mode mode=Mode::Smooth)
{
    Curve::Key k;

    k.time=time;
    k.value=value;
    k.mode=mode;
    return k;
}

Curve::Key freeKey(double time, double value, double in, double out, bool broken)
{
    Curve::Key k=key(time, value, Mode::Free);

    k.inTangent=in;
    k.outTangent=out;
    k.broken=broken;
    return k;
}

// n keys at times 0..n-1, values alternating 0 and 1, all in one mode.
Curve row(size_t n, Mode mode)
{
    std::vector<Curve::Key> keys;

    for(size_t i=0; i < n; ++i)
        keys.push_back(key((double)i, (double)(i%2), mode));
    return Curve(keys);
}

std::string times(const Curve &curve)
{
    std::string text;

    for(const Curve::Key &k:curve.keys())
        text+=std::to_string((int)(k.time*10))+":"+std::to_string((int)k.value)+" ";
    return text;
}

// Whether span `a` of one curve draws as span `b` of another: the same points
// along it (tessellate at 8 segments), the same shape.
bool sameSpan(const Curve &x, size_t a, const Curve &y, size_t b)
{
    if(x.spanShape(a) != y.spanShape(b))
        return false;

    std::vector<CurvePoint> p, q;

    x.tessellate(a, 8, p);
    y.tessellate(b, 8, q);
    if(p.size() != q.size())
        return false;
    for(size_t i=0; i < p.size(); ++i)
    {
        if((p[i].x != q[i].x) || (p[i].y != q[i].y))
            return false;
    }
    return true;
}

}//namespace

TEST_CASE("Curve: evaluate per span shape against hand-computed values")
{
    // Linear 0 -> 4 over 0..2, Constant 4 over 2..4, Smooth 1 -> 3 over 4..6:
    // key 2's tangent is the chord between its neighbours, (3-4)/(6-2), the
    // last key's 0.
    Curve curve({key(0, 0, Mode::Linear), key(2, 4, Mode::Constant), key(4, 1, Mode::Smooth),
        key(6, 3, Mode::Smooth)});

    CHECK(curve.evaluate(1.0) == doctest::Approx(2.0));
    CHECK(curve.evaluate(0.5) == doctest::Approx(1.0));
    CHECK(curve.evaluate(2.0) == doctest::Approx(4.0));
    CHECK(curve.evaluate(3.0) == doctest::Approx(4.0));
    CHECK(curve.evaluate(3.999) == doctest::Approx(4.0));
    CHECK(curve.evaluate(4.0) == doctest::Approx(1.0));
    CHECK(curve.key(2).inTangent == doctest::Approx(-0.25));
    CHECK(curve.key(2).outTangent == doctest::Approx(-0.25));
    // u 0.5, dt 2: h00 0.5, h10 0.125, h01 0.5, h11 -0.125.
    CHECK(curve.evaluate(5.0) == doctest::Approx(0.5*1+0.125*2*(-0.25)+0.5*3));

    CHECK(curve.spanShape(0) == Curve::SpanShape::Linear);
    CHECK(curve.spanShape(1) == Curve::SpanShape::Constant);
    CHECK(curve.spanShape(2) == Curve::SpanShape::Hermite);

    // Free: (0,0) leaving at slope 1, (1,1) arriving flat; u 0.5, dt 1.
    Curve free({freeKey(0, 0, 1, 1, false), freeKey(1, 1, 0, 0, false)});

    CHECK(free.evaluate(0.5) == doctest::Approx(0.125*1+0.5*1));
    CHECK(free.evaluate(0.25) == doctest::Approx(
        (0.25*0.25*0.25-2*0.25*0.25+0.25)*1+(-2*0.25*0.25*0.25+3*0.25*0.25)*1));

    // A Hermite span runs straight into a Linear key: its in tangent is the
    // chord from the key before, (2-0)/2.
    Curve intoLinear({key(0, 0, Mode::Smooth), key(2, 2, Mode::Linear), key(3, 0, Mode::Linear)});

    CHECK(intoLinear.key(1).inTangent == doctest::Approx(1.0));
    CHECK(intoLinear.key(1).outTangent == doctest::Approx(-2.0));
    CHECK(intoLinear.evaluate(1.0) == doctest::Approx(0.5*2+(-0.125)*2*1.0));
    CHECK(intoLinear.evaluate(2.5) == doctest::Approx(1.0));
}

TEST_CASE("Curve: the ends are clamped")
{
    CHECK(Curve().evaluate(3.0) == 0.0);
    CHECK(Curve({key(2, 7)}).evaluate(-5.0) == 7.0);
    CHECK(Curve({key(2, 7)}).evaluate(5.0) == 7.0);

    Curve curve({freeKey(1, 3, 5, 5, false), freeKey(2, -1, 5, 5, false)});

    CHECK(curve.evaluate(0.0) == 3.0);
    CHECK(curve.evaluate(-100.0) == 3.0);
    CHECK(curve.evaluate(1.0) == 3.0);
    CHECK(curve.evaluate(2.0) == -1.0);
    CHECK(curve.evaluate(100.0) == -1.0);
    CHECK(curve.evaluate(std::nan("")) == 3.0);
}

TEST_CASE("Curve: Smooth keys' tangents follow their neighbours, flat at the ends")
{
    Curve curve({key(0, 0), key(1, 2), key(3, 6)});

    CHECK(curve.key(0).outTangent == 0.0);
    CHECK(curve.key(1).inTangent == doctest::Approx(2.0));
    CHECK(curve.key(1).outTangent == doctest::Approx(2.0));
    CHECK(curve.key(2).inTangent == 0.0);

    // Its neighbour moved: worked out again.
    curve.moveKey(2, 3, 0);
    CHECK(curve.key(1).inTangent == doctest::Approx(0.0));

    // A key added past the end makes the old last key an inner one.
    curve.addKey(5, 4);
    CHECK(curve.key(2).inTangent == doctest::Approx((4.0-2.0)/(5.0-1.0)));

    // Two neighbours at one time: no slope to take, flat.
    Curve stacked({key(1, 0), key(1, 5), key(1, 9)});

    CHECK(stacked.key(1).value == 5.0);
    CHECK(stacked.key(1).inTangent == 0.0);
}

TEST_CASE("Curve: Free tangents are kept, joined unless broken")
{
    Curve curve({key(0, 0), key(1, 1), key(2, 0)});

    // A handle set on a Smooth key makes it Free with both tangents.
    curve.setOutTangent(1, 3.0);
    CHECK(curve.key(1).mode == Mode::Free);
    CHECK(curve.key(1).inTangent == 3.0);
    CHECK(curve.key(1).outTangent == 3.0);

    // A neighbour's move leaves a Free key's tangents.
    curve.moveKey(0, 0, 5);
    CHECK(curve.key(1).inTangent == 3.0);

    curve.setBroken(1, true);
    curve.setInTangent(1, -1.0);
    CHECK(curve.key(1).inTangent == -1.0);
    CHECK(curve.key(1).outTangent == 3.0);

    // Joined again at the in tangent.
    curve.setBroken(1, false);
    CHECK(curve.key(1).outTangent == -1.0);

    // An unbroken Free key given apart tangents is joined at its in tangent.
    Curve given({freeKey(0, 0, 2, 7, false)});

    CHECK(given.key(0).outTangent == 2.0);

    // Smooth -> Free keeps the shape; Free -> Smooth works the tangent out.
    Curve modes({key(0, 0), key(1, 1), key(2, 4)});

    modes.setMode(1, Mode::Free);
    CHECK(modes.key(1).inTangent == doctest::Approx(2.0));
    CHECK(modes.lastChange().empty());
    modes.setInTangent(1, 0.0);
    modes.setMode(1, Mode::Smooth);
    CHECK(modes.key(1).inTangent == doctest::Approx(2.0));
    CHECK(!modes.key(1).broken);
}

TEST_CASE("Curve: keys stay sorted, a key dragged past a neighbour is reordered")
{
    Curve curve({key(0, 0), key(1, 1), key(2, 2), key(3, 3)});

    // Within its neighbours, an equal time included: the same index.
    CHECK(curve.moveKey(1, 1.5, 1) == 1);
    CHECK(curve.moveKey(1, 2.0, 1) == 1);
    CHECK(times(curve) == "0:0 20:1 20:2 30:3 ");

    // Past the next key: after it, before a key at its new time.
    CHECK(curve.moveKey(1, 3.0, 1) == 2);
    CHECK(times(curve) == "0:0 20:2 30:1 30:3 ");

    // Back left past two keys: after any key at its time.
    CHECK(curve.moveKey(2, 0.0, 1) == 1);
    CHECK(times(curve) == "0:0 0:1 20:2 30:3 ");

    // Right to the end.
    CHECK(curve.moveKey(0, 9.0, 0) == 3);
    CHECK(times(curve) == "0:1 20:2 30:3 90:0 ");

    // Added after the keys at its time; a bad index changes nothing.
    CHECK(curve.addKey(2.0, 7.0) == 2);
    CHECK(curve.moveKey(9, 1, 1) == curve.keyCount());
    CHECK(curve.lastChange().empty());
    CHECK(!curve.removeKey(9));
    CHECK(curve.removeKey(0));
    CHECK(times(curve) == "20:2 20:7 30:3 90:0 ");

    // NaN is 0.
    CHECK(curve.addKey(std::nan(""), std::nan("")) == 0);
    CHECK(curve.key(0).time == 0.0);
    CHECK(curve.key(0).value == 0.0);

    // addKey(time) puts the key on the curve.
    Curve on({key(0, 0, Mode::Linear), key(2, 4, Mode::Linear)});

    on.addKey(1.0);
    CHECK(on.key(1).value == doctest::Approx(2.0));
}

TEST_CASE("Curve: tessellate gives each span's points")
{
    Curve curve({key(0, 0, Mode::Linear), key(2, 4, Mode::Constant), key(4, 1), key(6, 3)});
    std::vector<CurvePoint> points;

    curve.tessellate(0, 20, points);
    REQUIRE(points.size() == 2);
    CHECK(points[1].x == 2.0);
    CHECK(points[1].y == 4.0);

    points.clear();
    curve.tessellate(1, 20, points);
    REQUIRE(points.size() == 3);
    CHECK(points[1].x == 4.0);
    CHECK(points[1].y == 4.0);
    CHECK(points[2].y == 1.0);

    points.clear();
    curve.tessellate(2, 4, points);
    REQUIRE(points.size() == 5);
    CHECK(points[2].x == 5.0);
    CHECK(points[2].y == doctest::Approx(curve.evaluate(5.0)));
    CHECK(points[4].y == 3.0);

    points.clear();
    curve.tessellate(3, 4, points);
    CHECK(points.empty());
}

TEST_CASE("Curve: lastChange is the spans an edit changed")
{
    SUBCASE("a Smooth key's move changes its spans and its neighbours' outer ones")
    {
        Curve curve=row(7, Mode::Smooth);

        curve.moveKey(3, 3.0, 5.0);
        CHECK(curve.lastChange() == CurveSpanChange{1, 4, 4});
        curve.moveKey(3, 3.0, 5.0);
        CHECK(curve.lastChange().empty());
        curve.moveKey(0, 0.0, 9.0);
        CHECK(curve.lastChange() == CurveSpanChange{0, 2, 2});
    }
    SUBCASE("a Linear key's move changes its two spans alone")
    {
        Curve curve=row(7, Mode::Linear);

        curve.moveKey(3, 3.5, 5.0);
        CHECK(curve.lastChange() == CurveSpanChange{2, 2, 2});
    }
    SUBCASE("a Free neighbour keeps its other span")
    {
        Curve curve=row(7, Mode::Smooth);

        curve.setMode(2, Mode::Free);
        CHECK(curve.lastChange().empty());
        curve.moveKey(3, 3.0, 5.0);
        CHECK(curve.lastChange() == CurveSpanChange{2, 3, 3});
    }
    SUBCASE("an added key splits its span")
    {
        Curve curve=row(7, Mode::Smooth);

        CHECK(curve.addKey(3.5, 2.0) == 4);
        CHECK(curve.lastChange() == CurveSpanChange{2, 3, 4});
        curve.addKey(9.0, 0.0);
        CHECK(curve.lastChange() == CurveSpanChange{6, 1, 2});
    }
    SUBCASE("a removed key joins its spans")
    {
        Curve curve=row(7, Mode::Smooth);

        curve.removeKey(3);
        CHECK(curve.lastChange() == CurveSpanChange{1, 4, 3});
        // The new first key is flat, as its tangent was: one span goes alone.
        curve.removeKey(0);
        CHECK(curve.lastChange() == CurveSpanChange{0, 1, 0});
    }
    SUBCASE("a reorder changes the spans it passes")
    {
        Curve curve=row(7, Mode::Linear);

        CHECK(curve.moveKey(2, 4.5, 0.0) == 4);
        CHECK(curve.lastChange() == CurveSpanChange{1, 4, 4});
    }
    SUBCASE("a mode or tangent change")
    {
        Curve curve=row(7, Mode::Smooth);

        curve.setMode(3, Mode::Linear);
        CHECK(curve.lastChange() == CurveSpanChange{2, 2, 2});
        curve.setInTangent(5, 2.0);
        CHECK(curve.lastChange() == CurveSpanChange{4, 2, 2});
        curve.setBroken(5, true);
        CHECK(curve.lastChange().empty());
        curve.setOutTangent(5, 1.0);
        CHECK(curve.lastChange() == CurveSpanChange{5, 1, 1});
        curve.setKeys({key(0, 0), key(1, 1)});
        CHECK(curve.lastChange() == CurveSpanChange{0, 6, 1});
    }
}

TEST_CASE("Curve: over random edits every span outside the change draws as it did")
{
    uint32_t seed=12345;
    auto next=[&seed](uint32_t range)
    {
        seed=seed*1664525u+1013904223u;
        return (seed >> 8)%range;
    };
    Curve curve;

    for(int i=0; i < 12; ++i)
        curve.addKey((double)next(40)/4.0, (double)next(9), (Mode)next(4));

    for(int step=0; step < 2000; ++step)
    {
        Curve before=curve;
        size_t n=curve.keyCount();
        size_t index=n?next((uint32_t)n):0;

        switch(next(8))
        {
        case 0:
            curve.addKey((double)next(40)/4.0, (double)next(9), (Mode)next(4));
            break;
        case 1:
            if(n > 3)
                curve.removeKey(index);
            break;
        case 2:
            curve.setMode(index, (Mode)next(4));
            break;
        case 3:
            curve.setBroken(index, next(2) == 0);
            break;
        case 4:
            curve.setInTangent(index, (double)next(9)-4.0);
            break;
        case 5:
            curve.setOutTangent(index, (double)next(9)-4.0);
            break;
        default:
            curve.moveKey(index, (double)next(40)/4.0, (double)next(9));
            break;
        }
        if(curve == before)
            continue;

        const CurveSpanChange &change=curve.lastChange();

        INFO("step " << step);
        REQUIRE(change.first+change.removed <= before.spanCount());
        REQUIRE(change.first+change.inserted <= curve.spanCount());
        REQUIRE(before.spanCount()-change.removed == curve.spanCount()-change.inserted);
        for(size_t s=0; s < change.first; ++s)
            REQUIRE(sameSpan(before, s, curve, s));
        for(size_t s=change.first+change.removed; s < before.spanCount(); ++s)
            REQUIRE(sameSpan(before, s, curve, s-change.removed+change.inserted));
        // As small as it can be: its ends differ.
        if((change.removed > 0) && (change.inserted > 0))
        {
            CHECK(!sameSpan(before, change.first, curve, change.first));
            CHECK(!sameSpan(before, change.first+change.removed-1, curve, change.first+change.inserted-1));
        }
        for(size_t k=1; k < curve.keyCount(); ++k)
            REQUIRE(curve.key(k-1).time <= curve.key(k).time);
    }
}
