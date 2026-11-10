/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dragnumber_h_
#define _gweni_controls_dragnumber_h_

#include <gweni/controls/label.h>
#include <gweni/controls/textBox.h>

namespace gweni
{
namespace controls
{

class DragNumber;

namespace internal
{

//! The TextBoxNumeric a DragNumber swaps in to type a value: Return commits
//! it and gives the focus back to the DragNumber (a TextBox's Return would tab
//! to the next control), Escape cancels it. It draws as a TextBox.
class GWENI_EXPORT DragNumberTextBox: public TextBoxNumeric
{
public:

    GWENI_CONTROL(DragNumberTextBox, TextBoxNumeric);

public:

    bool onKeyReturn(bool down) override;
    bool onKeyEscape(bool down) override;
    //! Up and Down stay in the box: unhandled, they would reach the
    //! DragNumber behind it and step its value.
    bool onKeyUp(bool /*down*/) override { return true; }
    bool onKeyDown(bool /*down*/) override { return true; }

    void setOwner(DragNumber *owner) { m_owner=owner; }

private:

    DragNumber *m_owner=nullptr;
};

}//namespace internal

//! A number field scrubbed by dragging (imgui's DragFloat/DragInt): it shows
//! its value as text centred in a field, and a horizontal drag changes it by
//! the mouse's motion times getSpeed(), ShiftFactor times as fast with Shift
//! held and AltFactor times with Alt, clamped to its range (none unless
//! setRange()). Motion is summed as it comes, so a slow drag at a low speed
//! still moves the value, and the drag goes back to where it started when the
//! mouse does. A press and release without motion changes nothing.
//!
//! A double-click, a Ctrl+click or Return while it has the focus swaps in a
//! TextBoxNumeric over the field with the value selected (beginEdit()):
//! Return or a lost focus commits it (parsed, clamped, rounded, formatted),
//! Escape cancels it, and text that is no number leaves the value as it was.
//! The box is made by the first edit and only shown or hidden after that, so
//! it keeps its ids. While it has the keyboard focus, Up and Right step the
//! value up by the speed and Down and Left down (Shift and Alt as for a drag,
//! a whole step at least for an integer field).
//!
//! The value is a double: Base already has a virtual getValue() returning a
//! String (the field's text form here), so the number is setNumber()/
//! getNumber(). onValueChanged fires once per real change of it, never for a
//! value that stays (the same number set again, a drag step clamped at the
//! range's end, an integer step of less than one). The shown text is set, and
//! StateChange_Text raised, only when the formatted string changes, so a drag
//! frame that keeps it marks nothing; hover, press, focus and the edit box
//! repaint the field alone (redraw()).
class GWENI_EXPORT DragNumber: public Label
{
public:

    GWENI_CONTROL(DragNumber, Label);

public:

    //! Drag and key-step multipliers while Shift (faster) or Alt (slower) is held.
    static constexpr double ShiftFactor=10.0;
    static constexpr double AltFactor=0.1;

    //! \p value clamped to the range and, for an integer field, rounded to the
    //! nearest whole number. onValueChanged fires with \p doEvents if that is
    //! not the value it had.
    void setNumber(double value, bool doEvents=true);
    double getNumber() const { return m_value; }

    //! Clamps the value to [min, max] (the two swapped when min > max), which
    //! may fire onValueChanged.
    void setRange(double min, double max);
    //! No range: any finite value.
    void clearRange();
    bool hasRange() const { return m_hasRange; }
    double getMin() const;
    double getMax() const;

    //! Value units per unit of mouse motion (and per key step). Zero or less
    //! gives the default back: 1 for an integer field, 0.1 otherwise.
    void setSpeed(double unitsPerUnit);
    double getSpeed() const;

    //! A printf format holding one conversion of a double (Slider::
    //! validValueFormat(): %f, %e, %g or %a with flags, a width and a
    //! precision, "%%" anywhere), e.g. "%.2f" or "%.1f ms"; anything else is
    //! refused with one line on stderr and the format kept. "" gives the
    //! default back: "%.0f" for an integer field, "%.3f" otherwise.
    bool setFormat(const String &format);
    //! The format in use, the default included.
    const String &getFormat() const;

    //! An integer field rounds its value to whole numbers (now and from then
    //! on) and has its own default speed and format.
    void setInteger(bool integer);
    bool isInteger() const { return m_integer; }

    //! \p value as the field shows it (getFormat()).
    String formatNumber(double value) const;
    //! The text the edit box starts with: the value with the format's
    //! precision for %f (6 otherwise), trailing zeros dropped; whole for an
    //! integer field.
    String editText() const;
    //! \p text as a number: a whole finite strtod() parse, spaces around it
    //! allowed. False, and \p value untouched, for anything else.
    static bool parseNumber(const String &text, double &value);

    //! Shows the edit box over the field, focused, its text selected.
    void beginEdit();
    //! Applies the edit box's text (if it is a number) and hides the box;
    //! \p refocus gives the keyboard focus back to the field, else the box
    //! just drops it. Nothing when not editing.
    void commitEdit(bool refocus=true);
    //! Hides the edit box, the value as it was.
    void cancelEdit(bool refocus=true);
    bool isEditing() const { return m_editing; }
    //! Null until the first beginEdit().
    internal::DragNumberTextBox *getEditBox() const { return m_editBox; }

    //! The left button is down on the field (a scrub in progress).
    bool isDepressed() const { return m_depressed; }

    //! The field's text form of the value (editText()), and a parse of one.
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
    bool onKeyReturn(bool down) override;

    void updateColors() override;
    //! While the edit box is open the field's own Text is hidden, and it is
    //! measured as if it showed, so the field keeps its size.
    void calculateSize(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;

    event::Caller onValueChanged;

protected:

    //! The multiplier the held modifiers give a drag or a key step.
    static double modifierFactor();
    //! \p value clamped and rounded as setNumber() stores it.
    double constrain(double value) const;
    void updateText();
    void step(double direction);
    //! Where the keyboard focus goes when an edit ends with the box focused:
    //! back to the field, nowhere, or (the box losing it) where it is going.
    enum class EditFocus { Field, Drop, Leave };
    void endEdit(bool commit, EditFocus after);
    void onEditLostFocus(event::Info info);

    double m_value=0.0;
    double m_min=0.0;
    double m_max=0.0;
    bool m_hasRange=false;
    double m_speed=0.0;
    String m_format;
    bool m_integer=false;

    bool m_depressed=false;
    //! The value at the press, and the motion since then times the modifiers'
    //! factors: the drag's value is m_dragBase+m_dragUnits*speed.
    double m_dragBase=0.0;
    double m_dragUnits=0.0;
    int m_lastMouseX=0;

    bool m_editing=false;
    internal::DragNumberTextBox *m_editBox=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_dragnumber_h_
