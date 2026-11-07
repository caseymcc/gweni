/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/slider.h>
#include <gweni/controls/horizontalSlider.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(HorizontalSlider)
{
    m_sliderBar->setHorizontal(true);
}

float HorizontalSlider::calculateValue()
{
    return static_cast<float>(m_sliderBar->getX()) / (getWidth() - m_sliderBar->getWidth());
}

void HorizontalSlider::updateBarFromValue()
{
    placeChildPos(m_sliderBar, static_cast<int>((getWidth()-m_sliderBar->getWidth())*m_value),
        m_sliderBar->getY());
}

void HorizontalSlider::onMouseClickLeft(int x, int y, bool down)
{
    m_sliderBar->moveTo(canvasPosToLocal(Point(x, y)).x - m_sliderBar->getWidth()/2,
        m_sliderBar->getY());
    m_sliderBar->onMouseClickLeft(x, y, down);
    onMoved(m_sliderBar);
}

void HorizontalSlider::layout(skin::Base * /*skin*/)
{
    placeChildSize(m_sliderBar, 15, getHeight());
    // a resize moves the thumb's travel, so put it back at the value
    updateBarFromValue();
}

}//namespace controls
}//namespace gweni
