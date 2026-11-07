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
        // a swatch is the size it is given, and its parent sizes to it
        setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        setSize(32, 32);
        m_color=Color(255, 0, 0, 255);
        m_drawCheckers=true;
    }

public:
    // Each setter repaints the swatch only when the value changes.
    virtual void setColor(gweni::Color color);
    virtual gweni::Color getColor() { return m_color; }

    virtual void setRed(int red);
    virtual void setGreen(int green);
    virtual void setBlue(int blue);
    virtual void setAlpha(int alpha);

    //! Show a translucent colour over a checkerboard; when false the swatch is
    //! always a plain fill.
    virtual void setDrawCheckers(bool should);
    bool getDrawCheckers() const { return m_drawCheckers; }

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
    //! A container of its sliders, text boxes and ColorDisplays.
    GWENI_DRAWS_NOTHING

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
    ///     Each goes through setColor() when the value changes, so the rows
    ///     and the Result swatch follow it.
    virtual void setRed(int red);
    virtual void setGreen(int green);
    virtual void setBlue(int blue);
    virtual void setAlpha(int alpha);
    // \}

    event::Caller onColorChanged;

protected:

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;
    virtual void createControls();
    virtual void onSlidersMoved(event::Info info);
    virtual void onNumericTyped(event::Info info);
    virtual void updateControls();
    virtual void updateColorControls(gweni::String name, gweni::Color col, int sliderVal);
    virtual void createColorControl(gweni::String name, int y);

    gweni::Color m_color;
    //! The Result swatch's group, placed by this control (Placement::Owner):
    //! at resultX, centred vertically.
    Base *m_result=nullptr;
    static const int resultX=180;
};


}//namespace controls
}//namespace gweni

#endif//_gweni_controls_colorpicker_h_
