/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#ifndef _gweni_gradient_h_
#define _gweni_gradient_h_

#include <gweni/export.h>
#include <gweni/platforms/platformTypes.h>

#include <cstddef>
#include <initializer_list>
#include <vector>

namespace gweni
{

//! One colour stop of a Gradient: a position in 0..1 and a straight-alpha
//! RGBA colour.
struct GradientStop
{
    float position=0.0f;
    Color color;

    bool operator==(const GradientStop &other) const
    {
        return (position == other.position) && (color == other.color);
    }
    bool operator!=(const GradientStop &other) const { return !(*this == other); }
};

//! A colour ramp (Blender's ColorRamp, Unity's Gradient): stops sorted by
//! position, which an app samples with evaluate(t). A value type with no
//! control behind it; GradientEditor edits one the app owns.
//!
//! There is always at least one stop: a gradient made from no stops has one
//! black stop at 0, and removeStop() refuses the last one. Positions are
//! clamped to 0..1 (NaN is 0). Stops at an equal position keep the order they
//! were given in.
class GWENI_EXPORT Gradient
{
public:
    enum class Interpolation
    {
        Linear,  //!< each channel linear in t between two stops
        Constant //!< a stop's colour held up to the next stop
    };

    //! Black at 0 and white at 1, linear.
    Gradient();
    //! The stops sorted (stably) by their clamped positions; none gives one
    //! black stop at 0.
    Gradient(std::initializer_list<GradientStop> stops, Interpolation interpolation=Interpolation::Linear);
    explicit Gradient(std::vector<GradientStop> stops, Interpolation interpolation=Interpolation::Linear);

    Interpolation interpolation() const { return m_interpolation; }
    void setInterpolation(Interpolation interpolation) { m_interpolation=interpolation; }

    size_t stopCount() const { return m_stops.size(); }
    const GradientStop &stop(size_t index) const { return m_stops[index]; }
    const std::vector<GradientStop> &stops() const { return m_stops; }
    //! Replaces every stop, as the constructor takes them.
    void setStops(std::vector<GradientStop> stops);

    //! Adds a stop and returns its index: after every stop at a lower or equal
    //! position, so a stop added where others are goes after them.
    size_t addStop(float position, const Color &color);
    //! Adds a stop of the colour the gradient has there, evaluate(position)
    //! (Unity's add), so the ramp looks the same in Linear mode.
    size_t addStop(float position);

    //! Moves stop `index` to `position` and returns its new index. It keeps its
    //! index while it stays between its neighbours (an equal position
    //! included); past one it passes as few stops as it must: moved right it
    //! goes before any stop at its new position, moved left after them. A bad
    //! index changes nothing and returns stopCount().
    size_t moveStop(size_t index, float position);
    //! A bad index does nothing.
    void setStopColor(size_t index, const Color &color);
    //! Removes stop `index`; false (nothing removed) for the last stop or a bad
    //! index.
    bool removeStop(size_t index);

    //! The colour at t. Before the first stop it is the first stop's colour,
    //! from the last stop on the last one's (NaN is the first's). Linear: the
    //! two stops around t, each 8-bit channel, alpha included (straight, not
    //! premultiplied), at a + (b-a)*u rounded to the nearest level, u = t's
    //! place between them; the arithmetic of the bilinear fill (style 6) a span
    //! between two stops is drawn with, so a ramp draws what it evaluates to.
    //! At a position two stops share, the later one's colour. Constant: the
    //! colour of the last stop at or before t.
    Color evaluate(float t) const;

    //! a + (b-a)*u per channel, rounded to the nearest level; u clamped to 0..1.
    static Color lerp(const Color &a, const Color &b, float u);

    //! Positions as the gradient keeps them: clamped to 0..1, NaN 0.
    static float clampPosition(float position);

    bool operator==(const Gradient &other) const
    {
        return (m_interpolation == other.m_interpolation) && (m_stops == other.m_stops);
    }
    bool operator!=(const Gradient &other) const { return !(*this == other); }

private:
    size_t insertAt(std::vector<GradientStop>::iterator at, const GradientStop &stop);

    std::vector<GradientStop> m_stops;
    Interpolation m_interpolation=Interpolation::Linear;
};

}//namespace gweni

#endif//_gweni_gradient_h_
