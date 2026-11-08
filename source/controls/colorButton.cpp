/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/colorButton.h>

namespace gweni
{
namespace controls
{

namespace
{
int hexDigit(char c)
{
    if(c >= '0' && c <= '9')
        return c-'0';
    if(c >= 'a' && c <= 'f')
        return c-'a'+10;
    if(c >= 'A' && c <= 'F')
        return c-'A'+10;
    return -1;
}
}

GWENI_CONTROL_CONSTRUCTOR(ColorButton)
{
    // a swatch is the size it is given, as a ColorDisplay is
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setSize(20, 20);
    setText("");
    m_color=colors::Black;
    setToolTipText(toHex(m_color, false));
}

void ColorButton::setColor(const gweni::Color &color)
{
    if(color == m_color)
        return;

    m_color=color;
    setToolTipText(toHex(m_color, m_color.a != 255));
    addStateChange(StateChange_Background);
}

String ColorButton::toHex(const gweni::Color &color, bool alpha)
{
    static const char digits[]="0123456789ABCDEF";
    const unsigned char channels[4]={color.r, color.g, color.b, color.a};
    String text="#";

    for(int i=0; i < (alpha?4:3); ++i)
    {
        text+=digits[channels[i]>>4];
        text+=digits[channels[i]&0xf];
    }
    return text;
}

bool ColorButton::fromHex(const String &text, gweni::Color &color, bool alpha)
{
    const size_t start=(!text.empty() && text[0] == '#')?1:0;
    const size_t digits=text.size()-start;

    if(digits != 6 && !(alpha && digits == 8))
        return false;

    unsigned char channels[4]={0, 0, 0, 255};

    for(size_t i=0; i < digits; i+=2)
    {
        const int high=hexDigit(text[start+i]);
        const int low=hexDigit(text[start+i+1]);

        if(high < 0 || low < 0)
            return false;
        channels[i/2]=(unsigned char)(high*16+low);
    }
    color=gweni::Color(channels[0], channels[1], channels[2], channels[3]);
    return true;
}

}//namespace controls
}//namespace gweni
