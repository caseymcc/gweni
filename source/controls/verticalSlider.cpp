/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/slider.h>
#include <gweni/controls/verticalSlider.h>

#include <algorithm>
#include <cmath>

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

// Rounded, so a thumb the mouse put at y, read back as 1-y/travel, lands at y
// again (a truncating cast left it a unit short on some positions, #466).
void VerticalSlider::updateBarFromValue()
{
    const int travel=std::max(0, getHeight()-m_sliderBar->getHeight());

    placeChildPos(m_sliderBar, m_sliderBar->getX(),
        thumbOffset(travel, 1.f-m_value));
}

// The thumb jumps to the click, kept on the track: computed, like the value.
void VerticalSlider::onMouseClickLeft(int x, int y, bool down)
{
    const Point pos=m_sliderBar->restrictedPos(m_sliderBar->getX(),
        canvasPosToLocal(gweni::Point(x, y)).y - m_sliderBar->getHeight()  *0.5f);

    placeChildPos(m_sliderBar, pos.x, pos.y);
    m_sliderBar->onMouseClickLeft(x, y, down);
    onMoved(m_sliderBar);
}

void VerticalSlider::layout(skin::Base * /*skin*/)
{
    // the track's column: the whole width, less a value text's column
    placeChildSize(m_sliderBar, getTrackBounds().w, 15);
    // a resize moves the thumb's travel, so put it back at the value
    updateBarFromValue();
    placeValueText();
}

//void VerticalSlider::render(skin::Base *skin)
//{
////    skin->drawSlider(this, skin::Generate, false, m_clampToNotches?m_numNotches:0, m_sliderBar->getHeight());
//    skin->drawControl(this);
//}

}//namespace controls
}//namespace gweni
