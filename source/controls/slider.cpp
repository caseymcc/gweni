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
        m_valueText->setTextColor(active?getSkin()->Colors.Button.Disabled:getSkin()->Colors.Label.Default);
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

void Slider::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(!m_valueText)
    {
        m_valueColumn=0;
        return;
    }

    if(isHorizontal())
    {
        // the text's line inside the track's 1-unit border
        if(dim == Dim::Y)
            m_preferredSize.height=preferredFromContent(std::max(m_requestedBounds.h,
                m_valueText->getPreferredSize().height+2), Dim::Y);
        return;
    }

    if(dim != Dim::X)
        return;

    // Measured from the range, not the value shown, so a drag never changes it.
    const gweni::Font &font=m_valueText->getFont();

    m_valueColumn=std::max(measureText(font, formatValue(m_valueFormat, m_min)).x,
        measureText(font, formatValue(m_valueFormat, m_max)).x);
    m_preferredSize.width=preferredFromContent(m_requestedBounds.w+ValueTextGap+m_valueColumn, Dim::X);
}

gweni::Rect Slider::getTrackBounds()
{
    gweni::Rect r=getRenderBounds();

    if(m_valueText && !isHorizontal())
        r.w=std::max(0, r.w-ValueTextGap-m_valueColumn);

    return r;
}

void Slider::placeValueText()
{
    if(!m_valueText)
        return;

    const Size &size=m_valueText->getPreferredSize();
    const int y=(getHeight()-size.height)/2;

    if(isHorizontal())
        placeChild(m_valueText, (getWidth()-size.width)/2, y, size.width, size.height);
    else
        placeChild(m_valueText, getTrackBounds().w+ValueTextGap, y, size.width, size.height);
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

String Slider::formatValue(const String &format, float value)
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
        m_valueText->setTextColor(isDisabled()?getSkin()->Colors.Button.Disabled:getSkin()->Colors.Label.Default);
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
