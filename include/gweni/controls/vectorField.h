/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_vectorfield_h_
#define _gweni_controls_vectorfield_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>

#include <array>

namespace gweni
{
namespace controls
{

class DragNumber;

namespace internal
{

//! A VectorField component's axis tag: its letter (X, Y, Z or W) centred on a
//! block of the axis's colour, the textured skin's VectorTagX..VectorTagW
//! surfaces, in the skin's AxisTagText colour. It takes no input: a press on
//! it goes to the row behind.
class GWENI_EXPORT VectorFieldTag: public Label
{
public:

    GWENI_CONTROL(VectorFieldTag, Label);

public:

    //! 0..3, X to W: the letter and, in the skin, the colour.
    void setAxis(int axis);
    int getAxis() const { return m_axis; }

    void updateColors() override;

private:

    int m_axis=0;
};

}//namespace internal

//! A row of 2 to 4 DragNumbers editing a vector (imgui's DragFloat2/3/4,
//! DragInt2/3/4), one per component, sharing a range, a speed, a format and
//! integer rounding (setRange(), setSpeed(), setFormat(), setInteger(), kept
//! for components added later; getField() reaches one component's
//! DragNumber, setRange(i, ...) one component's range).
//!
//! The components split the row's width in whole units with Gap units between
//! them: each is (width-Gap*(count-1))/count wide, the last one taking the
//! remainder, so the others keep their width when the row grows by less than
//! a unit per component (componentSpan()). With setAxisTags(true) each
//! component begins with a TagWidth-unit tag, an internal::VectorFieldTag
//! labelled X, Y, Z or W, its DragNumber taking the rest. The fields and tags
//! are owner-placed (layout()): the row is the height it was asked for, and
//! its content width MinFieldWidth (and a tag) per component and the gaps
//! (calculateSize()). By default it is Fixed at 300x20.
//!
//! onChanged fires once per real change of a component, from a drag, a typed
//! edit, a key step or setValue(): info.integer is the component's index and
//! info.control its DragNumber (event::Packet is the handler's own, set when
//! it was added, so the index travels in event::Info). A change marks only
//! that component's field. The fields are tabable in order and nothing else
//! in the row is, so Tab and Shift+Tab step from component to component.
class GWENI_EXPORT VectorField: public Base
{
public:

    GWENI_CONTROL(VectorField, Base);

public:

    //! A container of its fields and tags.
    GWENI_DRAWS_NOTHING

    static constexpr int MinComponents=2;
    static constexpr int MaxComponents=4;
    //! Units between two components.
    static constexpr int Gap=4;
    //! An axis tag's width.
    static constexpr int TagWidth=14;
    //! The narrowest a field is in the row's preferred width.
    static constexpr int MinFieldWidth=48;

    //! Where component \p index lies in a row \p width units wide holding
    //! \p count components: x from the row's left edge, and its width (the
    //! tag's and the field's together). A width too small for the gaps gives
    //! the components no width.
    static void componentSpan(int width, int count, int index, int &x, int &w);

    //! The letter of axis \p axis: "X", "Y", "Z" or "W".
    static const char *axisName(int axis);

    //! 2 to 4 (clamped). Components added get the shared range, speed, format
    //! and rounding and the value 0; removed ones are deleted with their ids.
    void setComponentCount(int count);
    int getComponentCount() const { return m_count; }

    //! Component \p index's value, clamped and rounded by its field.
    //! onChanged fires with \p doEvents if the value changed. Nothing for an
    //! index out of range.
    void setValue(int index, double value, bool doEvents=true);
    //! 0 for an index out of range.
    double getValue(int index) const;
    //! The first getComponentCount() of \p values, each as setValue().
    void setValues(const std::array<double, 4> &values, bool doEvents=true);
    //! The components, 0 past getComponentCount().
    std::array<double, 4> getValues() const;

    //! Base's text value, unchanged.
    using Base::getValue;
    using Base::setValue;

    //! Every component's range (DragNumber::setRange), and the one later
    //! components get.
    void setRange(double min, double max);
    //! Component \p index's range alone.
    void setRange(int index, double min, double max);
    void clearRange();
    //! Every component's speed (DragNumber::setSpeed).
    void setSpeed(double unitsPerUnit);
    //! Every component's format (DragNumber::setFormat); false, and nothing
    //! changed, if the format is refused.
    bool setFormat(const String &format);
    //! Every component's rounding (DragNumber::setInteger).
    void setInteger(bool integer);
    bool isInteger() const { return m_integer; }

    //! The X/Y/Z/W tags before the fields.
    void setAxisTags(bool tags);
    bool hasAxisTags() const { return m_tags; }

    //! Component \p index's DragNumber, null out of range.
    DragNumber *getField(int index) const;
    //! Component \p index's tag, null out of range or without tags.
    internal::VectorFieldTag *getTag(int index) const;

    void setDisabled(bool active) override;

    //! Its width: MinFieldWidth (and a tag) per component and the gaps, given
    //! to the size flags (a Fixed row, the default, is the width asked for);
    //! its height: the one asked for.
    void calculateSize(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;
    bool sizedByRequest(Dim dim) const override { return dim == Dim::Y; }

    //! A component changed: info.integer its index, info.control its field.
    event::Caller onChanged;

protected:

    void addComponent();
    void removeComponent();
    void onFieldChanged(event::Info info);

    int m_count=0;
    DragNumber *m_fields[MaxComponents]={nullptr, nullptr, nullptr, nullptr};
    internal::VectorFieldTag *m_tagControls[MaxComponents]={nullptr, nullptr, nullptr, nullptr};
    bool m_tags=false;

    bool m_hasRange=false;
    double m_min=0.0;
    double m_max=0.0;
    double m_speed=0.0;
    String m_format;
    bool m_integer=false;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_vectorfield_h_
