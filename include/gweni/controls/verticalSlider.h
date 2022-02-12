/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_verticalslider_h_
#define _gweni_controls_verticalslider_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/controls/dragger.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/slider.h>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT VerticalSlider: public Slider
{
    GWENI_CONTROL(VerticalSlider, Slider);

public:
    void layout(skin::Base *skin) override;
    //void render(skin::Base *skin) override;

    float calculateValue() override;
    void  updateBarFromValue() override;
    void  onMouseClickLeft(int x, int y, bool down) override;

};


}
}
#endif//_gweni_controls_verticalslider_h_
