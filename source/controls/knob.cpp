/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/knob.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/slider.h>
#include <gweni/inputHandler.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace gweni
{
namespace controls
{

namespace
{

constexpr double Pi=3.14159265358979323846;

//! \p degrees in [0, 360).
double wrapTurn(double degrees)
{
    double wrapped=std::fmod(degrees, 360.0);

    if(wrapped < 0.0)
        wrapped+=360.0;
    return wrapped;
}

//! \p degrees in (-180, 180].
double wrapHalfTurn(double degrees)
{
    double wrapped=wrapTurn(degrees);

    if(wrapped > 180.0)
        wrapped-=360.0;
    return wrapped;
}

}//namespace


GWENI_CONTROL_CONSTRUCTOR(Knob)
{
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(true);
    setTabable(true);
    setCursor(cursor::SizeNS);
    setShowValue(true);
}

double Knob::constrain(double value) const
{
    if(!std::isfinite(value))
        value=value > 0?m_max:(value < 0?m_min:m_value);

    value=std::min(std::max(value, m_min), m_max);

    if(m_integer)
    {
        value=std::round(value);
        // a range whose ends are not whole keeps the rounded value inside it
        if(value > m_max)
            value-=1.0;
        if(value < m_min)
            value+=1.0;
    }

    // never "-0"
    if(value == 0.0)
        value=0.0;
    return value;
}

void Knob::setNumber(double value, bool doEvents)
{
    value=constrain(value);

    if(value == m_value)
        return;

    m_value=value;
    updateText();
    // the indicator and the value arc
    addStateChange(StateChange_Background);

    if(doEvents)
        onValueChanged.call(this);
}

void Knob::setRange(double min, double max)
{
    if(!std::isfinite(min) || !std::isfinite(max))
        return;
    if(min > max)
        std::swap(min, max);
    if(min == m_min && max == m_max)
        return;

    m_min=min;
    m_max=max;
    // the arc shows the value on another scale, and the text's column moves
    addStateChange(StateChange_Background);
    invalidate();
    setNumber(m_value);
}

void Knob::setSpeed(double unitsPerStep)
{
    m_speed=unitsPerStep > 0.0?unitsPerStep:0.0;
}

double Knob::getSpeed() const
{
    if(m_speed > 0.0)
        return m_speed;
    return (m_max-m_min)/DefaultSteps;
}

bool Knob::setFormat(const String &format)
{
    if(!format.empty() && !Slider::validValueFormat(format))
    {
        std::cerr<<"gweni: Knob::setFormat: \""<<format<<"\" refused: it must hold one "
            "conversion of a double (%f, %e, %g or %a, with flags, a width and a precision)\n";
        return false;
    }

    if(format == m_format)
        return true;

    m_format=format;
    updateText();
    // the column is the format's
    invalidate();
    return true;
}

const String &Knob::getFormat() const
{
    static const String integerFormat="%.0f";
    static const String floatFormat="%.2f";

    if(!m_format.empty())
        return m_format;
    return m_integer?integerFormat:floatFormat;
}

String Knob::formatNumber(double value) const
{
    return Slider::formatValue(getFormat(), value);
}

void Knob::setInteger(bool integer)
{
    if(integer == m_integer)
        return;

    m_integer=integer;
    // the default format changes with it, and the value may round
    updateText();
    invalidate();
    setNumber(m_value);
}

void Knob::setAngles(double startDegrees, double endDegrees)
{
    if(!std::isfinite(startDegrees) || !std::isfinite(endDegrees))
        return;

    // at most a whole turn either way
    endDegrees=startDegrees+std::min(std::max(endDegrees-startDegrees, -360.0), 360.0);
    if(startDegrees == m_startAngle && endDegrees == m_endAngle)
        return;

    m_startAngle=startDegrees;
    m_endAngle=endDegrees;
    addStateChange(StateChange_Background);
}

double Knob::angleForValue(double value) const
{
    return m_startAngle+sweepShare(value)*(m_endAngle-m_startAngle);
}

// The angle's distance from the start in the sweep's direction, a turn at
// most: inside the sweep it is the value's share of the range, in the gap the
// nearer end.
double Knob::valueForAngle(double degrees) const
{
    const double sweep=m_endAngle-m_startAngle;
    const double length=std::fabs(sweep);

    if(length == 0.0)
        return m_min;

    const double along=wrapTurn((degrees-m_startAngle)*(sweep < 0.0?-1.0:1.0));
    double t;

    if(along <= length)
        t=along/length;
    else
        t=(along-length < 360.0-along)?1.0:0.0;
    return m_min+t*(m_max-m_min);
}

void Knob::setBipolar(bool bipolar)
{
    if(bipolar == m_bipolar)
        return;

    m_bipolar=bipolar;
    addStateChange(StateChange_Background);
}

double Knob::getArcOrigin() const
{
    if(!m_bipolar)
        return m_min;
    return std::min(std::max(0.0, m_min), m_max);
}

void Knob::setDragMode(DragMode mode)
{
    m_dragMode=mode;
    setCursor(mode == DragMode::Vertical?cursor::SizeNS:cursor::Finger);
}

void Knob::setTickCount(int count)
{
    count=std::max(0, count);
    if(count == 1)
        count=2;
    if(count == m_tickCount)
        return;

    m_tickCount=count;
    addStateChange(StateChange_Background);
}

void Knob::setDiameter(int diameter)
{
    diameter=std::max(1, diameter);
    if(diameter == m_diameter)
        return;

    m_diameter=diameter;
    addStateChange(StateChange_Background);
    invalidate();
}

void Knob::setShowValue(bool show)
{
    if(show == (m_valueText != nullptr))
        return;

    if(!show)
    {
        delete m_valueText;
        m_valueText=nullptr;
        m_valueColumn=0;
        // the dial takes the room it leaves, its bounds the same
        addStateChange(StateChange_Background);
        invalidate();
        return;
    }

    m_valueText=newChild<internal::Text>();
    m_valueText->setPlacement(Placement::Owner);
    // a value of the same width redraws its glyphs and lays nothing out
    m_valueText->setSameSizeKeepsLayout(true);
    updateColors();
    updateText();
    // the dial gives it room, its bounds the same
    addStateChange(StateChange_Background);
    invalidate();
}

String Knob::getValueText() const
{
    return m_valueText?m_valueText->getText():String();
}

void Knob::setCaption(const String &caption)
{
    if(caption.empty())
    {
        if(m_caption)
        {
            delete m_caption;
            m_caption=nullptr;
            // the dial moves up, its bounds the same
            addStateChange(StateChange_Background);
            invalidate();
        }
        return;
    }

    if(!m_caption)
    {
        m_caption=newChild<internal::Text>();
        m_caption->setPlacement(Placement::Owner);
        updateColors();
        // the dial moves down under it, its bounds the same
        addStateChange(StateChange_Background);
        invalidate();
    }
    m_caption->setText(caption);
}

String Knob::getCaption() const
{
    return m_caption?m_caption->getText():String();
}

// Text::setText marks the text only for a new string.
void Knob::updateText()
{
    if(m_valueText)
        m_valueText->setText(formatNumber(m_value));
}

String Knob::getValue()
{
    return DragNumber::editTextFor(getFormat(), m_integer, m_value);
}

void Knob::setValue(const String &value)
{
    double number;

    if(DragNumber::parseNumber(value, number))
        setNumber(number);
}

void Knob::setDisabled(bool active)
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
    updateColors();
}

// The skin's Knob colours, the disabled button colour while disabled; read
// again here, so a theme changed at run time reaches them.
void Knob::updateColors()
{
    skin::Base *skin=getSkin();

    if(!skin)
        return;

    const Color &disabled=skin->Colors.Button.Disabled;

    if(m_valueText)
        m_valueText->setTextColor(isDisabled()?disabled:skin->Colors.Knob.ValueText);
    if(m_caption)
        m_caption->setTextColor(isDisabled()?disabled:skin->Colors.Knob.CaptionText);
}

Rect Knob::getDialRect() const
{
    const int top=m_caption?m_caption->getPreferredSize().height+TextGap:0;
    const int below=m_valueText?m_valueText->getPreferredSize().height+TextGap:0;
    const int side=std::max(0, std::min({m_diameter, getWidth(), getHeight()-top-below}));

    return Rect((getWidth()-side)/2, top, side, side);
}

// A tick ring a tenth of the side wide (3 units at least) with a unit's gap
// inside it, the arcs a twelfth of the side thick (2 at least), the body a
// sixteenth of the side (2 at least) inside them, the indicator from a
// quarter of the body's radius to 0.85 of it, half the arcs' thickness.
Knob::Geometry Knob::getGeometry() const
{
    const Rect dial=getDialRect();
    const float side=float(dial.w);
    const float half=side*0.5f;
    Geometry geometry;

    geometry.centre=PointF(dial.x+half, dial.y+half);
    geometry.arcThickness=std::max(2.0f, side/12.0f);

    // the arcs' outer edge, a whole number of units in from the dial's
    int inset=0;

    if(m_tickCount > 0)
    {
        const float length=std::round(std::max(3.0f, side/10.0f));

        geometry.tickThickness=1.0f;
        // a tick's round cap reaches half its thickness past its end
        geometry.tickOuter=half-geometry.tickThickness*0.5f;
        geometry.tickInner=half-length;
        inset=int(length)+1;
    }
    geometry.arcRect=Rect(dial.x+inset, dial.y+inset, std::max(0, dial.w-2*inset),
        std::max(0, dial.h-2*inset));

    const float outer=std::max(0.0f, half-float(inset));

    geometry.arcRadius=std::max(0.0f, outer-geometry.arcThickness*0.5f);
    geometry.bodyRadius=std::max(0.0f,
        geometry.arcRadius-geometry.arcThickness*0.5f-std::max(2.0f, side/16.0f));
    geometry.indicatorInner=geometry.bodyRadius*0.25f;
    geometry.indicatorOuter=geometry.bodyRadius*0.85f;
    geometry.indicatorThickness=std::max(1.5f, geometry.arcThickness*0.5f);
    return geometry;
}

PointF Knob::pointAt(const PointF &centre, double degrees, float radius)
{
    const double rad=degrees*Pi/180.0;

    return PointF(float(centre.x+radius*std::cos(rad)), float(centre.y-radius*std::sin(rad)));
}

double Knob::sweepShare(double value) const
{
    const double range=m_max-m_min;
    const double t=range > 0.0?(value-m_min)/range:0.0;

    return std::min(std::max(t, 0.0), 1.0);
}

bool Knob::mouseAngle(int x, int y, double &degrees)
{
    const PointF local=canvasPosToLocalF(PointF(static_cast<float>(x), static_cast<float>(y)));
    const Rect dial=getDialRect();
    const double dx=local.x-(dial.x+dial.w*0.5);
    // y up
    const double dy=(dial.y+dial.h*0.5)-local.y;

    if(std::hypot(dx, dy) < MinCentreDistance)
        return false;

    degrees=std::atan2(dy, dx)*180.0/Pi;
    return true;
}

void Knob::onMouseClickLeft(int x, int y, bool down)
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
        m_lastMouseY=y;
        m_haveAngle=mouseAngle(x, y, m_lastAngle);
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

void Knob::onMouseDoubleClickLeft(int /*x*/, int /*y*/)
{
    if(isDisabled())
        return;

    beginEdit();
}

// The change is summed from the press and held at the range's ends, so a drag
// back to where it started gives the value at the press and turning back at
// an end moves the value at once. An angular drag sums the angle turned from
// one move to the next, each under half a turn, so it follows the mouse round
// the arc and, held at the end, never jumps across the gap.
void Knob::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    if(!m_depressed || isDisabled())
        return;

    const double factor=input::isShiftDown()?FineFactor:1.0;
    // the value per unit of motion: a unit of vertical motion, or a degree
    double perUnit;
    double units;

    if(m_dragMode == DragMode::Vertical)
    {
        const int dy=y-m_lastMouseY;

        m_lastMouseY=y;
        if(dy == 0)
            return;
        units=-dy;
        perUnit=getSpeed()*DefaultSteps/DragRange;
    }
    else
    {
        double angle;

        // at the centre the angle means nothing: forget it, so the move out
        // of it, on whichever side, takes the angle afresh rather than
        // summing half a turn
        if(!mouseAngle(x, y, angle))
        {
            m_haveAngle=false;
            return;
        }
        if(!m_haveAngle)
        {
            m_lastAngle=angle;
            m_haveAngle=true;
            return;
        }

        const double sweep=m_endAngle-m_startAngle;

        units=wrapHalfTurn(angle-m_lastAngle);
        m_lastAngle=angle;
        if(units == 0.0 || sweep == 0.0)
            return;
        perUnit=(m_max-m_min)/sweep;
    }
    if(perUnit == 0.0)
        return;

    m_dragUnits+=units*factor;

    double raw=m_dragBase+m_dragUnits*perUnit;

    if(raw > m_max)
    {
        raw=m_max;
        m_dragUnits=(raw-m_dragBase)/perUnit;
    }
    else if(raw < m_min)
    {
        raw=m_min;
        m_dragUnits=(raw-m_dragBase)/perUnit;
    }

    setNumber(raw);
}

