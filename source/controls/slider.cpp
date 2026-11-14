/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/slider.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(SliderBar)
{
    setTarget(this);
    restrictToParent(true);
}

}//namespace internal


GWENI_CONTROL_CONSTRUCTOR(Slider)
{
    setBounds(gweni::Rect(0, 0, 32, 128));
    m_sliderBar=newChild<internal::SliderBar>();
    // Sized from this control and placed at its value by layout(), so it
    // does not count in the size this control asks for.
    m_sliderBar->setPlacement(Placement::Owner);
    m_sliderBar->onDragged.add(this, &Slider::onMoved);
    m_min=0.0f;
    m_max=1.0f;
    m_clampToNotches=false;
    m_numNotches=5;
    m_value=0.0f;
    setTabable(true);
}

void Slider::setDisabled(bool active)
{
    ParentClass::setDisabled(active);
    m_sliderBar->setDisabled(active);

    if(m_valueText)
        updateColors();
}

// The value text is the skin's label colour, the disabled button colour while
// disabled; read again here, so a theme changed at run time reaches it.
void Slider::updateColors()
{
    updateValueTextColor(m_valueText, this);
}

void Slider::updateValueTextColor(internal::Text *text, Base *owner)
{
    if(!text)
        return;
    if(owner->isDisabled())
        text->setTextColor(owner->getSkin()->Colors.Button.Disabled);
    else
        text->setSkinTextColor(internal::SkinTextColor::LabelDefault);
}

void Slider::onMoved(event::Info)
{
    setValueInternal(calculateValue());
}

void Slider::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
    placeValueText();
}

// The value text's height is measured in the Y pass, after layout()'s run at
// the start of the X arrange read it: it is placed again here, with its
// height final, before the pass arranges it in Y.
void Slider::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::Y)
        placeValueText();
    ParentClass::arrange(skin, dim);
}

void Slider::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    const int content=valueTextSize(dim, isHorizontal(), m_valueText, m_valueFormat, m_min, m_max,
        dim == Dim::X?m_requestedBounds.w:m_requestedBounds.h, m_valueColumn,
        [this](const Font &font, const String &text) { return measureText(font, text); });

    if(m_valueText)
        m_preferredSize.value[int(dim)]=preferredFromContent(content, dim);
}

gweni::Rect Slider::getTrackBounds()
{
    return valueTextTrackBounds(getRenderBounds(), isHorizontal(), m_valueText != nullptr, m_valueColumn);
}

Rect Slider::valueTextTrackBounds(Rect bounds, bool horizontal, bool hasText, int column)
{
    if(hasText && !horizontal)
        bounds.w=std::max(0, bounds.w-ValueTextGap-column);
    return bounds;
}

Rect Slider::valueTextBounds(int width, int height, int trackWidth, bool horizontal, const Size &size)
{
    return Rect(horizontal?(width-size.width)/2:trackWidth+ValueTextGap,
        (height-size.height)/2, size.width, size.height);
}

void Slider::placeValueText()
{
    if(m_valueText)
        placeChild(m_valueText, valueTextBounds(getWidth(), getHeight(), getTrackBounds().w,
            isHorizontal(), m_valueText->getPreferredSize()));
}

// One conversion of a double, with the flags, width and precision snprintf
// takes for it; "%%" anywhere. Width and precision are kept to 2 digits so a
// format cannot ask for a string of any length.
bool Slider::validValueFormat(const String &format)
{
    const size_t size=format.size();
    int conversions=0;

    for(size_t i=0; i<size; ++i)
    {
        if(format[i] != '%')
            continue;

        ++i;

        if(i<size && format[i] == '%')
            continue;

        while(i<size && format[i] != '\0' && std::strchr("-+ #0", format[i]))
            ++i;

        int digits=0;

        while(i<size && format[i] >= '0' && format[i] <= '9')
        {
            ++i;
            ++digits;
        }

        if(digits > 2)
            return false;

        if(i<size && format[i] == '.')
        {
            ++i;
            digits=0;

            while(i<size && format[i] >= '0' && format[i] <= '9')
            {
                ++i;
                ++digits;
            }

            if(digits > 2)
                return false;
        }

        if(i >= size || format[i] == '\0' || !std::strchr("fFeEgGaA", format[i]))
            return false;

        ++conversions;
    }

    return conversions == 1;
}

String Slider::formatValue(const String &format, double value)
{
    if(!validValueFormat(format))
        return String();

    // validValueFormat() has checked it takes exactly one double
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    const int length=std::snprintf(nullptr, 0, format.c_str(), double(value));

    if(length <= 0)
        return String();

    String text(size_t(length)+1, '\0');

    std::snprintf(&text[0], text.size(), format.c_str(), double(value));
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    text.resize(size_t(length));
    return text;
}

bool Slider::setValueFormat(const String &format)
{
    if(format == m_valueFormat)
        return true;

    if(!format.empty() && !validValueFormat(format))
    {
        std::cerr<<"gweni: Slider::setValueFormat: \""<<format<<"\" refused: it must hold one "
            "conversion of a double (%f, %e, %g or %a, with flags, a width and a precision)\n";
        return false;
    }

    m_valueFormat=format;

    if(format.empty())
    {
        delete m_valueText;
        m_valueText=nullptr;
        m_valueColumn=0;
        invalidate();
        return true;
    }

    if(!m_valueText)
    {
        // After the thumb, so it is numbered, and drawn, over it.
        m_valueText=newChild<internal::Text>();
        m_valueText->setPlacement(Placement::Owner);
        updateColors();
    }

    updateValueText();
    // the column (vertical) or the least height (horizontal) is the format's
    invalidate();
    return true;
}

String Slider::getValueText() const
{
    return m_valueText?m_valueText->getText():String();
}

// Text::setText marks the text, and lays it out again, only for a new string.
void Slider::updateValueText()
{
    if(!m_valueText)
        return;

    m_valueText->setText(formatValue(m_valueFormat, getFloatValue()));
}

float Slider::calculateValue()
{
    return 0;
}

void Slider::setFloatValue(float val, bool /*forceUpdate*/)
{
    val=clamp(val, m_min, m_max);

    // Normalize Value
    val=(val-m_min)/(m_max-m_min);
    setValueInternal(val);
    redraw();
}

void Slider::setValueInternal(float val)
{
    if(m_clampToNotches)
    {
        val=std::floor(val  *float(m_numNotches) + 0.5f);
        val/=float(m_numNotches);
    }

    if(m_value != val)
    {
        m_value=val;
        updateValueText();
        onValueChanged.call(this);
    }

    updateBarFromValue();
}

float Slider::getFloatValue()
{
    return m_min+(m_value*(m_max-m_min));
}

void Slider::setRange(float fMin, float fMax)
{
    m_min=fMin;
    m_max=fMax;

    if(m_valueText)
    {
        updateValueText();
        invalidate();
    }
}

void Slider::renderFocus(gweni::skin::Base *skin)
{
    if(gweni::KeyboardFocus != this)
        return;

    if(!isTabable())
        return;

//    skin->drawKeyboardHighlight(this, skin::Generate, getRenderBounds(), 0);
//    skin->drawControl(this);
    if(getStateChange() != StateChange_Nothing)
        m_skinControl->update(skin->getRenderer(), this);
}

}//namespace controls
}//namespace gweni
