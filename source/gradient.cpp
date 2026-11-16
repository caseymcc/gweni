/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include <gweni/gradient.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace gweni
{

namespace
{

bool lowerPosition(const GradientStop &a, const GradientStop &b)
{
    return a.position < b.position;
}

}//namespace

size_t Gradient::insertAt(std::vector<GradientStop>::iterator at, const GradientStop &stop)
{
    // The index first: the insert may move the storage begin() points into.
    size_t index=(size_t)(at-m_stops.begin());

    m_stops.insert(at, stop);
    return index;
}

Gradient::Gradient():
    m_stops{{0.0f, Color(0, 0, 0, 255)}, {1.0f, Color(255, 255, 255, 255)}}
{}

Gradient::Gradient(std::initializer_list<GradientStop> stops, Interpolation interpolation):
    m_interpolation(interpolation)
{
    setStops(std::vector<GradientStop>(stops));
}

Gradient::Gradient(std::vector<GradientStop> stops, Interpolation interpolation):
    m_interpolation(interpolation)
{
    setStops(std::move(stops));
}

float Gradient::clampPosition(float position)
{
    if(std::isnan(position))
        return 0.0f;
    return std::min(std::max(position, 0.0f), 1.0f);
}

void Gradient::setStops(std::vector<GradientStop> stops)
{
    for(GradientStop &stop:stops)
        stop.position=clampPosition(stop.position);
    std::stable_sort(stops.begin(), stops.end(), lowerPosition);
    if(stops.empty())
        stops.push_back({0.0f, Color(0, 0, 0, 255)});
    m_stops=std::move(stops);
}

size_t Gradient::addStop(float position, const Color &color)
{
    GradientStop stop{clampPosition(position), color};
    auto at=std::upper_bound(m_stops.begin(), m_stops.end(), stop, lowerPosition);

    return insertAt(at, stop);
}

size_t Gradient::addStop(float position)
{
    position=clampPosition(position);
    return addStop(position, evaluate(position));
}

size_t Gradient::moveStop(size_t index, float position)
{
    if(index >= m_stops.size())
        return m_stops.size();

    position=clampPosition(position);

    bool afterPrevious=(index == 0) || (m_stops[index-1].position <= position);
    bool beforeNext=(index+1 == m_stops.size()) || (position <= m_stops[index+1].position);

    if(afterPrevious && beforeNext)
    {
        m_stops[index].position=position;
        return index;
    }

    GradientStop stop{position, m_stops[index].color};

    m_stops.erase(m_stops.begin()+(std::ptrdiff_t)index);

    // Moved right it stops before the first stop at its position, moved left
    // after the last one there: it passes only the stops it must.
    auto at=beforeNext?
        std::upper_bound(m_stops.begin(), m_stops.end(), stop, lowerPosition):
        std::lower_bound(m_stops.begin(), m_stops.end(), stop, lowerPosition);

    return insertAt(at, stop);
}

void Gradient::setStopColor(size_t index, const Color &color)
{
    if(index < m_stops.size())
        m_stops[index].color=color;
}

bool Gradient::removeStop(size_t index)
{
    if((m_stops.size() <= 1) || (index >= m_stops.size()))
        return false;
    m_stops.erase(m_stops.begin()+(std::ptrdiff_t)index);
    return true;
}

Color Gradient::lerp(const Color &a, const Color &b, float u)
{
    if(!(u > 0.0f))
        return a;
    if(u >= 1.0f)
        return b;

    auto channel=[u](unsigned char from, unsigned char to)
    {
        float value=(float)from+((float)to-(float)from)*u;

        return (unsigned char)std::floor(value+0.5f);
    };

    return Color(channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a));
}

Color Gradient::evaluate(float t) const
{
    if(std::isnan(t))
        return m_stops.front().color;

    // The first stop past t; the stop before it is at or before t.
    auto next=std::upper_bound(m_stops.begin(), m_stops.end(), t,
        [](float value, const GradientStop &stop) { return value < stop.position; });

    if(next == m_stops.begin())
        return m_stops.front().color;
    if(next == m_stops.end())
        return m_stops.back().color;

    const GradientStop &previous=*(next-1);

    if(m_interpolation == Interpolation::Constant)
        return previous.color;

    // next->position > t >= previous.position, so the span has a width.
    float u=(t-previous.position)/(next->position-previous.position);

    return lerp(previous.color, next->color, u);
}

}//namespace gweni
