/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_colorpicker_h_
#define _gweni_controls_colorpicker_h_

#include <gweni/controls/base.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{
namespace internal
{

class GWENI_EXPORT ColorDisplay: public controls::Base
{
public:

    GWENI_CONTROL_INLINE(ColorDisplay, controls::Base)
    {
        setSize(32, 32);
        m_color=Color(255, 0, 0, 255);
        m_drawCheckers=true;
    }

public:
    virtual void setColor(gweni::Color color) { m_color=color; }
    virtual gweni::Color getColor() { return m_color; }

    virtual void setRed(int red) { m_color.r=red; }
    virtual void setGreen(int green) { m_color.g=green; }
    virtual void setBlue(int blue) { m_color.b=blue; }
    virtual void setAlpha(int alpha) { m_color.a=alpha; }

    virtual void setDrawCheckers(bool should) { m_drawCheckers=should; }

protected:

    gweni::Color m_color;
    bool m_drawCheckers;
};

}//namespace internal

class GWENI_EXPORT ColorPicker: public Base
{
public:

    GWENI_CONTROL(ColorPicker, Base);

public:
    virtual void setAlphaVisible(bool visible);

    virtual void        setColor(gweni::Color color);
    virtual gweni::Color getColor() { return m_color; }

    /// \sect{By name}
    ///     Set colour channel value. Range: 0-255.
    //
    int         getColorByName(gweni::String colorName);
    void        setColorByName(gweni::String colorName, int colorValue);
    gweni::String getColorFromName(gweni::String name);
    // \}

    /// \sect{Set by value}
    ///     Set colour channel value. Range: 0-255.
    //
    virtual void setRed(int red) { m_color.r=red; }
    virtual void setGreen(int green) { m_color.g=green; }
    virtual void setBlue(int blue) { m_color.b=blue; }
    virtual void setAlpha(int alpha) { m_color.a=alpha; }
    // \}

    event::Caller onColorChanged;

protected:

    void layout(skin::Base *skin) override;
    virtual void createControls();
    virtual void onSlidersMoved(event::Info info);
    virtual void onNumericTyped(event::Info info);
    virtual void updateControls();
    virtual void updateColorControls(gweni::String name, gweni::Color col, int sliderVal);
    virtual void createColorControl(gweni::String name, int y);

    gweni::Color m_color;
};


}//namespace controls
}//namespace gweni

#endif//_gweni_controls_colorpicker_h_
