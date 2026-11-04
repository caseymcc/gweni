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
    void render(gweni::skin::Base *skin) override;
    gweni::Color  getColorAtPos(int x, int y);
    void         setColor(gweni::Color color, bool onlyHue=true);
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseClickLeft(int x, int y, bool down) override;
    gweni::Color  getSelectedColor();

    event::Caller onSelectionChanged;

protected:

    gweni::Point cursorPos;
    bool m_depressed;
    int m_hue;

};


class GWENI_EXPORT ColorSlider: public controls::Base
{
public:
    GWENI_CONTROL(ColorSlider, controls::Base);

public:
    void render(gweni::skin::Base *skin) override;
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseClickLeft(int x, int y, bool down) override;
    gweni::Color  getSelectedColor();
    gweni::Color  getColorAtHeight(int y);
    void         setColor(gweni::Color color);

    event::Caller onSelectionChanged;

protected:

    int m_selectedDist;
    bool m_depressed;

};


}

}
#endif//_gweni_controls_colorcontrols_h_
