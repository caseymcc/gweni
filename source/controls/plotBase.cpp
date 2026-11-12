/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/plotBase.h>
#include <gweni/controls/text.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(PlotBase)
{
    setBounds(gweni::Rect(0, 0, 200, 80));
    // inside the frame's 1-unit border, with a unit clear of it
    setPadding(Padding(2, 2, 2, 2));
}

void PlotBase::setValues(const std::vector<float> &values)
{
    if(values == m_values)
        return;

    m_values=values;
    updateAutoScale();

    if((m_hovered >= 0) && (size_t(m_hovered) >= m_values.size()))
        setHoveredSample(-1);
    else if(m_hovered >= 0)
        setToolTipText(sampleToolTip(size_t(m_hovered)));

    // the renderer redraws the samples that changed, and resizes its slot
    // list for a new count
    addStateChange(StateChange_Background);
}

void PlotBase::setValue(size_t index, float value)
{
    if(index >= m_values.size())
        return;

    // NaN never equals itself: one NaN replacing another is no change
    const float old=m_values[index];

    if((old == value) || (std::isnan(old) && std::isnan(value)))
        return;

    const float before=drawnKey(index);

    m_values[index]=value;

    if(updateAutoScale())
        addStateChange(StateChange_Background);
    else
    {
        const float after=drawnKey(index);

        if((before != after) && !(std::isnan(before) && std::isnan(after)))
            addStateChange(StateChange_Background);
    }

    if(m_hovered == int(index))
        setToolTipText(sampleToolTip(index));
}

void PlotBase::setScaleRange(float min, float max)
{
    if(!(max > min))
        max=min+1.0f;

    // the automatic range it had may be this one: then nothing moves
    const bool changed=(m_scaleMin != min) || (m_scaleMax != max);

    m_fixedScale=true;
    m_scaleMin=min;
    m_scaleMax=max;

    if(changed)
        addStateChange(StateChange_Background);
}

void PlotBase::setAutoScale()
{
    if(!m_fixedScale)
        return;

    m_fixedScale=false;

    if(updateAutoScale())
        addStateChange(StateChange_Background);
}

bool PlotBase::updateAutoScale()
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

void PlotBase::setCaption(const String &caption)
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
        // a child, so it is numbered, and drawn, over the series
        m_caption=newChild<internal::Text>();
        m_caption->setPlacement(Placement::Owner);
        m_caption->setMouseInputEnabled(false);
        invalidate();
    }

    m_caption->setText(caption);
}

String PlotBase::getCaption() const
{
    return m_caption?m_caption->getText():String();
}

Rect PlotBase::getPlotBounds() const
{
    const Padding &padding=getPadding();
    // below the caption's line, so nothing is drawn under its text
    const int top=padding.top+m_captionHeight;

    return Rect(padding.left, top,
        std::max(0, getWidth()-padding.left-padding.right),
        std::max(0, getHeight()-top-padding.bottom));
}

String PlotBase::sampleToolTip(size_t index) const
{
    if(index >= m_values.size())
        return String();

    char text[64];

    std::snprintf(text, sizeof(text), "%zu: %g", index, double(m_values[index]));
    return text;
}

void PlotBase::setHoveredSample(int sample)
{
    if(sample == m_hovered)
        return;

    m_hovered=sample;
    setToolTipText((sample >= 0)?sampleToolTip(size_t(sample)):String());
    hoveredChanged();
}

void PlotBase::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    setHoveredSample(sampleAt(canvasPosToLocal(Point(x, y))));
}

void PlotBase::onMouseLeave()
{
    setHoveredSample(-1);
    ParentClass::onMouseLeave();
}

void PlotBase::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
    placeCaption();
}

// The caption's height is measured in the Y pass, after layout()'s run at the
// start of the X arrange read it: it is placed again here, with its height
// final, before the pass arranges it in Y.
void PlotBase::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::Y)
        placeCaption();
    ParentClass::arrange(skin, dim);
}

void PlotBase::placeCaption()
{
    // The caption's line is taken off the plot's top: a new height moves the
    // series, which the renderer redraws.
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
