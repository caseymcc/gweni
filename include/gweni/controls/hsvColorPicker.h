/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_hsvcolorpicker_h_
#define _gweni_controls_hsvcolorpicker_h_

#include <gweni/controls/base.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/colorControls.h>
#include <gweni/controls/colorPicker.h>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT HSVColorPicker: public controls::Base
{
public:

    GWENI_CONTROL(HSVColorPicker, controls::Base);

public:
    gweni::Color getColor();
    gweni::Color getDefaultColor()
    {
        return m_before->getColor();
    }

    void setColor(gweni::Color color, bool onlyHue=false, bool reset=false);

    void colorBoxChanged(event::Info info);
    void colorSliderChanged(event::Info info);
    void onNumericTyped(event::Info info);

    void updateControls(gweni::Color newColor);

    event::Caller onColorChanged;

protected:

    ColorLerpBox *m_lerpBox;
    ColorSlider *m_colorSlider;
    internal::ColorDisplay *m_before;
    internal::ColorDisplay *m_after;
};


}
}
#endif//_gweni_controls_hsvcolorpicker_h_
