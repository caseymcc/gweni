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

static inline Color HSVToColor(float h, float s, float v)
{
    float r, g, b;
    al_color_hsv_to_rgb(h, s, v, &r, &g, &b);
    return Color(r*255.f, g*255.f, b*255.f, 255);
}

static inline HSV RGBtoHSV(int r, int g, int b)
{
    HSV hsv;
    al_color_rgb_to_hsv(r, g, b, &hsv.h, &hsv.s, &hsv.v);
    return hsv;
}

GWENI_CONTROL_CONSTRUCTOR(ColorLerpBox)
{
    setColor(gweni::Color(255, 128, 0, 255));
    setSize(128, 128);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setMouseInputEnabled(true);
    m_depressed=false;
}

//// Find a place to put this? color member?
//static gweni::Color LerpColor(gweni::Color &toColor, gweni::Color &fromColor, float amount)
//{
//    gweni::Color colorDelta = toColor-fromColor;
//    colorDelta.r *= amount;
//    colorDelta.g *= amount;
//    colorDelta.b *= amount;
//    gweni::Color newColor = fromColor+colorDelta;
//    return newColor;
//}

gweni::Color ColorLerpBox::getSelectedColor()
{
    return getColorAtPos(cursorPos.x, cursorPos.y);
}

void ColorLerpBox::setColor(gweni::Color color, bool onlyHue)
{
    HSV hsv=RGBtoHSV(color.r, color.g, color.b);
    m_hue=hsv.h;

    if(!onlyHue)
    {
        cursorPos.x=hsv.s*getWidth();
        cursorPos.y=(1-hsv.v)*getHeight();
    }

    onSelectionChanged.call(this);
}

void ColorLerpBox::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    if(m_depressed)
    {
        cursorPos=canvasPosToLocal(gweni::Point(x, y));

        // Do we have clamp?
        cursorPos.x=clamp(cursorPos.x, 0, getWidth());
        cursorPos.y=clamp(cursorPos.y, 0, getHeight());

        onSelectionChanged.call(this);
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
    float xPercent=static_cast<float>(x) / getWidth();
    float yPercent=1.f - static_cast<float>(y) / getHeight();
    gweni::Color result=HSVToColor(m_hue, xPercent, yPercent);
    result.a=255;
    return result;
}

void ColorLerpBox::render(gweni::skin::Base *skin)
{
    // Is there any way to move this into skin? Not for now, no idea how we'll
    // "actually" render these
    ParentClass::render(skin);

    size_t primitiveIndex=0;
    size_t primitives=(getWidth()*getHeight())+4+8;

    if(m_primitiveIds.size() < primitives)
        enlargePrimitiveIds(this, m_primitiveIds, primitives);

    for(int x=0; x < getWidth(); x++)
    {
        for(int y=0; y < getHeight(); y++)
        {
            skin->getRenderer()->setDrawColor(getColorAtPos(x, y));
            skin->getRenderer()->drawPixel(&m_primitiveIds[primitiveIndex], x, y, getZIndex());
            primitiveIndex++;
        }
    }

    skin->getRenderer()->setDrawColor(gweni::Color(0, 0, 0, 255));
    skin->getRenderer()->drawLinedRect(&m_primitiveIds[primitiveIndex], getRenderBounds(), getZIndex());
    gweni::Color selected=getSelectedColor();

    if((selected.r+selected.g+selected.b)/3 < 170)
        skin->getRenderer()->setDrawColor(gweni::Color(255, 255, 255, 255));
    else
        skin->getRenderer()->setDrawColor(gweni::Color(0, 0, 0, 255));

    gweni::Rect testRect=gweni::Rect(cursorPos.x-3, cursorPos.y-3, 6, 6);
    skin->getRenderer()->drawShavedCornerRect(&m_primitiveIds[primitiveIndex+4], testRect, getZIndex());
}

GWENI_CONTROL_CONSTRUCTOR(ColorSlider)
{
    setSize(32, 128);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setMouseInputEnabled(true);
    m_depressed=false;
}

void ColorSlider::render(gweni::skin::Base *skin)
{
    // Is there any way to move this into skin? Not for now, no idea how we'll
    // "actually" render these
    size_t primitiveIndex=0;
    size_t primitives=getHeight()+5;

    if(m_primitiveIds.size() < primitives)
        enlargePrimitiveIds(this, m_primitiveIds, primitives);

    for(int y=0; y < getHeight(); y++)
    {
        const float yPercent=static_cast<float>(y) / getHeight();
        skin->getRenderer()->setDrawColor(HSVToColor(yPercent*360, 1, 1));
        skin->getRenderer()->drawFilledRect(m_primitiveIds[primitiveIndex], gweni::Rect(5, y, getWidth()-10, 1), getZIndex());
        primitiveIndex++;
    }

    int drawHeight=m_selectedDist-3;
    // Draw our selectors
    skin->getRenderer()->setDrawColor(gweni::Color(0, 0, 0, 255));
    skin->getRenderer()->drawFilledRect(m_primitiveIds[primitiveIndex], gweni::Rect(0, drawHeight+2, getWidth(), 1), getZIndex());
    skin->getRenderer()->drawFilledRect(m_primitiveIds[primitiveIndex+1], gweni::Rect(0, drawHeight, 5, 5), getZIndex());
    skin->getRenderer()->drawFilledRect(m_primitiveIds[primitiveIndex+2], gweni::Rect(getWidth()-5, drawHeight, 5, 5), getZIndex());
    skin->getRenderer()->setDrawColor(gweni::Color(255, 255, 255, 255));
    skin->getRenderer()->drawFilledRect(m_primitiveIds[primitiveIndex+3], gweni::Rect(1, drawHeight+1, 3, 3), getZIndex());
    skin->getRenderer()->drawFilledRect(m_primitiveIds[primitiveIndex+4], gweni::Rect(getWidth()-4, drawHeight+1, 3, 3), getZIndex());
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

void ColorSlider::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    if(m_depressed)
    {
        gweni::Point cursorPos=canvasPosToLocal(gweni::Point(x, y));

        if(cursorPos.y < 0)
            cursorPos.y=0;

        if(cursorPos.y > getHeight())
            cursorPos.y=getHeight();

        m_selectedDist=cursorPos.y;
        onSelectionChanged.call(this);
    }
}

void ColorSlider::setColor(gweni::Color color)
{
    HSV hsv=RGBtoHSV(color.r, color.g, color.b);
    m_selectedDist=hsv.h/360*getHeight();
    onSelectionChanged.call(this);
}

gweni::Color ColorSlider::getSelectedColor()
{
    return getColorAtHeight(m_selectedDist);
}

}//namespace controls
}//namespace gweni
