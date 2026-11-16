/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include <gweni/curve.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace gweni
{

namespace
{

bool earlier(const Curve::Key &a, const Curve::Key &b)
{
    return a.time < b.time;
}

Curve::SpanShape shapeOf(Curve::TangentMode mode)
{
    switch(mode)
    {
    case Curve::TangentMode::Constant:
        return Curve::SpanShape::Constant;
    case Curve::TangentMode::Linear:
        return Curve::SpanShape::Linear;
    default:
        return Curve::SpanShape::Hermite;
    }
}

// Whether span (a, b) and span (c, d) draw the same: a Constant or Linear span
// reads its keys' points alone, a Hermite one of some width a's out and b's
// in tangent too.
bool sameSpan(const Curve::Key &a, const Curve::Key &b, const Curve::Key &c, const Curve::Key &d)
{
    Curve::SpanShape shape=shapeOf(a.mode);

    if(shape != shapeOf(c.mode))
        return false;
    if((a.time != c.time) || (a.value != c.value) || (b.time != d.time) || (b.value != d.value))
        return false;
    // A span of no width is its two points whatever its tangents.
    if((shape != Curve::SpanShape::Hermite) || !(b.time > a.time))
        return true;
    return (a.outTangent == c.outTangent) && (b.inTangent == d.inTangent);
}

// The slope of the chord from a to b; 0 for two keys at one time.
double chord(const Curve::Key &a, const Curve::Key &b)
{
    double dt=b.time-a.time;

    return (dt > 0.0)?(b.value-a.value)/dt:0.0;
}

}//namespace

double Curve::clean(double v)
{
    return std::isnan(v)?0.0:v;
}

Curve::Key Curve::cleanKey(Key key)
{
    key.time=clean(key.time);
    key.value=clean(key.value);
    key.inTangent=clean(key.inTangent);
    key.outTangent=clean(key.outTangent);
    if(key.mode != TangentMode::Free)
        key.broken=false;
    else if(!key.broken)
        key.outTangent=key.inTangent;
    return key;
}

Curve::Curve(std::initializer_list<Key> keys)
{
    setKeys(std::vector<Key>(keys));
}

Curve::Curve(std::vector<Key> keys)
{
    setKeys(std::move(keys));
}

void Curve::setKeys(std::vector<Key> keys)
{
    size_t oldSpans=spanCount();

    for(Key &key:keys)
        key=cleanKey(key);
    std::stable_sort(keys.begin(), keys.end(), earlier);
    m_keys=std::move(keys);
    for(size_t i=0; i < m_keys.size(); ++i)
        updateTangents(i);
    m_lastChange={0, oldSpans, spanCount()};
}

void Curve::updateTangents(size_t index)
{
    Key &key=m_keys[index];
    bool first=(index == 0);
    bool last=(index+1 == m_keys.size());

    switch(key.mode)
    {
    case TangentMode::Free:
        return;
    case TangentMode::Constant:
        key.inTangent=0.0;
        key.outTangent=0.0;
        return;
    case TangentMode::Linear:
        key.inTangent=first?0.0:chord(m_keys[index-1], key);
        key.outTangent=last?0.0:chord(key, m_keys[index+1]);
        return;
    case TangentMode::Smooth:
    {
        // Catmull-Rom: the slope of the chord between the neighbours, flat at
        // the ends.
        double slope=(first || last)?0.0:chord(m_keys[index-1], m_keys[index+1]);

        key.inTangent=slope;
        key.outTangent=slope;
        return;
    }
    }
}

// An edit of the old keys [keyLo, keyHi) changes those keys and the tangents
// of their neighbours keyLo-1 and keyHi, so the spans with an end in
// [keyLo-1, keyHi]: spans [keyLo-2, keyHi+1).
Curve::Window Curve::begin(size_t keyLo, size_t keyHi) const
{
    Window window;

    window.spanLo=(keyLo >= 2)?keyLo-2:0;
    window.oldSpanHi=std::min(spanCount(), keyHi+1);
    window.spanLo=std::min(window.spanLo, window.oldSpanHi);

    size_t keyEnd=std::min(m_keys.size(), window.oldSpanHi+1);

    if(window.spanLo < keyEnd)
        window.old.assign(m_keys.begin()+(std::ptrdiff_t)window.spanLo, m_keys.begin()+(std::ptrdiff_t)keyEnd);
    return window;
}

void Curve::finish(const Window &window, size_t newKeyLo, size_t newKeyHi)
{
    size_t tangentLo=(newKeyLo >= 1)?newKeyLo-1:0;
    size_t tangentHi=std::min(m_keys.size(), newKeyHi+1);

    for(size_t i=tangentLo; i < tangentHi; ++i)
        updateTangents(i);

    // The new spans the edit can have changed, then the ones at either end
    // that draw as their old counterparts left out.
    size_t first=window.spanLo;
    size_t oldHi=window.oldSpanHi;
    size_t newHi=std::max(first, std::min(spanCount(), newKeyHi+1));
    auto oldSpanSame=[&](size_t oldSpan, size_t newSpan)
    {
        const Key &a=window.old[oldSpan-window.spanLo];
        const Key &b=window.old[oldSpan-window.spanLo+1];

        return sameSpan(a, b, m_keys[newSpan], m_keys[newSpan+1]);
    };

    while((first < oldHi) && (first < newHi) && oldSpanSame(first, first))
        ++first;
    while((first < oldHi) && (first < newHi) && oldSpanSame(oldHi-1, newHi-1))
    {
        --oldHi;
        --newHi;
    }
    m_lastChange={first, oldHi-first, newHi-first};
}

size_t Curve::addKey(const Key &key)
{
    Key added=cleanKey(key);
    auto at=std::upper_bound(m_keys.begin(), m_keys.end(), added, earlier);
    size_t index=(size_t)(at-m_keys.begin());
    Window window=begin(index, index);

    m_keys.insert(m_keys.begin()+(std::ptrdiff_t)index, added);
    finish(window, index, index+1);
    return index;
}

size_t Curve::addKey(double time, double value, TangentMode mode)
{
    Key key;

    key.time=time;
    key.value=value;
    key.mode=mode;
    return addKey(key);
}

size_t Curve::addKey(double time)
{
    time=clean(time);
    return addKey(time, evaluate(time), TangentMode::Smooth);
}

size_t Curve::moveKey(size_t index, double time, double value)
{
    if(index >= m_keys.size())
    {
        m_lastChange={};
        return m_keys.size();
    }

    time=clean(time);
    value=clean(value);

    bool afterPrevious=(index == 0) || (m_keys[index-1].time <= time);
    bool beforeNext=(index+1 == m_keys.size()) || (time <= m_keys[index+1].time);

    if(afterPrevious && beforeNext)
    {
        Window window=begin(index, index+1);

        m_keys[index].time=time;
        m_keys[index].value=value;
        finish(window, index, index+1);
        return index;
    }

    Key key=m_keys[index];

    key.time=time;
    key.value=value;

    // Where it goes among the other keys: moved right before the first key at
    // its time, moved left after the last one there, so it passes only the
    // keys it must. Found before the erase, in the old indices.
    size_t target;

    if(beforeNext)
        target=(size_t)(std::upper_bound(m_keys.begin(), m_keys.begin()+(std::ptrdiff_t)index, key, earlier)
            -m_keys.begin());
    else
        target=(size_t)(std::lower_bound(m_keys.begin()+(std::ptrdiff_t)index+1, m_keys.end(), key, earlier)
            -m_keys.begin())-1;

    size_t lo=std::min(index, target);
    size_t hi=std::max(index, target)+1;
    Window window=begin(lo, hi);

    m_keys.erase(m_keys.begin()+(std::ptrdiff_t)index);
    m_keys.insert(m_keys.begin()+(std::ptrdiff_t)target, key);
    finish(window, lo, hi);
    return target;
}

bool Curve::removeKey(size_t index)
{
    if(index >= m_keys.size())
    {
        m_lastChange={};
        return false;
    }

    Window window=begin(index, index+1);

    m_keys.erase(m_keys.begin()+(std::ptrdiff_t)index);
    finish(window, index, index);
    return true;
}

void Curve::setMode(size_t index, TangentMode mode)
{
    if(index >= m_keys.size())
    {
        m_lastChange={};
        return;
    }

    Window window=begin(index, index+1);
    Key &key=m_keys[index];

    key.mode=mode;
    key=cleanKey(key);
    finish(window, index, index+1);
}

void Curve::setBroken(size_t index, bool broken)
{
    if(index >= m_keys.size())
    {
        m_lastChange={};
        return;
    }

    Window window=begin(index, index+1);
    Key &key=m_keys[index];

    key.mode=TangentMode::Free;
    key.broken=broken;
    key=cleanKey(key);
    finish(window, index, index+1);
}

void Curve::setInTangent(size_t index, double slope)
{
    if(index >= m_keys.size())
    {
        m_lastChange={};
        return;
    }

    Window window=begin(index, index+1);
    Key &key=m_keys[index];

    key.mode=TangentMode::Free;
    key.inTangent=clean(slope);
    if(!key.broken)
        key.outTangent=key.inTangent;
    finish(window, index, index+1);
}

void Curve::setOutTangent(size_t index, double slope)
{
    if(index >= m_keys.size())
    {
        m_lastChange={};
        return;
    }

    Window window=begin(index, index+1);
    Key &key=m_keys[index];

    key.mode=TangentMode::Free;
    key.outTangent=clean(slope);
    if(!key.broken)
        key.inTangent=key.outTangent;
    finish(window, index, index+1);
}

double Curve::hermite(const Key &a, const Key &b, double x)
{
    double dt=b.time-a.time;

    if(!(dt > 0.0))
        return b.value;

    double u=(x-a.time)/dt;
    double u2=u*u;
    double u3=u2*u;
    double h00=2.0*u3-3.0*u2+1.0;
    double h10=u3-2.0*u2+u;
    double h01=-2.0*u3+3.0*u2;
    double h11=u3-u2;

    return h00*a.value+h10*dt*a.outTangent+h01*b.value+h11*dt*b.inTangent;
}

Curve::SpanShape Curve::spanShape(size_t span) const
{
    if(span >= spanCount())
        return SpanShape::Linear;
    return shapeOf(m_keys[span].mode);
}

double Curve::evaluateSpan(size_t span, double x) const
{
    if(span >= spanCount())
        return evaluate(x);

    const Key &a=m_keys[span];
    const Key &b=m_keys[span+1];

    switch(shapeOf(a.mode))
    {
    case SpanShape::Constant:
        return (x < b.time)?a.value:b.value;
    case SpanShape::Linear:
    {
        double dt=b.time-a.time;

        if(!(dt > 0.0))
            return b.value;
        return a.value+(b.value-a.value)*((x-a.time)/dt);
    }
    case SpanShape::Hermite:
        break;
    }
    return hermite(a, b, x);
}

double Curve::evaluate(double x) const
{
    if(m_keys.empty())
        return 0.0;
    if(std::isnan(x))
        return m_keys.front().value;

    // The first key past x; the key before it is the last at or before x.
    auto next=std::upper_bound(m_keys.begin(), m_keys.end(), x,
        [](double value, const Key &key) { return value < key.time; });

    if(next == m_keys.begin())
        return m_keys.front().value;
    if(next == m_keys.end())
        return m_keys.back().value;
    return evaluateSpan((size_t)(next-m_keys.begin())-1, x);
}

void Curve::tessellate(size_t span, int segments, std::vector<CurvePoint> &points) const
{
    if(span >= spanCount())
        return;

    const Key &a=m_keys[span];
    const Key &b=m_keys[span+1];

    switch(shapeOf(a.mode))
    {
    case SpanShape::Constant:
        points.push_back({a.time, a.value});
        points.push_back({b.time, a.value});
        points.push_back({b.time, b.value});
        return;
    case SpanShape::Linear:
        points.push_back({a.time, a.value});
        points.push_back({b.time, b.value});
        return;
    case SpanShape::Hermite:
        break;
    }

    segments=std::max(segments, 1);
    points.push_back({a.time, a.value});
    for(int i=1; i < segments; ++i)
    {
        double x=a.time+(b.time-a.time)*((double)i/(double)segments);

        points.push_back({x, hermite(a, b, x)});
    }
    points.push_back({b.time, b.value});
}

}//namespace gweni
