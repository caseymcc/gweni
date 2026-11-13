/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_chip_h_
#define _gweni_controls_chip_h_

#include <gweni/controls/button.h>
#include <gweni/controls/windowButtons.h>

namespace gweni
{
namespace controls
{

//! A chip's colour by meaning (Chip::setTone).
enum class ChipTone
{
    Neutral,
    Accent,
    Success,
    Warning,
    Danger
};

//! A removable chip's x: the window close button's look (the WindowCloseButton
//! renderer, found up its parent types: a cross, icon kind 2, red on hover and
//! press), a Chip::RemoveButtonSize square, as a closable tab's.
class GWENI_EXPORT ChipRemoveButton: public WindowCloseButton
{
    GWENI_CONTROL_INLINE(ChipRemoveButton, WindowCloseButton)
    {
        setTabable(false);
        setKeyboardInputEnabled(false);
    }
};

//! A small rounded label: a task's area, a tag, a pick of a multi-select. Its
//! colour is a tone (setTone(), by meaning) or an entry of the theme's palette
//! (setPaletteIndex(i), used as i mod PaletteSize, for colours an app hands
//! out by index). Each tone and palette entry is a theme surface of its own
//! (ChipNeutral .. ChipDanger, ChipPalette0 .. ChipPalette7: the fill and
//! border) with its text colour (ChipNeutralText .. ChipPalette7Text in the
//! skin's text colours), so a theme sets them by name; the hover and press
//! looks are derived from the entry's surface (its fill moved toward its
//! border), not named surfaces of their own.
//!
//! Not clickable (the default), it takes no press: no hover look, a click and
//! Space do nothing (it keeps the mouse for its tooltip). setClickable(true)
//! gives it hover and press looks and fires onPressCaller on a click, for an
//! app that navigates from it. setRemovable(true) adds a ChipRemoveButton in
//! a square kept in its right padding, so its width does not follow the
//! button; a click on it fires onRemove. The chip never deletes itself: the
//! app removes it, with delayedDelete() from the handler (never delete it or
//! setRemovable(false) there: the button is still in its click). A disabled
//! chip hides the button (its square stays) and fires no onRemove, and draws
//! the disabled colours in its own shape.
//!
//! A text, tone or palette change marks that chip alone (its slot, its
//! Label's glyphs), and lays out the chip and the way down to it only when
//! its size changes; hover and press redraw its slot (and the x's) only.
class GWENI_EXPORT Chip: public Button
{
public:

    GWENI_CONTROL(Chip, Button);

public:

    //! The theme's palette entries (ChipPalette0..7).
    static constexpr int PaletteSize=8;
    //! The side of the remove button's square, in units.
    static constexpr int RemoveButtonSize=12;
    //! Units each side of the text, and above and below it.
    static constexpr int PaddingX=8;
    static constexpr int PaddingY=2;

    //! A tone; the chip leaves the palette.
    void setTone(ChipTone tone);
    ChipTone getTone() const { return m_tone; }

    //! Palette entry \p index mod PaletteSize (a negative index counts from
    //! the end); the chip leaves its tone.
    void setPaletteIndex(int index);
    //! The palette entry it shows, or -1 for a tone.
    int getPaletteIndex() const { return m_paletteIndex; }

    void setClickable(bool clickable);
    bool isClickable() const { return m_clickable; }

    void setRemovable(bool removable);
    bool isRemovable() const { return m_removeButton != nullptr; }
    //! The remove button, or null when the chip is not removable.
    ChipRemoveButton *getRemoveButton() const { return m_removeButton; }

    //! Fired by a click on the remove button, the chip as its control.
    gweni::event::Caller onRemove;

    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseClickRight(int x, int y, bool down) override;
    void onMouseDoubleClickLeft(int x, int y) override;
    bool onKeySpace(bool down) override;
    void acceleratePressed() override;

    //! The text colour of its tone or palette entry (the skin's
    //! Colors.Chip), the disabled one while disabled.
    void updateColors() override;

    //! Whether it draws a hover or press look: clickable and enabled.
    bool showsInteraction() const { return m_clickable && !m_disabled; }

    void arrange(skin::Base *skin, Dim dim) override;

private:
    //! The padding: PaddingX, PaddingY, and the remove square on the right.
    void updatePadding();
    void onRemovePressed(event::Info info);

    ChipTone m_tone=ChipTone::Neutral;
    int m_paletteIndex=-1;
    bool m_clickable=false;
    ChipRemoveButton *m_removeButton=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_chip_h_
