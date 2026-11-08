/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_colorbutton_h_
#define _gweni_controls_colorbutton_h_

#include <gweni/controls/button.h>

namespace gweni
{
namespace controls
{

//! A clickable colour swatch (imgui's ColorButton): its colour in a rounded
//! border, over a checkerboard where it is translucent, with a button's hover,
//! press and disabled looks (only its border changes with them) and onPress.
//! Its tooltip is the colour's hex value (toHex()), set only when the colour
//! changes. It has no caption: it is a Button with an empty one.
//!
//! The textured skin draws it in one primitive slot, so a hover, a press or a
//! new colour rewrites that slot only. A ColorEdit is a ColorButton with
//! fields beside it; property::ColorSelector puts one in its text box.
class GWENI_EXPORT ColorButton: public Button
{
public:

    GWENI_CONTROL(ColorButton, Button);

public:

    //! Marks StateChange_Background and updates the tooltip when \p color is
    //! not the one it has; the same colour again marks nothing.
    virtual void setColor(const gweni::Color &color);
    const gweni::Color &getColor() const { return m_color; }

    //! "#RRGGBB", or "#RRGGBBAA" with \p alpha, in upper-case hex digits.
    static String toHex(const gweni::Color &color, bool alpha);
    //! The colour "#RRGGBB" (alpha 255) or, with \p alpha only, "#RRGGBBAA"
    //! stands for, in \p color: the '#' is optional, the digits are either
    //! case and nothing else may be in \p text (no spaces). Anything else, a
    //! 3-digit shorthand included, returns false and leaves \p color as it
    //! was.
    static bool fromHex(const String &text, gweni::Color &color, bool alpha);

protected:

    gweni::Color m_color;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_colorbutton_h_
