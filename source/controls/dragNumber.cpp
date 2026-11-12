/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/dragNumber.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/slider.h>
#include <gweni/inputHandler.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(DragNumberTextBox)
{
}

// On the release, as a TextBox commits: the release then finds the key up
// and goes nowhere.
bool DragNumberTextBox::onKeyReturn(bool down)
{
    if(!down && m_owner)
        m_owner->commitEdit(true);
    return true;
}

bool DragNumberTextBox::onKeyEscape(bool down)
{
    if(down && m_owner)
        m_owner->cancelEdit(true);
    return true;
}

}//namespace internal


GWENI_CONTROL_CONSTRUCTOR(DragNumber)
{
    // a field of the size asked for, like a TextBox: its text is centred in it
    setSize(100, 20);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setPadding(Padding(4, 2, 4, 2));
    setDefaultTextAlignment(Alignment::Center);
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(true);
    setTabable(true);
    setCursor(cursor::SizeWE);
    updateColors();
    updateText();
}

double DragNumber::getMin() const
{
    return m_hasRange?m_min:-std::numeric_limits<double>::max();
}

double DragNumber::getMax() const
{
    return m_hasRange?m_max:std::numeric_limits<double>::max();
}

double DragNumber::constrain(double value) const
{
    if(!std::isfinite(value))
        value=value > 0?getMax():(value < 0?getMin():m_value);

    value=std::min(std::max(value, getMin()), getMax());

    if(m_integer)
    {
        value=std::round(value);
        // a range whose ends are not whole keeps the rounded value inside it
        if(value > getMax())
            value-=1.0;
        if(value < getMin())
            value+=1.0;
    }

    // never "-0"
    if(value == 0.0)
        value=0.0;
    return value;
}

void DragNumber::setNumber(double value, bool doEvents)
{
    value=constrain(value);

    if(value == m_value)
        return;

    m_value=value;
    updateText();

    if(doEvents)
        onValueChanged.call(this);
}

void DragNumber::setRange(double min, double max)
{
    if(min > max)
        std::swap(min, max);

    m_min=min;
    m_max=max;
    m_hasRange=true;
    setNumber(m_value);
}

void DragNumber::clearRange()
{
    m_hasRange=false;
}

void DragNumber::setSpeed(double unitsPerUnit)
{
    m_speed=unitsPerUnit > 0.0?unitsPerUnit:0.0;
}

double DragNumber::getSpeed() const
{
    if(m_speed > 0.0)
        return m_speed;
    return m_integer?1.0:0.1;
}

bool DragNumber::setFormat(const String &format)
{
    if(!format.empty() && !Slider::validValueFormat(format))
    {
        std::cerr<<"gweni: DragNumber::setFormat: \""<<format<<"\" refused: it must hold one "
            "conversion of a double (%f, %e, %g or %a, with flags, a width and a precision)\n";
        return false;
    }

    m_format=format;
    updateText();
    return true;
}

const String &DragNumber::getFormat() const
{
    static const String integerFormat="%.0f";
    static const String floatFormat="%.3f";

    if(!m_format.empty())
        return m_format;
    return m_integer?integerFormat:floatFormat;
}

void DragNumber::setInteger(bool integer)
{
    if(integer == m_integer)
        return;

    m_integer=integer;
    // the default format changes with it, and the value may round
    updateText();
    setNumber(m_value);
}

String DragNumber::formatNumber(double value) const
{
    return Slider::formatValue(getFormat(), value);
}

String DragNumber::editText() const
{
    if(m_integer)
        return Slider::formatValue("%.0f", m_value);

    // the precision of the format's conversion when it is %f, else 6
    int precision=6;
    const String &format=getFormat();

    for(size_t i=0; i < format.size(); ++i)
    {
        if(format[i] != '%')
            continue;
        if(i+1 < format.size() && format[i+1] == '%')
        {
            ++i;
            continue;
        }

        size_t end=i+1;
        while(end < format.size() && !std::strchr("fFeEgGaA", format[end]))
            ++end;
        if(end < format.size() && (format[end] == 'f' || format[end] == 'F'))
        {
            const size_t dot=format.find('.', i);

            precision=(dot != String::npos && dot < end)?std::atoi(format.c_str()+dot+1):6;
        }
        break;
    }

    String text=Slider::formatValue("%."+std::to_string(precision)+"f", m_value);

    if(text.find('.') != String::npos)
    {
        while(!text.empty() && text.back() == '0')
            text.pop_back();
        if(!text.empty() && text.back() == '.')
            text.pop_back();
    }
    if(text == "-0")
        text="0";
    return text;
}

