/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_colorcontrols_h_
#define _gweni_controls_colorcontrols_h_

#include <gweni/controls/base.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{
//! The saturation/value box of an HSV picker, drawn in one hue. Its selection
//! is a hue, saturation and value in floats, so a colour set on it is kept
//! exactly and its cursor follows the box's size.
class GWENI_EXPORT ColorLerpBox: public controls::Base
{
public:

    GWENI_CONTROL(ColorLerpBox, controls::Base);

public:
    //! The colour at local (x, y) in the box's hue.
    gweni::Color  getColorAtPos(int x, int y);
    //! Selects \p color: its hue, and with !onlyHue its saturation and value.
    //! A colour with no hue (black, a grey) keeps the box's hue. Fires
    //! onSelectionChanged when \p doEvents and the selection changed.
    void         setColor(gweni::Color color, bool onlyHue=true, bool doEvents=true);
    //! Draws the box in \p hue (degrees), keeping its saturation and value.
    void         setHue(float hue, bool doEvents=true);
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseClickLeft(int x, int y, bool down) override;
    //! The selected hue, saturation and value as a colour.
    gweni::Color  getSelectedColor();

    //! The hue (degrees) the box is drawn in, truncated.
    int getHue() const { return static_cast<int>(m_hue); }
    //! The hue at full saturation and value: the box's top-right corner.
    gweni::Color getHueColor() const;
    //! The cursor, in local pixels (0..width, 0..height): the saturation
    //! across the box and the value up it, at the box's current size.
    gweni::Point getCursorPos() const;

    //! Fired by a user change of the selection, and by setColor/setHue with
    //! doEvents when they changed it.
    event::Caller onSelectionChanged;

protected:
    //! Sets the selection; repaints if what the skin draws moved, and fires
    //! onSelectionChanged if \p doEvents and anything changed.
    void select(float hue, float saturation, float value, bool doEvents);

    bool m_depressed;
    float m_hue=0;
    float m_saturation=0;
    float m_value=1;

};


//! The hue strip of an HSV picker. Its selection is a hue in degrees, kept
//! as a float, its row derived from the strip's current height.
class GWENI_EXPORT ColorSlider: public controls::Base
{
public:
    GWENI_CONTROL(ColorSlider, controls::Base);

public:
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseClickLeft(int x, int y, bool down) override;
    //! The selected hue at full saturation and value.
    gweni::Color  getSelectedColor();
    gweni::Color  getColorAtHeight(int y);
    //! Selects \p color's hue; a colour with no hue (black, a grey) keeps
    //! the strip's. Fires onSelectionChanged when \p doEvents and it changed.
    void         setColor(gweni::Color color, bool doEvents=true);
    //! Selects \p hue (degrees).
    void         setHue(float hue, bool doEvents=true);
    //! The selected hue, degrees.
    float getHue() const { return m_hue; }

    //! The selected row, in local pixels (0..height): hue/360*height, rounded.
    int getSelectedDist() const;

    //! Fired by a user change of the hue, and by setColor/setHue with
    //! doEvents when they changed it.
    event::Caller onSelectionChanged;

protected:

    float m_hue=0;
    bool m_depressed;

};

}

}
#endif//_gweni_controls_colorcontrols_h_
