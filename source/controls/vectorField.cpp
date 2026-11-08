/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/vectorField.h>
#include <gweni/controls/dragNumber.h>
#include <gweni/skin.h>

#include <algorithm>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(VectorFieldTag)
{
    setDefaultTextAlignment(Alignment::Center);
    setText(VectorField::axisName(0));
    updateColors();
}

void VectorFieldTag::setAxis(int axis)
{
    axis=std::min(std::max(axis, 0), VectorField::MaxComponents-1);
    if(axis == m_axis)
        return;

    m_axis=axis;
    setText(VectorField::axisName(axis));
    // the renderer picks the axis's surface
    addStateChange(StateChange_Background);
}

void VectorFieldTag::updateColors()
{
    setTextColor(isDisabled()?getSkin()->Colors.Button.Disabled:getSkin()->Colors.AxisTagText);
}

}//namespace internal


GWENI_CONTROL_CONSTRUCTOR(VectorField)
{
    setSize(300, 20);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setComponentCount(3);
}

void VectorField::componentSpan(int width, int count, int index, int &x, int &w)
{
    count=std::min(std::max(count, MinComponents), MaxComponents);

    const int available=std::max(0, width-Gap*(count-1));
    const int each=available/count;

    x=index*(each+Gap);
    w=(index == count-1)?available-each*(count-1):each;
}

const char *VectorField::axisName(int axis)
{
    static const char *const names[MaxComponents]={"X", "Y", "Z", "W"};

    return names[std::min(std::max(axis, 0), MaxComponents-1)];
}

// A component is its tag (when the row has tags) and its field, in that order,
// so the fields are numbered, and tabbed through, in component order.
void VectorField::addComponent()
{
    const int index=m_count;

    if(m_tags)
    {
        internal::VectorFieldTag *tag=newChild<internal::VectorFieldTag>();

        tag->setPlacement(Placement::Owner);
        tag->setAxis(index);
        tag->setDisabled(isDisabled());
        m_tagControls[index]=tag;
    }

    DragNumber *field=newChild<DragNumber>();

    field->setPlacement(Placement::Owner);
    field->setInteger(m_integer);
    field->setSpeed(m_speed);
    if(!m_format.empty())
        field->setFormat(m_format);
    if(m_hasRange)
        field->setRange(m_min, m_max);
    field->setDisabled(isDisabled());
    field->onValueChanged.add(this, &VectorField::onFieldChanged);
    m_fields[index]=field;
    ++m_count;
}

void VectorField::removeComponent()
{
    --m_count;
    delete m_fields[m_count];
    delete m_tagControls[m_count];
    m_fields[m_count]=nullptr;
    m_tagControls[m_count]=nullptr;
}

void VectorField::setComponentCount(int count)
{
    count=std::min(std::max(count, MinComponents), MaxComponents);
    if(count == m_count)
        return;

    while(m_count < count)
        addComponent();
    while(m_count > count)
        removeComponent();
    invalidate();
}

void VectorField::setValue(int index, double value, bool doEvents)
{
    if(DragNumber *field=getField(index))
        field->setNumber(value, doEvents);
}

double VectorField::getValue(int index) const
{
    const DragNumber *field=getField(index);

    return field?field->getNumber():0.0;
}

void VectorField::setValues(const std::array<double, 4> &values, bool doEvents)
{
    for(int i=0; i < m_count; ++i)
        m_fields[i]->setNumber(values[i], doEvents);
}

std::array<double, 4> VectorField::getValues() const
{
    std::array<double, 4> values={0.0, 0.0, 0.0, 0.0};

    for(int i=0; i < m_count; ++i)
        values[i]=m_fields[i]->getNumber();
    return values;
}

void VectorField::setRange(double min, double max)
{
    if(min > max)
        std::swap(min, max);

    m_hasRange=true;
    m_min=min;
    m_max=max;
    for(int i=0; i < m_count; ++i)
        m_fields[i]->setRange(min, max);
}

