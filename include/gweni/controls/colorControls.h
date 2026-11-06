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
class GWENI_EXPORT ColorLerpBox: public controls::Base
{
public:

    GWENI_CONTROL(ColorLerpBox, controls::Base);

public:
    gweni::Color  getColorAtPos(int x, int y);
    void         setColor(gweni::Color color, bool onlyHue=true);
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseClickLeft(int x, int y, bool down) override;
    gweni::Color  getSelectedColor();

    //! The hue (degrees) the box is drawn in.
    int getHue() const { return m_hue; }
    //! The hue at full saturation and value: the box's top-right corner.
    gweni::Color getHueColor() const;
    //! The cursor, in local pixels (0..width, 0..height).
    const gweni::Point &getCursorPos() const { return cursorPos; }

    event::Caller onSelectionChanged;

protected:

    gweni::Point cursorPos;
    bool m_depressed;
    int m_hue=0;

};


class GWENI_EXPORT ColorSlider: public controls::Base
{
public:
    GWENI_CONTROL(ColorSlider, controls::Base);

public:
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseClickLeft(int x, int y, bool down) override;
    gweni::Color  getSelectedColor();
    gweni::Color  getColorAtHeight(int y);
    void         setColor(gweni::Color color);

    //! The selected row, in local pixels (0..height); hue = row/height*360.
    int getSelectedDist() const { return m_selectedDist; }

    event::Caller onSelectionChanged;

protected:

    int m_selectedDist=0;
    bool m_depressed;

};


}

}
#endif//_gweni_controls_colorcontrols_h_