void Knob::step(double steps)
{
    // one step: a whole number at least for an integer knob
    double delta=getSpeed()*(input::isShiftDown()?FineFactor:1.0);

    if(m_integer)
        delta=std::max(1.0, std::round(delta));
    delta*=std::fabs(steps);
    setNumber(m_value+(steps < 0.0?-delta:delta));
}

bool Knob::onKeyUp(bool down)
{
    if(down && !m_editing && !isDisabled())
        step(1.0);
    return true;
}

bool Knob::onKeyRight(bool down)
{
    return onKeyUp(down);
}

bool Knob::onKeyDown(bool down)
{
    if(down && !m_editing && !isDisabled())
        step(-1.0);
    return true;
}

bool Knob::onKeyLeft(bool down)
{
    return onKeyDown(down);
}

bool Knob::onKeyPageUp(bool down)
{
    if(down && !m_editing && !isDisabled())
        step(PageFactor);
    return true;
}

bool Knob::onKeyPageDown(bool down)
{
    if(down && !m_editing && !isDisabled())
        step(-PageFactor);
    return true;
}

bool Knob::onKeyHome(bool down)
{
    if(down && !m_editing && !isDisabled())
        setNumber(m_min);
    return true;
}

bool Knob::onKeyend(bool down)
{
    if(down && !m_editing && !isDisabled())
        setNumber(m_max);
    return true;
}