bool DragNumber::parseNumber(const String &text, double &value)
{
    const char *start=text.c_str();

    while(*start == ' ')
        ++start;
    if(*start == '\0')
        return false;

    char *end=nullptr;

    errno=0;
    const double parsed=std::strtod(start, &end);

    if(end == start || errno == ERANGE || !std::isfinite(parsed))
        return false;
    while(*end == ' ')
        ++end;
    if(*end != '\0')
        return false;

    value=parsed;
    return true;
}

// Label::setText marks the Text, and lays it out again, only for a new string.
void DragNumber::updateText()
{
    setText(formatNumber(m_value), false);
}

String DragNumber::getValue()
{
    return editText();
}

void DragNumber::setValue(const String &value)
{
    double number;

    if(parseNumber(value, number))
        setNumber(number);
}

void DragNumber::setDisabled(bool active)
{
    if(active == isDisabled())
        return;

    if(active)
    {
        cancelEdit(false);
        if(m_depressed)
        {
            m_depressed=false;
            if(gweni::MouseFocus == this)
                gweni::MouseFocus=nullptr;
        }
    }
    ParentClass::setDisabled(active);
}

void DragNumber::updateColors()
{
    setTextColor(isDisabled()?getSkin()->Colors.Button.Disabled:getSkin()->Colors.Label.Default);
}

double DragNumber::modifierFactor()
{
    if(input::isShiftDown())
        return ShiftFactor;
    if(input::isKeyDown(keys::Alt))
        return AltFactor;
    return 1.0;
}

void DragNumber::onMouseClickLeft(int x, int /*y*/, bool down)
{
    if(isDisabled())
        return;

    if(down)
    {
        if(m_editing)
            return;

        if(input::isControlDown())
        {
            beginEdit();
            return;
        }

        m_depressed=true;
        m_dragBase=m_value;
        m_dragUnits=0.0;
        m_lastMouseX=x;
        gweni::MouseFocus=this;
        redraw();
        return;
    }

    // the release of a double-click's second press, or one the press never saw
    if(!m_depressed)
        return;

    m_depressed=false;
    if(gweni::MouseFocus == this)
        gweni::MouseFocus=nullptr;
    redraw();
}

void DragNumber::onMouseDoubleClickLeft(int /*x*/, int /*y*/)
{
    if(isDisabled())
        return;

    beginEdit();
}

// The motion is summed in units of the mouse times the modifiers' factors, so
// a drag back to where it started gives the value at the press, and the sum
// is held at the range's ends, so turning back moves the value at once.
void DragNumber::onMouseMoved(int x, int /*y*/, int /*deltaX*/, int /*deltaY*/)
{
    if(!m_depressed || isDisabled())
        return;

    const int dx=x-m_lastMouseX;

    m_lastMouseX=x;
    if(dx == 0)
        return;

    const double speed=getSpeed();

    m_dragUnits+=dx*modifierFactor();

    double raw=m_dragBase+m_dragUnits*speed;

    if(raw > getMax())
    {
        raw=getMax();
        m_dragUnits=(raw-m_dragBase)/speed;
    }
    else if(raw < getMin())
    {
        raw=getMin();
        m_dragUnits=(raw-m_dragBase)/speed;
    }

    setNumber(raw);
}

void DragNumber::step(double direction)
{
    double delta=getSpeed()*modifierFactor();

    if(m_integer)
        delta=std::max(1.0, std::round(delta));
    setNumber(m_value+direction*delta);
}

bool DragNumber::onKeyUp(bool down)
{
    if(down && !m_editing && !isDisabled())
        step(1.0);
    return true;
}

