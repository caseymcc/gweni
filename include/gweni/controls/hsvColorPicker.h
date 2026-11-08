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
    //! A container of its ColorLerpBox, ColorSlider, ColorDisplays and text boxes.
    GWENI_DRAWS_NOTHING

    //! The colour last set or chosen, alpha included (the picker has no
    //! alpha field: it carries the alpha it was given).
    gweni::Color getColor() { return m_color; }
    gweni::Color getDefaultColor()
    {
        return m_before->getColor();
    }

    //! Shows \p color: the box, the slider, the R, G, B fields and the
    //! "after" swatch, and with \p reset the "before" one. With \p onlyHue
    //! only the hue moves, and the colour is the box's at its saturation and
    //! value. A programmatic set: it fires no onColorChanged.
    void setColor(gweni::Color color, bool onlyHue=false, bool reset=false);

    void colorBoxChanged(event::Info info);
    void colorSliderChanged(event::Info info);
    void onNumericTyped(event::Info info);

    void updateControls(gweni::Color newColor);

    //! Fired once per change of the colour the user makes: a drag in the box
    //! or the slider, a value typed into R, G or B.
    event::Caller onColorChanged;

protected:
    //! Takes the box's selected colour (keeping the alpha) after a user
    //! change, and fires onColorChanged if the colour changed.
    void selectionChanged();

    gweni::Color m_color;

    ColorLerpBox *m_lerpBox;
    ColorSlider *m_colorSlider;
    internal::ColorDisplay *m_before;
    internal::ColorDisplay *m_after;
};


}
}
#endif//_gweni_controls_hsvcolorpicker_h_
