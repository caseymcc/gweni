/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/plotHistogram.h>
#include <gweni/controls/text.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(PlotHistogram)
{
    setBounds(gweni::Rect(0, 0, 200, 80));
    // inside the frame's 1-unit border, with a unit clear of it
    setPadding(Padding(2, 2, 2, 2));
}

void PlotHistogram::setValues(const std::vector<float> &values)
{
    if(values == m_values)
        return;

    m_values=values;
    updateAutoScale();

    if((m_hoveredBar >= 0) && (size_t(m_hoveredBar) >= m_values.size()))
        setHoveredBar(-1);
    else if(m_hoveredBar >= 0)
        setToolTipText(barToolTip(size_t(m_hoveredBar)));

    // the renderer redraws the bars that changed, and resizes its slot list
    // for a new count
    addStateChange(StateChange_Background);
}

void PlotHistogram::setValue(size_t index, float value)
{
    if(index >= m_values.size())
        return;

    // NaN never equals itself: one NaN replacing another is no change
    const float old=m_values[index];

    if((old == value) || (std::isnan(old) && std::isnan(value)))
        return;

    const int plotHeight=getPlotBounds().h;
    const int oldHeight=barHeight(index, plotHeight);

    m_values[index]=value;

    if(updateAutoScale() || (barHeight(index, plotHeight) != oldHeight))
        addStateChange(StateChange_Background);

    if(m_hoveredBar == int(index))
        setToolTipText(barToolTip(index));
}

void PlotHistogram::setScaleRange(float min, float max)
{
    if(!(max > min))
        max=min+1.0f;

    // the automatic range it had may be this one: then no bar moves
    const bool changed=(m_scaleMin != min) || (m_scaleMax != max);

    m_fixedScale=true;
    m_scaleMin=min;
    m_scaleMax=max;

    if(changed)
        addStateChange(StateChange_Background);
}

void PlotHistogram::setAutoScale()
{
    if(!m_fixedScale)
        return;

    m_fixedScale=false;

    if(updateAutoScale())
        addStateChange(StateChange_Background);
}

bool PlotHistogram::updateAutoScale()
{
    if(m_fixedScale)
        return false;

    float low=0.0f;
    float high=0.0f;
    bool any=false;

    for(float value : m_values)
    {
        if(std::isnan(value))
            continue;

        if(!any)
        {
            low=std::min(0.0f, value);
            high=value;
            any=true;
            continue;
        }
        low=std::min(low, value);
        high=std::max(high, value);
    }

    if(!(high > low))
        high=low+1.0f;

    if((low == m_scaleMin) && (high == m_scaleMax))
        return false;

    m_scaleMin=low;
    m_scaleMax=high;
    return true;
}

void PlotHistogram::setCaption(const String &caption)
{
    if(caption.empty())
    {
        if(!m_caption)
            return;

        delete m_caption;
        m_caption=nullptr;
        invalidate();
        return;
    }

    if(!m_caption)
    {
        // a child, so it is numbered, and drawn, over the bars
        m_caption=newChild<internal::Text>();
        m_caption->setPlacement(Placement::Owner);
        m_caption->setMouseInputEnabled(false);
        invalidate();
    }

    m_caption->setText(caption);
}

String PlotHistogram::getCaption() const
{
    return m_caption?m_caption->getText():String();
}

Rect PlotHistogram::getPlotBounds() const
{
    const Padding &padding=getPadding();
    // below the caption's line, so no bar runs under its text
    const int top=padding.top+m_captionHeight;

    return Rect(padding.left, top,
        std::max(0, getWidth()-padding.left-padding.right),
        std::max(0, getHeight()-top-padding.bottom));
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

String PlotHistogram::barToolTip(size_t index) const
{
    if(index >= m_values.size())
        return String();

    char text[64];

    std::snprintf(text, sizeof(text), "%zu: %g", index, double(m_values[index]));
    return text;
}

void PlotHistogram::setHoveredBar(int bar)
{
    if(bar == m_hoveredBar)
        return;

    m_hoveredBar=bar;
    // the two bars' looks; the renderer redraws only those
    addStateChange(StateChange_Background);
    setToolTipText((bar >= 0)?barToolTip(size_t(bar)):String());
}

void PlotHistogram::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    setHoveredBar(getBarAt(canvasPosToLocal(Point(x, y))));
}

void PlotHistogram::onMouseLeave()
{
    setHoveredBar(-1);
    ParentClass::onMouseLeave();
}

void PlotHistogram::layout(skin::Base *skin)
{
    ParentClass::layout(skin);

    // The caption's line, measured by this pass, is taken off the plot's top:
    // a new height moves the bars, which the renderer redraws.
    const int captionHeight=m_caption?m_caption->getPreferredSize().height:0;

    if(captionHeight != m_captionHeight)
    {
        m_captionHeight=captionHeight;
        addStateChange(StateChange_Background);
    }

    if(!m_caption)
        return;

    // at the top centre, above the plot
    const Padding &padding=getPadding();
    const Size &size=m_caption->getPreferredSize();
    const int width=getWidth()-padding.left-padding.right;

    placeChild(m_caption, padding.left+(width-size.width)/2, padding.top, size.width, size.height);
}

}//namespace controls
}//namespace gweni
