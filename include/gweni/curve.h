/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#ifndef _gweni_curve_h_
#define _gweni_curve_h_

#include <gweni/export.h>

#include <cstddef>
#include <initializer_list>
#include <vector>

namespace gweni
{

//! A point of a curve: x is time, y the value.
struct CurvePoint
{
    double x=0.0;
    double y=0.0;
};

//! Which spans of a curve an edit changed, as a splice: the old curve's spans
//! [first, first+removed) were replaced by the new curve's [first,
//! first+inserted). Every span before first, and every span after the splice
//! (at its index shifted by inserted-removed), has the keys and the shape it
//! had. Span k joins key k to key k+1.
struct CurveSpanChange
{
    size_t first=0;
    size_t removed=0;
    size_t inserted=0;

    bool empty() const { return (removed == 0) && (inserted == 0); }
};

//! A keyframed function curve (Unity's AnimationCurve): keys sorted by time,
//! each with a value, an in and an out tangent (slopes, value per unit of
//! time) and a tangent mode, which an app samples with evaluate(x). A value
//! type with no control behind it; CurveEditor edits one the app owns.
//!
//! Span k, between keys a=k and b=k+1, is shaped by a's mode: Constant holds
//! a's value up to b, Linear is the straight line, and Smooth and Free are the
//! cubic Hermite on a's out tangent and b's in tangent. A key's tangents are
//! always the ones its spans are drawn with: a Free key keeps the ones it is
//! given, and the curve works out every other key's after each edit, a
//! Smooth key's from its neighbours (Catmull-Rom, flat at the ends), a Linear
//! key's as the slopes of the chords to its neighbours (so a Hermite span
//! runs straight into it) and a Constant key's as 0.
//!
//! Times and values are never NaN (a NaN is 0). Keys at an equal time keep
//! the order they were given in; at such a time evaluate() is the later one's
//! value.
class GWENI_EXPORT Curve
{
public:
    enum class TangentMode
    {
        Linear,   //!< the span to the next key is straight
        Constant, //!< the value holds until the next key
        Smooth,   //!< automatic tangents from the neighbours
        Free      //!< tangents set by the user, joined unless broken
    };

    struct Key
    {
        double time=0.0;
        double value=0.0;
        double inTangent=0.0;
        double outTangent=0.0;
        TangentMode mode=TangentMode::Smooth;
        //! A Free key's two tangents are set apart; an unbroken Free key's out
        //! tangent is its in tangent.
        bool broken=false;

        bool operator==(const Key &other) const
        {
            return (time == other.time) && (value == other.value) && (inTangent == other.inTangent)
                && (outTangent == other.outTangent) && (mode == other.mode) && (broken == other.broken);
        }
        bool operator!=(const Key &other) const { return !(*this == other); }
    };

    //! What tessellate() draws a span as.
    enum class SpanShape
    {
        Constant, //!< a step: across at the first key's value, then up or down
        Linear,   //!< one straight segment
        Hermite   //!< a cubic
    };

    Curve()=default;
    //! The keys sorted (stably) by time, as setKeys takes them.
    Curve(std::initializer_list<Key> keys);
    explicit Curve(std::vector<Key> keys);

    size_t keyCount() const { return m_keys.size(); }
    const Key &key(size_t index) const { return m_keys[index]; }
    const std::vector<Key> &keys() const { return m_keys; }
    //! Spans join neighbouring keys: keyCount()-1 of them, none below 2 keys.
    size_t spanCount() const { return m_keys.empty()?0:m_keys.size()-1; }

    //! Replaces every key: sorted stably by time, a Free key's tangents kept
    //! (joined at its in tangent unless broken), every other key's worked out.
    //! Its change is every span.
    void setKeys(std::vector<Key> keys);

    //! Adds a key and returns its index: after every key at a lower or equal
    //! time. A Free key keeps its tangents (joined unless broken).
    size_t addKey(const Key &key);
    size_t addKey(double time, double value, TangentMode mode=TangentMode::Smooth);
    //! Adds a key at the curve's value there, evaluate(time) (Unity's add),
    //! Smooth.
    size_t addKey(double time);

    //! Moves key `index` to (time, value) and returns its new index. It keeps
    //! its index while it stays between its neighbours (an equal time
    //! included); past one it passes as few keys as it must: moved right it
    //! goes before any key at its new time, moved left after them. A bad index
    //! changes nothing and returns keyCount().
    size_t moveKey(size_t index, double time, double value);
    //! Removes key `index`; false (nothing removed) for a bad index.
    bool removeKey(size_t index);

    //! A key changed to another mode keeps its tangents if the new mode is
    //! Free (joined at the in tangent unless broken), else gets the new mode's.
    //! A bad index does nothing.
    void setMode(size_t index, TangentMode mode);
    //! Breaks a key's tangents apart, or joins them at the in tangent. Only
    //! a Free key's tangents can be broken; a key of any other mode is made
    //! Free first. A bad index does nothing.
    void setBroken(size_t index, bool broken);
    //! Sets a tangent (both while the key is not broken). A key that is not
    //! Free is made Free with the tangents it had (Unity: dragging an Auto
    //! key's handle makes it Free). A bad index does nothing.
    void setInTangent(size_t index, double slope);
    void setOutTangent(size_t index, double slope);

    //! The curve at x: 0 with no key, the first key's value before the first
    //! key (and for a NaN), the last key's from the last key on, else span k's
    //! value where key k is the last key at or before x.
    double evaluate(double x) const;

    //! Span `span`'s shape and its value at x (x between its keys' times).
    SpanShape spanShape(size_t span) const;
    double evaluateSpan(size_t span, double x) const;

    //! Appends span `span`'s points to `points`, its first key's point first
    //! and its second key's last: a Linear span's two, a Constant span's
    //! three (the step's corner between them), a Hermite span's segments+1 at
    //! equal steps of time (segments at least 1). Nothing for a bad span.
    void tessellate(size_t span, int segments, std::vector<CurvePoint> &points) const;

    //! What the last edit changed: setKeys, addKey, moveKey, removeKey,
    //! setMode, setBroken, setInTangent or setOutTangent. An edit that changed
    //! no span (a key moved to where it was, a mode that keeps the shape) is
    //! an empty change, as is a refused one.
    const CurveSpanChange &lastChange() const { return m_lastChange; }

    bool operator==(const Curve &other) const { return m_keys == other.m_keys; }
    bool operator!=(const Curve &other) const { return !(*this == other); }

    //! A time or value as the curve keeps it: a NaN is 0.
    static double clean(double v);

private:
    //! The Hermite of span (a, b) at x.
    static double hermite(const Key &a, const Key &b, double x);
    //! Key `index`'s tangents as its mode gives them (a Free key's kept).
    void updateTangents(size_t index);
    //! Joins an unbroken Free key's tangents, cleans its numbers.
    static Key cleanKey(Key key);

    //! The edit window: spans [lo, hi) of the old curve may change; begin()
    //! copies the old keys they join, finish() works out the tangents of keys
    //! [keyLo, keyHi) of the new curve and sets m_lastChange to the spans
    //! that differ.
    struct Window
    {
        size_t spanLo=0;
        size_t oldSpanHi=0;
        std::vector<Key> old;
    };
    Window begin(size_t keyLo, size_t keyHi) const;
    void finish(const Window &window, size_t newKeyLo, size_t newKeyHi);

    std::vector<Key> m_keys;
    CurveSpanChange m_lastChange;
};

}//namespace gweni

#endif//_gweni_curve_h_
