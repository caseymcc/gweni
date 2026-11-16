/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_knob_h_
#define _gweni_controls_knob_h_

#include <gweni/controls/base.h>
#include <gweni/controls/dragNumber.h>
#include <gweni/controls/text.h>

namespace gweni
{
namespace controls
{

//! A rotary value control (imgui-knobs, an audio plug-in's knob): a value in
//! a range shown on an arc round a dial, with an optional caption above the
//! dial and the value as text under it.
//!
//! The arc runs from getStartAngle() to getEndAngle() (setAngles(), degrees
//! counter-clockwise from the dial's right, y up), 225 to -45 by default: a
//! 270 degree sweep clockwise with the gap at the bottom. angleForValue() and
//! valueForAngle() map between the two; an angle in the gap is the nearer end.
//!
//! A drag changes the value: Vertical (the default) by the mouse's vertical
//! motion, up raising it, the full range over DragRange units at the default
//! speed; Angular by the angle the mouse turns about the dial's centre, a
//! whole sweep the full range, so the value follows the mouse round the arc.
//! Either way the motion is summed from the press and held at the range's
//! ends, so a slow drag still moves the value, the drag goes back to where it
//! started when the mouse does, an angular drag never jumps across the gap,
//! and turning back at an end moves the value at once. Shift makes a drag or
//! a key step FineFactor as large. A press without motion changes nothing.
//!
//! A double-click, a Ctrl+click or Return while it has the focus shows
//! DragNumber's edit box (internal::DragNumberTextBox) over the dial with the
//! value selected: Return or a lost focus commits it (parsed, clamped,
//! rounded), Escape cancels it, and text that is no number leaves the value
//! as it was. The box is made by the first edit and only shown or hidden
//! after that. Focused, Up/Right step the value up by getSpeed() and
//! Down/Left down, PageUp/PageDown PageFactor times as far, Home and End go
//! to the minimum and the maximum; an integer knob steps by a whole number at
//! least.
//!
//! onValueChanged fires once per real change of the value, never for one
//! that stays (the same number set again, a step clamped at an end). The
//! value text is set only when the formatted string changes.
class GWENI_EXPORT Knob: public Base, public internal::TypedEntryOwner
{
public:

    GWENI_CONTROL(Knob, Base);

public:

    enum class DragMode
    {
        //! Up raises the value, down lowers it.
        Vertical,
        //! The value follows the mouse's angle about the dial's centre.
        Angular
    };

    //! The vertical motion, in units, that drags the full range at the
    //! default speed (a hundredth of the range per key step).
    static constexpr int DragRange=200;
    //! Key steps the default speed takes to cross the range.
    static constexpr double DefaultSteps=100.0;
    //! A drag's or a key step's multiplier while Shift is held.
    static constexpr double FineFactor=0.1;
    //! PageUp's and PageDown's step, in key steps.
    static constexpr double PageFactor=10.0;
    //! The dial's side when none is set.
    static constexpr int DefaultDiameter=48;
    //! The gap between the dial and its caption or value text.
    static constexpr int TextGap=2;
    //! The edit box's height, centred on the dial.
    static constexpr int EditHeight=20;
    //! An angular drag ignores the mouse closer than this to the centre,
    //! where its angle means nothing.
    static constexpr double MinCentreDistance=4.0;

    //! \p value clamped to the range and, for an integer knob, rounded to the
    //! nearest whole number. onValueChanged fires with \p doEvents if that is
    //! not the value it had.
    void setNumber(double value, bool doEvents=true);
    double getNumber() const { return m_value; }

    //! The range, [0, 1] by default (the two swapped when min > max); the
    //! value is clamped to it, which may fire onValueChanged.
    void setRange(double min, double max);
    double getMin() const { return m_min; }
    double getMax() const { return m_max; }

    //! Value units per key step; a vertical drag moves DefaultSteps of them
    //! over DragRange units. Zero or less gives the default back: a
    //! hundredth of the range.
    void setSpeed(double unitsPerStep);
    double getSpeed() const;

    //! A printf format holding one conversion of a double (Slider::
    //! validValueFormat()); anything else is refused with one line on stderr
    //! and the format kept. "" gives the default back: "%.0f" for an integer
    //! knob, "%.2f" otherwise.
    bool setFormat(const String &format);
    const String &getFormat() const;
    //! \p value as the value text shows it (getFormat()).
    String formatNumber(double value) const;

    //! An integer knob rounds its value to whole numbers (now and from then
    //! on), steps by one at least and has its own default format.
    void setInteger(bool integer);
    bool isInteger() const { return m_integer; }

