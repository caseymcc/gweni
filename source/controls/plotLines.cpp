/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/plotLines.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace gweni
{
namespace controls
{

// the size, padding, series and caption are PlotBase's
GWENI_CONTROL_CONSTRUCTOR(PlotLines)
{
}

void PlotLines::push(float value)
{
    std::vector<float> values=m_values;

    if(!values.empty())
        values.erase(values.begin());
    values.push_back(value);

    // a series that comes out the same (all of it equal to value) marks nothing
    setValues(values);
}

void PlotLines::setLineThickness(float thickness)
{
    if(!(thickness > 0.0f))
        thickness=1.0f;

    if(thickness == m_thickness)
        return;

    m_thickness=thickness;
    addStateChange(StateChange_Background);
}

float PlotLines::sampleX(size_t index, const Rect &plot) const
{
    const size_t count=m_values.size();
    const float inset=std::min(m_thickness, float(plot.w))/2.0f;

    if(count < 2)
        return float(plot.w)/2.0f;

    const float span=std::max(0.0f, float(plot.w)-m_thickness);

    return inset+float(index)*span/float(count-1);
}

float PlotLines::sampleY(size_t index, const Rect &plot) const
{
    const float value=m_values[index];

    if(std::isnan(value))
        return std::numeric_limits<float>::quiet_NaN();

    const float share=std::min(1.0f, std::max(0.0f, (value-m_scaleMin)/(m_scaleMax-m_scaleMin)));
    const float inset=std::min(m_thickness, float(plot.h))/2.0f;
    const float span=std::max(0.0f, float(plot.h)-m_thickness);

    return inset+(1.0f-share)*span;
}

float PlotLines::drawnKey(size_t index) const
{
    return sampleY(index, getPlotBounds());
}

PointF PlotLines::getSamplePoint(size_t index) const
{
    if(index >= m_values.size())
        return PointF();

    const Rect plot=getPlotBounds();

    return PointF(float(plot.x)+sampleX(index, plot), float(plot.y)+sampleY(index, plot));
}

bool PlotLines::getSegment(size_t index, PointF &p0, PointF &p1) const
{
    if(index+1 >= m_values.size())
        return false;

    p0=getSamplePoint(index);
    p1=getSamplePoint(index+1);
    return !std::isnan(p0.y) && !std::isnan(p1.y);
}

int PlotLines::sampleAt(const Point &local) const
{
    const Rect plot=getPlotBounds();
    const size_t count=m_values.size();

    if((count == 0) || (plot.w <= 0) || (plot.h <= 0))
        return -1;
    if((local.x < plot.x) || (local.x >= plot.x+plot.w) || (local.y < plot.y) || (local.y >= plot.y+plot.h))
        return -1;
    if(count == 1)
        return 0;

    // the unit's middle, against the samples' x
    const float x=float(local.x-plot.x)+0.5f;
    const float span=std::max(0.0f, float(plot.w)-m_thickness);

    if(!(span > 0.0f))
        return 0;

    const float step=span/float(count-1);
    const float inset=std::min(m_thickness, float(plot.w))/2.0f;
    const long nearest=std::lround((x-inset)/step);

    return int(std::min(long(count-1), std::max(0L, nearest)));
}

}//namespace controls
}//namespace gweni
