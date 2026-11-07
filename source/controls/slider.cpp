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

void SliderBar::onMouseClickLeft(int x, int y, bool down)
{
    const bool wasDepressed=m_depressed;

    ParentClass::onMouseClickLeft(x, y, down);

    if(m_depressed != wasDepressed)
        redraw();
}

}//namespace internal


GWENI_CONTROL_CONSTRUCTOR(Slider)
{
    setBounds(gweni::Rect(0, 0, 32, 128));
    m_sliderBar=newChild<internal::SliderBar>();
    m_sliderBar->onDragged.add(this, &Slider::onMoved);
    m_min=0.0f;
    m_max=1.0f;
    m_clampToNotches=false;
    m_numNotches=5;
    m_value=0.0f;
    setTabable(true);
}

void Slider::setDisabled(bool active)
{
    ParentClass::setDisabled(active);
    m_sliderBar->setDisabled(active);
}

void Slider::onMoved(event::Info)
{
    setValueInternal(calculateValue());
}

// The thumb is sized from this control and placed at its value, so it does
// not count in the size this control asks for: that would hand it back what
// an earlier pass gave it, and move it with the value. A slider asks for the
// size it was asked to be.
void Slider::calculateSize(skin::Base *skin, Dim dim)
{
    m_sliderBar->calculateSize(skin, dim);

    if(dim == Dim::X)
        m_preferredSize.width=preferredFromContent(getRequestedBounds().w, Dim::X);
    else
        m_preferredSize.height=preferredFromContent(getRequestedBounds().h, Dim::Y);
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
//    skin->drawControl(this);
    if(getStateChange() != StateChange_Nothing)
        m_skinControl->update(skin->getRenderer(), this);
}

}//namespace controls
}//namespace gweni
