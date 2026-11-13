/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/chip.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{

// Its preferred size (Elastic), its text and padding's, whatever a Button's
// default request says; the text centred down it, from its left padding.
GWENI_CONTROL_CONSTRUCTOR(Chip)
{
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    // no request of its own (a Button asks for 100x20): a layout cell takes
    // the larger of the request and the preferred size
    setSize(0, 0);
    setTextPadding(Padding(0, 0, 0, 0));
    setDefaultTextAlignment(Alignment::Left|Alignment::CenterV);
    // a new text of the same size redraws the glyphs and lays nothing out
    m_text->setSameSizeKeepsLayout(true);
    updatePadding();
}

void Chip::setTone(ChipTone tone)
{
    if((m_paletteIndex < 0) && (tone == m_tone))
        return;

    m_tone=tone;
    m_paletteIndex=-1;
    redraw();
}

void Chip::setPaletteIndex(int index)
{
    index%=PaletteSize;
    if(index < 0)
        index+=PaletteSize;
    if(index == m_paletteIndex)
        return;

    m_paletteIndex=index;
    redraw();
}

void Chip::setClickable(bool clickable)
{
    if(clickable == m_clickable)
        return;

    m_clickable=clickable;
    setKeyboardInputEnabled(clickable);
    if(!clickable)
    {
        // a press it was holding is let go: the release, which a chip that
        // is not clickable no longer takes, would have given the mouse back
        setDepressed(false);
        if(MouseFocus == this)
            MouseFocus=nullptr;
    }
    redraw();
}

void Chip::setRemovable(bool removable)
{
    if(removable == isRemovable())
        return;

    if(removable)
    {
        m_removeButton=newChild<ChipRemoveButton>();
        m_removeButton->setPlacement(Placement::Owner);
        m_removeButton->setSize(RemoveButtonSize, RemoveButtonSize);
        m_removeButton->onPressCaller.add(this, &Chip::onRemovePressed);
        m_removeButton->setHidden(isDisabled());
    }
    else
    {
        delete m_removeButton;
        m_removeButton=nullptr;
    }
    updatePadding();
}

// The remove square sits in the right padding, so the text never runs under
// it and the chip is as wide whether or not the mouse is on it.
void Chip::updatePadding()
{
    Padding padding(PaddingX, PaddingY, PaddingX, PaddingY);

    if(m_removeButton)
        padding.right=PaddingX/2+RemoveButtonSize+PaddingX/4;
    setPadding(padding);
}

// The remove square is placed along each axis before the chip arranges its
// children (a closable tab's pattern): at the start of the right padding,
// centred down the chip.
void Chip::arrange(skin::Base *skin, Dim dim)
{
    if(m_removeButton)
    {
        const Rect &bounds=m_removeButton->getBounds();

        if(dim == Dim::X)
            placeChild(m_removeButton, Rect(getWidth()-getPadding().right+PaddingX/4, bounds.y,
                RemoveButtonSize, bounds.h));
        else
            placeChild(m_removeButton, Rect(bounds.x, (getHeight()-RemoveButtonSize)/2, bounds.w,
                RemoveButtonSize));
    }

    ParentClass::arrange(skin, dim);
}

void Chip::onRemovePressed(event::Info /*info*/)
{
    if(isDisabled())
        return;
    onRemove.call(this);
}

// A chip that is not clickable takes no press: the mouse stays its own (its
// tooltip), but nothing is held, pressed or fired.
void Chip::onMouseClickLeft(int x, int y, bool down)
{
    if(!m_clickable)
        return;
    ParentClass::onMouseClickLeft(x, y, down);
}

void Chip::onMouseClickRight(int x, int y, bool down)
{
    if(!m_clickable)
        return;
    ParentClass::onMouseClickRight(x, y, down);
}

void Chip::onMouseDoubleClickLeft(int x, int y)
{
    if(!m_clickable)
        return;
    ParentClass::onMouseDoubleClickLeft(x, y);
}

bool Chip::onKeySpace(bool down)
{
    if(!m_clickable)
        return false;
    return ParentClass::onKeySpace(down);
}

void Chip::acceleratePressed()
{
    if(m_clickable)
        ParentClass::acceleratePressed();
}

// Run from redraw(), so a disabled change settles the remove button too: a
// disabled chip shows none and fires no onRemove, as a disabled closable tab
// shows no close button (its square stays in the padding, so the width holds).
void Chip::updateColors()
{
    const auto &colors=getSkin()->Colors;

    if(m_removeButton)
        m_removeButton->setHidden(isDisabled());

    if(isDisabled())
        return setTextColor(colors.Button.Disabled);
    if(m_paletteIndex >= 0)
        return setTextColor(colors.Chip.Palette[m_paletteIndex]);

    switch(m_tone)
    {
    case ChipTone::Accent: return setTextColor(colors.Chip.Accent);
    case ChipTone::Success: return setTextColor(colors.Chip.Success);
    case ChipTone::Warning: return setTextColor(colors.Chip.Warning);
    case ChipTone::Danger: return setTextColor(colors.Chip.Danger);
    case ChipTone::Neutral: break;
    }
    setTextColor(colors.Chip.Neutral);
}

}//namespace controls
}//namespace gweni
