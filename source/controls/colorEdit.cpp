/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/colorEdit.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/colorButton.h>
#include <gweni/controls/hsvColorPicker.h>
#include <gweni/controls/label.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/textBox.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{

namespace
{
//! A number field's width: three digits and the TextBox padding.
constexpr int NumberWidth=34;
//! The hex field's width: "#RRGGBBAA" and the padding.
constexpr int HexWidth=84;
//! The units between the swatch, a label and the field it names.
constexpr int Gap=4;

const char *const ChannelNames[4]={"R", "G", "B", "A"};

unsigned char &channelRef(gweni::Color &color, int channel)
{
    switch(channel)
    {
    case 0: return color.r;
    case 1: return color.g;
    case 2: return color.b;
    default: return color.a;
    }
}

//! \p text as an optional '-' and 1-6 digits, in \p value.
bool parseInt(const String &text, int &value)
{
    const size_t start=(!text.empty() && text[0] == '-')?1:0;

    if(text.size() == start || text.size()-start > 6)
        return false;
    for(size_t i=start; i < text.size(); ++i)
    {
        if(text[i] < '0' || text[i] > '9')
            return false;
    }
    value=atoi(text.c_str());
    return true;
}
}

GWENI_CONTROL_CONSTRUCTOR(ColorEdit)
{
    // as wide and as high as its swatch and fields
    setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});

    m_swatch=newChild<ColorButton>();
    m_swatch->setDock(DockPosition::Left);
    m_swatch->setColor(m_color);
    m_swatch->onPressCaller.add(this, &ColorEdit::onSwatchPressed);

    buildFields();
}

ColorEdit::~ColorEdit()
{
    // Delayed, as the canvas deletes it with its other children when it is
    // tearing down (PopupDemo).
    if(m_popup)
        m_popup->delayedDelete();
}

void ColorEdit::buildFields()
{
    for(int i=0; i < 4; ++i)
    {
        delete m_labels[i];
        delete m_fields[i];
        m_labels[i]=nullptr;
        m_fields[i]=nullptr;
    }

    // The fields are TextBoxes, which are Fixed: each keeps the 20-unit
    // height and the width asked for here, its text scrolling inside it.
    if(m_mode == ColorEditMode::Hex)
    {
        TextBox *field=newChild<TextBox>();

        field->setDock(DockPosition::Left);
        field->setMargin(Margin(Gap, 0, 0, 0));
        field->setSize(HexWidth, 20);
        m_fields[0]=field;
    }
    else
    {
        for(int i=0; i < 4; ++i)
        {
            Label *label=newChild<Label>();

            label->setText(ChannelNames[i]);
            label->setDock(DockPosition::Left);
            label->setMargin(Margin(Gap, 0, 2, 0));
            label->setTextAlignment(Alignment::Left|Alignment::CenterV);
            m_labels[i]=label;

            TextBox *field=newChild<TextBoxNumeric>();

            field->setDock(DockPosition::Left);
            field->setSize(NumberWidth, 20);
            m_fields[i]=field;
        }
        m_labels[3]->setHidden(!m_alpha);
        m_fields[3]->setHidden(!m_alpha);
    }

    for(int i=0; i < 4; ++i)
    {
        if(!m_fields[i])
            continue;
        m_fields[i]->setText(fieldText(i), false);
        m_fields[i]->setMaxTextLength(m_mode == ColorEditMode::Hex ? 9 : 3);
        m_fields[i]->onTextChangedCaller.add(this, &ColorEdit::onFieldTyped);
        m_fields[i]->onLostFocusCaller.add(this, &ColorEdit::onFieldLostFocus);
    }
}

void ColorEdit::setMode(ColorEditMode mode)
{
    if(mode == m_mode)
        return;

    m_mode=mode;
    buildFields();
}

void ColorEdit::setAlphaEnabled(bool enabled)
{
    if(enabled == m_alpha)
        return;

    m_alpha=enabled;
    if(m_mode == ColorEditMode::Rgba)
    {
        m_labels[3]->setHidden(!m_alpha);
        m_fields[3]->setHidden(!m_alpha);
    }

    gweni::Color color=m_color;

    if(!m_alpha)
        color.a=255;
    if(!applyColor(color, nullptr, true) && m_mode == ColorEditMode::Hex)
        m_fields[0]->setText(fieldText(0), false);
}

