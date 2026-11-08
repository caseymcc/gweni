/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_coloredit_h_
#define _gweni_controls_coloredit_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{

class ColorButton;
class HSVColorPicker;
class Label;
class Popup;
class TextBox;

//! How a ColorEdit's fields show its colour.
enum class ColorEditMode
{
    //! Four TextBoxNumeric fields R, G, B and A, 0-255 each.
    Rgba,
    //! One TextBox holding "#RRGGBB", or "#RRGGBBAA" with alpha.
    Hex
};

//! A colour field (imgui's ColorEdit3/ColorEdit4): a ColorButton swatch and,
//! right of it, either four R, G, B, A fields or one hex field (setMode()).
//! A click on the swatch opens an HSVColorPicker in a Popup below it (flipped
//! and kept inside the canvas like any popup); a second click, Escape or a
//! click outside closes it.
//!
//! The swatch and the fields always show the same colour, and the open picker
//! shows it as nearly as its HSV box can (its own R/G/B boxes may read a
//! channel or two off after the round trip):
//! - Typing applies a field as soon as it holds a whole valid value: a number
//!   0-255 in an RGBA field, six hex digits (eight with alpha, the '#'
//!   optional) in the hex field. Until then nothing else changes.
//! - When the field loses the focus (Return moves it on, Tab, a click
//!   elsewhere) an RGBA field's number is clamped into 0-255 and applied, and
//!   every field is shown the colour it holds again: so text that is not a
//!   value (an empty field, "1.5", "#12") is reverted.
//! - A drag in the open picker applies its red, green and blue, keeping the
//!   alpha.
//!
//! An edit marks the swatch's StateChange_Background and sets the text of the
//! other fields (and the picker), so a field whose text stays as it is marks
//! nothing: an RGBA field edit marks the swatch and that field alone.
//! onColorChanged fires once per change of getColor(), never for a no-op (the
//! same colour set again, the same number typed again).
//!
//! The popup is made on the canvas by the first click and deleted with the
//! ColorEdit, as a Popup's creator does (popup.h).
class GWENI_EXPORT ColorEdit: public Base
{
public:

    GWENI_CONTROL(ColorEdit, Base);

    //! A row of its swatch, labels and fields, which draw themselves.
    GWENI_DRAWS_NOTHING

public:

    //! Deletes the popup it made on the canvas.
    ~ColorEdit() override;

public:

    //! Shows \p color in the swatch, the fields and the open picker; with
    //! \p doEvents, onColorChanged fires if it is not the colour it had.
    //! Without alpha (setAlphaEnabled(false)) the alpha is taken as 255.
    virtual void setColor(const gweni::Color &color, bool doEvents=true);
    const gweni::Color &getColor() const { return m_color; }

    //! Rgba (the default) or Hex; a new mode replaces the fields.
    virtual void setMode(ColorEditMode mode);
    ColorEditMode getMode() const { return m_mode; }

    //! With alpha off (it is on by default) the A field is hidden, the hex
    //! field takes six digits and the colour's alpha is 255 (onColorChanged
    //! fires if that changed it).
    virtual void setAlphaEnabled(bool enabled);
    bool isAlphaEnabled() const { return m_alpha; }

    ColorButton *getSwatch() const { return m_swatch; }
    //! An RGBA field, 0-3 for R, G, B and A, or the hex field (\p channel 0),
    //! by the mode; null for any other channel.
    TextBox *getField(int channel) const;
    //! The text a field shows for the colour: "122", or "#7AA2F7".
    String fieldText(int channel) const;

    //! Opens the picker below the swatch, or closes it, as a swatch click
    //! does.
    virtual void togglePicker();
    bool isPickerOpen() const;
    //! The popup holding the picker, and the picker; null until the first
    //! open.
    Popup *getPopup() const { return m_popup; }
    HSVColorPicker *getPicker() const { return m_picker; }

    //! Fired once per change of getColor(), from a field, the picker or
    //! setColor().
    gweni::event::Caller onColorChanged;

protected:

    void onSwatchPressed(event::Info info);
    void onFieldTyped(event::Info info);
    void onFieldLostFocus(event::Info info);
    void onPickerChanged(event::Info info);

    //! Takes \p color and shows it everywhere but in \p source (the field
    //! typed in, or the picker), firing onColorChanged with \p doEvents.
    //! Returns false, doing nothing, when it is the colour it has.
    bool applyColor(gweni::Color color, Base *source, bool doEvents);
    //! The colour field \p channel's text stands for, in \p color; \p
    //! lenient (on losing the focus) clamps a number out of range.
    bool parseField(int channel, const String &text, gweni::Color &color, bool lenient) const;
    //! Deletes the fields and makes those of the mode.
    void buildFields();
    int channelOf(Base *field) const;

    gweni::Color m_color=gweni::Color(0, 0, 0, 255);
    ColorEditMode m_mode=ColorEditMode::Rgba;
    bool m_alpha=true;
    //! Set while it gives the picker a colour, whose onColorChanged it ignores.
    bool m_settingPicker=false;

    ColorButton *m_swatch=nullptr;
    //! R, G, B, A in Rgba mode; the hex field in m_fields[0] in Hex mode.
    TextBox *m_fields[4]={nullptr, nullptr, nullptr, nullptr};
    Label *m_labels[4]={nullptr, nullptr, nullptr, nullptr};
    Popup *m_popup=nullptr;
    HSVColorPicker *m_picker=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_coloredit_h_
