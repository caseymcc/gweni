/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/slider.h>
#include <gweni/controls/horizontalSlider.h>

#include <algorithm>
#include <cmath>

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

// Rounded, so a thumb the mouse put at x, read back as x/travel, lands at x
// again (a truncating cast left it a unit short on some positions, #466). On
// the track at y 0 whatever the drag did across it (#727): the Dragger moves
// the thumb in both axes, and this runs in the same input event.
void HorizontalSlider::updateBarFromValue()
{
    const int travel=std::max(0, getWidth()-m_sliderBar->getWidth());

    placeChildPos(m_sliderBar, thumbOffset(travel, m_value), 0);
}

// The thumb jumps to the click, kept on the track: computed, like the value.
void HorizontalSlider::onMouseClickLeft(int x, int y, bool down)
{
    const Point pos=m_sliderBar->restrictedPos(canvasPosToLocal(Point(x, y)).x - m_sliderBar->getWidth()/2,
        0);

    placeChildPos(m_sliderBar, pos.x, pos.y);
    m_sliderBar->onMouseClickLeft(x, y, down);
    onMoved(m_sliderBar);
}

void HorizontalSlider::layout(skin::Base * /*skin*/)
{
    placeChildSize(m_sliderBar, 15, getHeight());
    // a resize moves the thumb's travel, so put it back at the value
    updateBarFromValue();
    placeValueText();
}

}//namespace controls
}//namespace gweni
