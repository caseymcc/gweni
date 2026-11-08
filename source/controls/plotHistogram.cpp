/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/plotHistogram.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace controls
{

// the size, padding, series and caption are PlotBase's
GWENI_CONTROL_CONSTRUCTOR(PlotHistogram)
{
}

float PlotHistogram::drawnKey(size_t index) const
{
    return float(barHeight(index, getPlotBounds().h));
}

int PlotHistogram::barHeight(size_t index, int plotHeight) const
{
    const float value=m_values[index];

    // NaN fails both comparisons, so it draws nothing too
    if(!(value > m_scaleMin))
        return 0;
    if(!(value < m_scaleMax))
        return plotHeight;

    const float share=(value-m_scaleMin)/(m_scaleMax-m_scaleMin);
    const int height=static_cast<int>(std::lround(share*float(plotHeight)));

    return std::min(std::max(height, 0), plotHeight);
}

Rect PlotHistogram::getBarRect(size_t index) const
{
    const size_t count=m_values.size();

    if(index >= count)
        return Rect();

    const Rect plot=getPlotBounds();
    // 64-bit, so a wide plot of many bars cannot overflow
    const int x0=int(int64_t(index)*plot.w/int64_t(count));
    const int x1=int(int64_t(index+1)*plot.w/int64_t(count));
    int width=x1-x0;

    // the column's last unit is the gap to the next bar
    if(width >= 2)
        --width;

    const int height=barHeight(index, plot.h);

    if((width <= 0) || (height <= 0))
        return Rect(plot.x+x0, plot.y+plot.h, 0, 0);

    return Rect(plot.x+x0, plot.y+plot.h-height, width, height);
}

int PlotHistogram::getBarAt(const Point &local) const
{
    const Rect plot=getPlotBounds();
    const size_t count=m_values.size();

    if((count == 0) || (plot.w <= 0) || (plot.h <= 0))
        return -1;
    if((local.x < plot.x) || (local.x >= plot.x+plot.w) || (local.y < plot.y) || (local.y >= plot.y+plot.h))
        return -1;

    // the column [i*W/N, (i+1)*W/N) holding x: the first i whose next column
    // starts past it
    const int64_t x=local.x-plot.x;
    int64_t bar=(x*int64_t(count))/plot.w;

    while((bar > 0) && (bar*plot.w/int64_t(count) > x))
        --bar;
    while((bar+1 < int64_t(count)) && ((bar+1)*plot.w/int64_t(count) <= x))
        ++bar;

    return int(bar);
}

}//namespace controls
}//namespace gweni