bool Knob::onKeyReturn(bool down)
{
    if(!down && !m_editing && !isDisabled())
        beginEdit();
    return true;
}

void Knob::beginEdit()
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
        // After the texts, so it is numbered, and drawn, over the knob; made
        // once and shown or hidden after that.
        m_editBox=newChild<internal::DragNumberTextBox>();
        m_editBox->setOwner(this);
        m_editBox->setPlacement(Placement::Owner);
        m_editBox->onLostFocusCaller.add(this, &Knob::onEditLostFocus);
        invalidate();
    }

    m_editing=true;
    m_editBox->setText(getValue(), false);
    // a setText of the text it has keeps the history; each edit starts with none
    m_editBox->getUndoStack().clear();
    m_editBox->setHidden(false);
    m_editBox->focus();
    m_editBox->onSelectAll(m_editBox);
    // the box now lies under the mouse
    if(Canvas *canvas=getCanvas())
        canvas->hoverMayHaveMoved();
    redraw();
}

void Knob::commitEdit(bool refocus)
{
    endEdit(true, refocus?EditFocus::Field:EditFocus::Drop);
}

void Knob::cancelEdit(bool refocus)
{
    endEdit(false, refocus?EditFocus::Field:EditFocus::Drop);
}

// m_editing is cleared first: the focus leaving the box below calls
// onEditLostFocus, which must find the edit over.
void Knob::endEdit(bool commit, EditFocus after)
{
    if(!m_editing)
        return;

    m_editing=false;

    double typed;
    const bool apply=commit && DragNumber::parseNumber(m_editBox->getText(), typed);

    m_editBox->setHidden(true);

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
void Knob::onEditLostFocus(event::Info)
{
    endEdit(true, EditFocus::Leave);
}

// Every child is owner-placed, measured by Base for its own subtree; the
// knob's content is the dial with the caption above it and the value text
// under it, as wide as the widest of the three, the value text measured at
// the range's ends (and as it is), so a drag never widens the knob.
void Knob::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::X)
    {
        int width=m_diameter;

        if(m_caption)
            width=std::max(width, m_caption->getPreferredSize().width);
        m_valueColumn=0;
        if(m_valueText)
        {
            const Font &font=m_valueText->getFont();

            for(double value : {m_min, m_max})
                m_valueColumn=std::max(m_valueColumn, measureText(font, formatNumber(value)).x);
            m_valueColumn=std::max(m_valueColumn, m_valueText->getPreferredSize().width);
            width=std::max(width, m_valueColumn);
        }
        m_preferredSize.width=preferredFromContent(width+m_padding.left+m_padding.right, Dim::X);
        return;
    }

    int height=m_diameter;

    if(m_caption)
        height+=m_caption->getPreferredSize().height+TextGap;
    if(m_valueText)
        height+=m_valueText->getPreferredSize().height+TextGap;
    m_preferredSize.height=preferredFromContent(height+m_padding.top+m_padding.bottom, Dim::Y);
}

void Knob::placeParts()
{
    const Rect dial=getDialRect();

    if(m_caption)
    {
        const Size &size=m_caption->getPreferredSize();

        placeChild(m_caption, (getWidth()-size.width)/2, 0, size.width, size.height);
    }
    if(m_valueText)
    {
        const Size &size=m_valueText->getPreferredSize();

        placeChild(m_valueText, (getWidth()-size.width)/2, dial.y+dial.h+TextGap, size.width,
            size.height);
    }
    if(m_editBox)
    {
        const int height=std::min(EditHeight, getHeight());

        placeChild(m_editBox, 0, dial.y+(dial.h-height)/2, getWidth(), height);
    }
}

void Knob::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
    placeParts();
}

// The texts' heights are measured in the Y pass, after layout() at the start
// of the X arrange read them: placed again here, final, before the pass
// arranges them in Y.
void Knob::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::Y)
        placeParts();
    ParentClass::arrange(skin, dim);
}

}//namespace controls
}//namespace gweni