    //! The arc's ends, degrees counter-clockwise from the dial's right (y
    //! up); the sweep is end - start, negative for a clockwise one, at most a
    //! whole turn either way (a larger one is cut to it).
    void setAngles(double startDegrees, double endDegrees);
    double getStartAngle() const { return m_startAngle; }
    double getEndAngle() const { return m_endAngle; }
    //! The angle the arc shows \p value at: the start for the minimum, the
    //! end for the maximum, linear between, clamped to the range.
    double angleForValue(double value) const;
    //! The value at \p degrees (any angle, taken modulo a turn) on the arc;
    //! an angle in the gap gives the nearer end's value. Not rounded.
    double valueForAngle(double degrees) const;

    //! A bipolar knob's value arc runs from the range's 0 (clamped into the
    //! range) to the value, not from the start; the look alone.
    void setBipolar(bool bipolar);
    bool isBipolar() const { return m_bipolar; }
    //! Where the value arc starts: the minimum, or the clamped 0 if bipolar.
    double getArcOrigin() const;

    void setDragMode(DragMode mode);
    DragMode getDragMode() const { return m_dragMode; }

    //! Ticks the renderer draws outside the arc, at both ends and evenly
    //! between; 0 (the default) draws none, 1 is taken as 2.
    void setTickCount(int count);
    int getTickCount() const { return m_tickCount; }

    //! The dial's side in units, DefaultDiameter by default.
    void setDiameter(int diameter);
    int getDiameter() const { return m_diameter; }

    //! The value text under the dial, shown by default.
    void setShowValue(bool show);
    bool showsValue() const { return m_valueText != nullptr; }
    //! The value text's string ("" with none).
    String getValueText() const;
    //! Null while the value text is off.
    internal::Text *getValueTextControl() const { return m_valueText; }

    //! A caption above the dial; "" (the default) has none.
    void setCaption(const String &caption);
    String getCaption() const;
    internal::Text *getCaptionControl() const { return m_caption; }

    //! The dial, in this control's coordinates: getDiameter() square (less
    //! if the control is smaller), centred across it, under the caption.
    Rect getDialRect() const;

    //! Shows the edit box over the dial, focused, its text selected.
    void beginEdit();
    //! Applies the edit box's text (if it is a number) and hides the box;
    //! \p refocus gives the keyboard focus back to the knob.
    void commitEdit(bool refocus=true) override;
    //! Hides the edit box, the value as it was.
    void cancelEdit(bool refocus=true) override;
    bool isEditing() const { return m_editing; }
    //! Null until the first beginEdit().
    internal::DragNumberTextBox *getEditBox() const { return m_editBox; }

    //! The left button is down on the knob (a drag in progress).
    bool isDepressed() const { return m_depressed; }

    //! The value as the edit box starts with it (DragNumber::editTextFor),
    //! and a parse of one.
    String getValue() override;
    void setValue(const String &value) override;

    void setDisabled(bool active) override;

    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseDoubleClickLeft(int x, int y) override;
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;

    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyLeft(bool down) override;
    bool onKeyPageUp(bool down) override;
    bool onKeyPageDown(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    bool onKeyReturn(bool down) override;

    void updateColors() override;
    //! The dial, the caption above it and the value text under it: as wide
    //! as the widest of the dial, the caption and the value text at the
    //! range's ends.
    void calculateSize(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;
    void arrange(skin::Base *skin, Dim dim) override;

    event::Caller onValueChanged;

protected:

    double constrain(double value) const;
    void updateText();
    //! A key step of \p steps key steps (signed), Shift fine.
    void step(double steps);
    //! The mouse's angle about the dial's centre at canvas point \p x, \p y,
    //! false when it is too close to the centre to have one.
    bool mouseAngle(int x, int y, double &degrees);
    void placeParts();
    enum class EditFocus { Field, Drop, Leave };
    void endEdit(bool commit, EditFocus after);
    void onEditLostFocus(event::Info info);

    double m_value=0.0;
    double m_min=0.0;
    double m_max=1.0;
    double m_speed=0.0;
    String m_format;
    bool m_integer=false;
    double m_startAngle=225.0;
    double m_endAngle=-45.0;
    bool m_bipolar=false;
    DragMode m_dragMode=DragMode::Vertical;
    int m_tickCount=0;
    int m_diameter=DefaultDiameter;

    internal::Text *m_valueText=nullptr;
    internal::Text *m_caption=nullptr;
    //! The value text's width at the range's ends, from the last X measure.
    int m_valueColumn=0;

    bool m_depressed=false;
    //! The value at the press, and the motion since then (units of vertical
    //! motion, or degrees turned) times Shift's factor, held at the range's
    //! ends: the drag's value is m_dragBase plus m_dragUnits times the
    //! mode's value per unit.
    double m_dragBase=0.0;
    double m_dragUnits=0.0;
    int m_lastMouseY=0;
    double m_lastAngle=0.0;
    bool m_haveAngle=false;

    bool m_editing=false;
    internal::DragNumberTextBox *m_editBox=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_knob_h_