bool DragNumber::onKeyRight(bool down)
{
    return onKeyUp(down);
}

bool DragNumber::onKeyDown(bool down)
{
    if(down && !m_editing && !isDisabled())
        step(-1.0);
    return true;
}

bool DragNumber::onKeyLeft(bool down)
{
    return onKeyDown(down);
}

bool DragNumber::onKeyReturn(bool down)
{
    if(!down && !m_editing && !isDisabled())
        beginEdit();
    return true;
}

void DragNumber::beginEdit()
{
    if(m_editing || isDisabled())
        return;

    if(m_depressed)
    {
        m_depressed=false;
        if(gweni::MouseFocus == this)
            gweni::MouseFocus=nullptr;
    }

    if(!m_editBox)
    {
        // After the text, so it is numbered, and drawn, over the field; made
        // once and shown or hidden after that.
        m_editBox=newChild<internal::DragNumberTextBox>();
        m_editBox->setOwner(this);
        m_editBox->setPlacement(Placement::Owner);
        m_editBox->onLostFocusCaller.add(this, &DragNumber::onEditLostFocus);
        invalidate();
    }

    m_editing=true;
    m_editBox->setText(editText(), false);
    // a setText of the text it has keeps the history; each edit starts with none
    m_editBox->getUndoStack().clear();
    m_text->setHidden(true);
    m_editBox->setHidden(false);
    m_editBox->focus();
    m_editBox->onSelectAll(m_editBox);
    // the box now lies under the mouse
    if(Canvas *canvas=getCanvas())
        canvas->hoverMayHaveMoved();
    redraw();
}

void DragNumber::commitEdit(bool refocus)
{
    endEdit(true, refocus?EditFocus::Field:EditFocus::Drop);
}

void DragNumber::cancelEdit(bool refocus)
{
    endEdit(false, refocus?EditFocus::Field:EditFocus::Drop);
}

// m_editing is cleared first: the focus leaving the box below calls
// onEditLostFocus, which must find the edit over.
void DragNumber::endEdit(bool commit, EditFocus after)
{
    if(!m_editing)
        return;

    m_editing=false;

    double typed;
    const bool apply=commit && parseNumber(m_editBox->getText(), typed);

    m_editBox->setHidden(true);
    m_text->setHidden(false);

    if(gweni::KeyboardFocus == m_editBox)
    {
        if(after == EditFocus::Field)
            focus();
        else if(after == EditFocus::Drop)
            m_editBox->unfocus();
    }
    if(Canvas *canvas=getCanvas())
        canvas->hoverMayHaveMoved();
    redraw();

    if(apply)
        setNumber(typed);
}

// The focus is on its way to another control: it is left to go there.
void DragNumber::onEditLostFocus(event::Info)
{
    endEdit(true, EditFocus::Leave);
}

// The box is owner-placed and the hidden Text counts for nothing in
// sizeOfChildren, so with the box open the field's content would be its
// padding alone, and any flag but Fixed would shrink it to that. The Text is
// counted as sizeOfChildren counts it shown (a Label's dock-None child: its
// size and margin), its string measured afresh (the app may set a value while
// the box is open): opening or closing the edit changes no size, and the
// layout stops at the field.
void DragNumber::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(!m_editing || !m_text->hidden() || !m_text->shouldIncludeInSize())
        return;

    m_text->doCalculateSize(skin, dim);

    const Size &text=m_text->getPreferredSize();
    const Margin &margin=m_text->getMargin();

    if(dim == Dim::X)
        m_preferredSize.width=std::max(m_preferredSize.width, preferredFromContent(
            text.width+margin.left+margin.right+m_padding.left+m_padding.right, Dim::X));
    else
        m_preferredSize.height=std::max(m_preferredSize.height, preferredFromContent(
            text.height+margin.top+margin.bottom+m_padding.top+m_padding.bottom, Dim::Y));
}

void DragNumber::layout(skin::Base *skin)
{
    ParentClass::layout(skin);

    if(m_editBox)
        placeChild(m_editBox, 0, 0, getWidth(), getHeight());
}

}//namespace controls
}//namespace gweni
