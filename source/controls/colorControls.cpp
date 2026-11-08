/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 *
  * The colourspace conversion functions al_color_hsv_to_rgb  &al_color_rgb_to_hsv are
  * from the Allegro 5 colour library.
  * License: http://alleg.sourceforge.net/license.html
 */


#include <gweni/utility.h>
#include <gweni/controls/colorControls.h>
#include <cmath>

namespace gweni
{
namespace controls
{

static void al_color_hsv_to_rgb(float hue, float saturation, float value,
    float *red, float *green, float *blue)
{
    int d;
    float e, a, b, c;
    hue=std::fmod(hue, 360.f);
    if(hue < 0) hue+=360.f;
    d=hue / 60.f;
    e=hue / 60.f - d;
    a=value  *(1.f - saturation);
    b=value  *(1.f - e  *saturation);
    c=value  *(1.f - (1.f - e)  *saturation);
    switch(d)
    {
    default:
    case 0: *red=value, *green=c, *blue=a;     return;
    case 1: *red=b, *green=value, *blue=a;     return;
    case 2: *red=a, *green=value, *blue=c;     return;
    case 3: *red=a, *green=b, *blue=value; return;
    case 4: *red=c, *green=a, *blue=value; return;
    case 5: *red=value, *green=a, *blue=b;     return;
    }
}

static void al_color_rgb_to_hsv(float red, float green, float blue,
    float *hue, float *saturation, float *value)
{
    float a, b, c, d;
    if(red > green)
        if(red > blue)
            if(green > blue)
                a=red, b=green - blue, c=blue, d=0;
            else
                a=red, b=green - blue, c=green, d=0;
        else
            a=blue, b=red - green, c=green, d=4;
    else
    {
        if(red > blue)
            a=green, b=blue - red, c=blue, d=2;
        else
        {
            if(green > blue)
                a=green, b=blue - red, c=red, d=2;
            else
                a=blue, b=red - green, c=red, d=4;
        }
    }

    if(a == c)
        *hue=0;
    else
        *hue=60  *(d + b / (a - c));
    if(*hue < 0)
        *hue+=360;
    if(*hue > 360)
        *hue-=360;

    if(a == 0)
        *saturation=0;
    else
        *saturation=(a - c) / a;
    *value=a;
}

//! One channel in 0..1 as 0..255, rounded and clamped: converting an
//! out-of-range float to unsigned char is undefined.
static inline unsigned char toChannel(float value)
{
    return static_cast<unsigned char>(clamp(static_cast<int>(std::lround(value*255.f)), 0, 255));
}

static inline Color HSVToColor(float h, float s, float v)
{
    float r, g, b;
    al_color_hsv_to_rgb(h, s, v, &r, &g, &b);
    return Color(toChannel(r), toChannel(g), toChannel(b), 255);
}

static inline HSV RGBtoHSV(int r, int g, int b)
{
    HSV hsv;
    // al_color_rgb_to_hsv takes channels in 0-1: given 0-255 its value was
    // 255 times too large, which put the lerp box cursor on its top edge
    al_color_rgb_to_hsv(r/255.f, g/255.f, b/255.f, &hsv.h, &hsv.s, &hsv.v);
    return hsv;
}

//! True if \p hsv's hue means nothing: black and greys come out as hue 0.
static inline bool hasNoHue(const HSV &hsv)
{
    return hsv.s <= 0.f || hsv.v <= 0.f;
}

//! \p value/\p size, clamped to 0..1 (0 for an empty size).
static inline float fraction(int value, int size)
{
    if(size <= 0)
        return 0.f;
    return clamp(static_cast<float>(value)/size, 0.f, 1.f);
}

GWENI_CONTROL_CONSTRUCTOR(ColorLerpBox)
{
    m_depressed=false;
    setColor(gweni::Color(255, 128, 0, 255), false, false);
    setSize(128, 128);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setMouseInputEnabled(true);
}

gweni::Color ColorLerpBox::getSelectedColor()
{
    return HSVToColor(m_hue, m_saturation, m_value);
}

gweni::Color ColorLerpBox::getHueColor() const
{
    return HSVToColor(m_hue, 1, 1);
}

gweni::Point ColorLerpBox::getCursorPos() const
{
    return gweni::Point(static_cast<int>(std::lround(m_saturation*getWidth())),
        static_cast<int>(std::lround((1.f-m_value)*getHeight())));
}

void ColorLerpBox::select(float hue, float saturation, float value, bool doEvents)
{
    if(hue == m_hue && saturation == m_saturation && value == m_value)
        return;

    const float oldHue=m_hue;
    const gweni::Point oldCursor=getCursorPos();

    m_hue=hue;
    m_saturation=saturation;
    m_value=value;

    // the skin draws the hue and the cursor: repaint only when one moved
    const gweni::Point cursor=getCursorPos();

    if(m_hue != oldHue || cursor.x != oldCursor.x || cursor.y != oldCursor.y)
        redraw();

    if(doEvents)
        onSelectionChanged.call(this);
}

void ColorLerpBox::setColor(gweni::Color color, bool onlyHue, bool doEvents)
{
    const HSV hsv=RGBtoHSV(color.r, color.g, color.b);
    const float hue=hasNoHue(hsv)?m_hue:hsv.h;

    if(onlyHue)
        select(hue, m_saturation, m_value, doEvents);
    else
        select(hue, hsv.s, hsv.v, doEvents);
}

void ColorLerpBox::setHue(float hue, bool doEvents)
{
    select(hue, m_saturation, m_value, doEvents);
}

void ColorLerpBox::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    if(m_depressed)
    {
        const gweni::Point local=canvasPosToLocal(gweni::Point(x, y));

        select(m_hue, fraction(local.x, getWidth()), 1.f-fraction(local.y, getHeight()), true);
    }
}

void ColorLerpBox::onMouseClickLeft(int x, int y, bool down)
{
    m_depressed=down;

    if(down)
        gweni::MouseFocus=this;
    else
        gweni::MouseFocus=nullptr;

    onMouseMoved(x, y, 0, 0);
}

gweni::Color ColorLerpBox::getColorAtPos(int x, int y)
{
    return HSVToColor(m_hue, fraction(x, getWidth()), 1.f-fraction(y, getHeight()));
}

GWENI_CONTROL_CONSTRUCTOR(ColorSlider)
{
    setSize(32, 128);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setMouseInputEnabled(true);
    m_depressed=false;
}

void ColorSlider::onMouseClickLeft(int x, int y, bool down)
{
    m_depressed=down;

    if(down)
        gweni::MouseFocus=this;
    else
        gweni::MouseFocus=nullptr;

    onMouseMoved(x, y, 0, 0);
}

gweni::Color ColorSlider::getColorAtHeight(int y)
{
    const float yPercent=static_cast<float>(y) / getHeight();
    return HSVToColor(yPercent*360, 1, 1);
}

int ColorSlider::getSelectedDist() const
{
    return static_cast<int>(std::lround(m_hue/360.f*getHeight()));
}

void ColorSlider::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    if(m_depressed)
    {
        const gweni::Point local=canvasPosToLocal(gweni::Point(x, y));

        setHue(fraction(local.y, getHeight())*360.f, true);
    }
}

void ColorSlider::setHue(float hue, bool doEvents)
{
    if(hue == m_hue)
        return;

    const int oldDist=getSelectedDist();

    m_hue=hue;

    // the skin draws the marker at the row: repaint only when it moved
    if(getSelectedDist() != oldDist)
        redraw();

    if(doEvents)
        onSelectionChanged.call(this);
}

void ColorSlider::setColor(gweni::Color color, bool doEvents)
{
    const HSV hsv=RGBtoHSV(color.r, color.g, color.b);

    if(!hasNoHue(hsv))
        setHue(hsv.h, doEvents);
}

gweni::Color ColorSlider::getSelectedColor()
{
    return HSVToColor(m_hue, 1, 1);
}

}//namespace controls
}//namespace gweni
