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
    m_sliderBar->moveTo((getWidth()-m_sliderBar->getWidth())*(m_value), m_sliderBar->getY());
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
    m_sliderBar->setSize(15, getHeight());
}

void HorizontalSlider::render(skin::Base *skin)
{
//    skin->drawSlider(this, skin::Generate, true, m_clampToNotches?m_numNotches:0, m_sliderBar->getWidth());
    skin->drawControl(this);
}

}//namespace controls
}//namespace gweni