void VectorField::setRange(int index, double min, double max)
{
    if(DragNumber *field=getField(index))
        field->setRange(min, max);
}

void VectorField::clearRange()
{
    m_hasRange=false;
    for(int i=0; i < m_count; ++i)
        m_fields[i]->clearRange();
}

void VectorField::setSpeed(double unitsPerUnit)
{
    m_speed=unitsPerUnit > 0.0?unitsPerUnit:0.0;
    for(int i=0; i < m_count; ++i)
        m_fields[i]->setSpeed(m_speed);
}

bool VectorField::setFormat(const String &format)
{
    // the first field refuses it with its one line, or takes it like the rest
    if(!m_fields[0]->setFormat(format))
        return false;

    m_format=format;
    for(int i=1; i < m_count; ++i)
        m_fields[i]->setFormat(format);
    return true;
}

void VectorField::setInteger(bool integer)
{
    m_integer=integer;
    for(int i=0; i < m_count; ++i)
        m_fields[i]->setInteger(integer);
}

// The tags go before their fields: the fields are taken out and put back after
// them, so the child order (and the tab order) stays tag, field, tag, field.
void VectorField::setAxisTags(bool tags)
{
    if(tags == m_tags)
        return;

    m_tags=tags;
    for(int i=0; i < m_count; ++i)
    {
        if(!tags)
        {
            delete m_tagControls[i];
            m_tagControls[i]=nullptr;
            continue;
        }

        internal::VectorFieldTag *tag=newChild<internal::VectorFieldTag>();

        tag->setPlacement(Placement::Owner);
        tag->setAxis(i);
        tag->setDisabled(isDisabled());
        m_tagControls[i]=tag;
        // after the tag: moved to the end, the fields keep their order
        m_fields[i]->bringToFront();
    }
    invalidate();
}

DragNumber *VectorField::getField(int index) const
{
    if(index < 0 || index >= m_count)
        return nullptr;
    return m_fields[index];
}

internal::VectorFieldTag *VectorField::getTag(int index) const
{
    if(index < 0 || index >= m_count)
        return nullptr;
    return m_tagControls[index];
}

void VectorField::setDisabled(bool active)
{
    if(active == isDisabled())
        return;

    ParentClass::setDisabled(active);
    for(int i=0; i < m_count; ++i)
    {
        m_fields[i]->setDisabled(active);
        if(m_tagControls[i])
            m_tagControls[i]->setDisabled(active);
    }
}

void VectorField::onFieldChanged(event::Info info)
{
    for(int i=0; i < m_count; ++i)
    {
        if(m_fields[i] != info.controlCaller)
            continue;

        event::Info changed(m_fields[i]);

        changed.integer=static_cast<size_t>(i);
        onChanged.call(this, changed);
        return;
    }
}

void VectorField::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim != Dim::X)
        return;

    const int perComponent=MinFieldWidth+(m_tags?TagWidth:0);
    const Padding &padding=getPadding();

    m_preferredSize.width=preferredFromContent(m_count*perComponent+Gap*(m_count-1)
        +padding.left+padding.right, Dim::X);
}

void VectorField::layout(skin::Base *skin)
{
    ParentClass::layout(skin);

    const Padding &padding=getPadding();
    const int width=getWidth()-padding.left-padding.right;
    const int height=std::max(0, getHeight()-padding.top-padding.bottom);

    for(int i=0; i < m_count; ++i)
    {
        int x, w;

        componentSpan(width, m_count, i, x, w);
        x+=padding.left;

        int tagWidth=0;

        if(m_tagControls[i])
        {
            tagWidth=std::min(TagWidth, w);
            placeChild(m_tagControls[i], x, padding.top, tagWidth, height);
        }
        placeChild(m_fields[i], x+tagWidth, padding.top, w-tagWidth, height);
    }
}

}//namespace controls
}//namespace gweni
