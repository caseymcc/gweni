/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/slider.h>
#include <gweni/controls/verticalSlider.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(VerticalSlider)
{
    m_sliderBar->setHorizontal(false);
}

float VerticalSlider::calculateValue()
{
    return 1.f - static_cast<float>(m_sliderBar->getY()) / (getHeight() - m_sliderBar->getHeight());
}

void VerticalSlider::updateBarFromValue()
{
    m_sliderBar->moveTo(m_sliderBar->getX(), (getHeight() - m_sliderBar->getHeight())  *(1.f - m_value));
}

void VerticalSlider::onMouseClickLeft(int x, int y, bool down)
{
    m_sliderBar->moveTo(m_sliderBar->getX(),
        canvasPosToLocal(gweni::Point(x, y)).y - m_sliderBar->getHeight()  *0.5f);
    m_sliderBar->onMouseClickLeft(x, y, down);
    onMoved(m_sliderBar);
}

void VerticalSlider::layout(skin::Base * /*skin*/)
{
    m_sliderBar->setSize(getWidth(), 15);
}

//void VerticalSlider::render(skin::Base *skin)
//{
////    skin->drawSlider(this, skin::Generate, false, m_clampToNotches?m_numNotches:0, m_sliderBar->getHeight());
//    skin->drawControl(this);
//}

}//namespace controls
}//namespace gweni
