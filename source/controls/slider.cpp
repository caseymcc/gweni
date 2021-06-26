/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/slider.h>
#include <cmath>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(SliderBar)
{
    setTarget(this);
    restrictToParent(true);
}

void SliderBar::render(skin::Base *skin)
{
//    skin->drawSlideButton(this, skin::Generate, isDepressed(), isHorizontal());
    skin->drawControl(this);
}

}//namespace internal


GWENI_CONTROL_CONSTRUCTOR(Slider)
{
    setBounds(gweni::Rect(0, 0, 32, 128));
    m_sliderBar=new internal::SliderBar(this);
    m_sliderBar->onDragged.add(this, &Slider::onMoved);
    m_min=0.0f;
    m_max=1.0f;
    m_clampToNotches=false;
    m_numNotches=5;
    m_value=0.0f;
    setTabable(true);
}

void Slider::onMoved(event::Info)
{
    setValueInternal(calculateValue());
}

void Slider::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
}

float Slider::calculateValue()
{
    return 0;
}

void Slider::setFloatValue(float val, bool /*forceUpdate*/)
{
    val=clamp(val, m_min, m_max);

    // Normalize Value
    val=(val-m_min)/(m_max-m_min);
    setValueInternal(val);
    redraw();
}

void Slider::setValueInternal(float val)
{
    if(m_clampToNotches)
    {
        val=std::floor(val  *float(m_numNotches) + 0.5f);
        val/=float(m_numNotches);
    }

    if(m_value != val)
    {
        m_value=val;
        onValueChanged.call(this);
    }

    updateBarFromValue();
}

float Slider::getFloatValue()
{
    return m_min+(m_value*(m_max-m_min));
}

void Slider::setRange(float fMin, float fMax)
{
    m_min=fMin;
    m_max=fMax;
}

void Slider::renderFocus(gweni::skin::Base *skin)
{
    if(gweni::KeyboardFocus != this)
        return;

    if(!isTabable())
        return;

//    skin->drawKeyboardHighlight(this, skin::Generate, getRenderBounds(), 0);
    skin->drawControl(this);
}

}//namespace controls
}//namespace gweni