TextBox *ColorEdit::getField(int channel) const
{
    if(channel < 0 || channel > 3)
        return nullptr;
    return m_fields[channel];
}

String ColorEdit::fieldText(int channel) const
{
    if(m_mode == ColorEditMode::Hex)
        return ColorButton::toHex(m_color, m_alpha);

    gweni::Color color=m_color;

    return toString(+channelRef(color, channel));
}

int ColorEdit::channelOf(Base *field) const
{
    for(int i=0; i < 4; ++i)
    {
        if(field && field == m_fields[i])
            return i;
    }
    return -1;
}

bool ColorEdit::parseField(int channel, const String &text, gweni::Color &color, bool lenient) const
{
    if(m_mode == ColorEditMode::Hex)
        return ColorButton::fromHex(text, color, m_alpha);

    int value=0;

    if(!parseInt(text, value))
        return false;
    if(!lenient && (value < 0 || value > 255))
        return false;
    channelRef(color, channel)=(unsigned char)clamp(value, 0, 255);
    return true;
}

void ColorEdit::setColor(const gweni::Color &color, bool doEvents)
{
    applyColor(color, nullptr, doEvents);
}

bool ColorEdit::applyColor(gweni::Color color, Base *source, bool doEvents)
{
    if(!m_alpha)
        color.a=255;
    if(color == m_color)
        return false;

    m_color=color;
    m_swatch->setColor(m_color);

    // a field whose text stays the same marks nothing (Label::setText)
    for(int i=0; i < 4; ++i)
    {
        if(m_fields[i] && m_fields[i] != source)
            m_fields[i]->setText(fieldText(i), false);
    }

    if(m_picker && source != m_picker && isPickerOpen())
    {
        m_settingPicker=true;
        m_picker->setColor(m_color);
        m_settingPicker=false;
    }

    if(doEvents)
        onColorChanged.call(this);
    return true;
}

void ColorEdit::onFieldTyped(event::Info info)
{
    const int channel=channelOf(info.controlCaller);

    if(channel < 0)
        return;

    gweni::Color color=m_color;

    if(parseField(channel, m_fields[channel]->getText(), color, false))
        applyColor(color, m_fields[channel], true);
}

void ColorEdit::onFieldLostFocus(event::Info info)
{
    const int channel=channelOf(info.controlCaller);

    if(channel < 0)
        return;

    gweni::Color color=m_color;

    if(parseField(channel, m_fields[channel]->getText(), color, true))
        applyColor(color, m_fields[channel], true);
    // the value it holds, clamped or reverted: "300" shows "255", "" the
    // colour's number again
    m_fields[channel]->setText(fieldText(channel), false);
}

void ColorEdit::onPickerChanged(event::Info)
{
    if(m_settingPicker || !m_picker)
        return;

    gweni::Color color=m_picker->getColor();

    color.a=m_color.a;
    applyColor(color, m_picker, true);
}

bool ColorEdit::isPickerOpen() const
{
    return m_popup && m_popup->isOpen();
}

void ColorEdit::onSwatchPressed(event::Info)
{
    togglePicker();
}

void ColorEdit::togglePicker()
{
    if(isPickerOpen())
    {
        m_popup->close();
        return;
    }

    Canvas *canvas=getCanvas();

    if(!canvas)
        return;

    if(!m_popup)
    {
        m_popup=canvas->newChild<Popup>();
        m_picker=m_popup->newChild<HSVColorPicker>();
        m_picker->setSize(250, 130);
        m_picker->onColorChanged.add(this, &ColorEdit::onPickerChanged);
    }

    // the picker's "before" swatch is the colour it opened with
    m_settingPicker=true;
    m_picker->setColor(m_color, false, true);
    m_settingPicker=false;
    m_popup->open(m_swatch);
}

}//namespace controls
}//namespace gweni
