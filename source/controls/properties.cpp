/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/properties.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(Properties)
{
    m_splitterBar=new SplitterBar(this);
    m_splitterBar->setPos(80, 0);
    m_splitterBar->setCursor(gweni::cursor::SizeWE);
    m_splitterBar->onDragged.add(this, &Properties::onSplitterMoved);
    m_splitterBar->setShouldDrawBackground(false);
    m_splitterBar->doNotIncludeInSize();
}

void Properties::postLayout(gweni::skin::Base * /*skin*/)
{
    if(sizeToChildren(false, true))
        invalidateParent();

    m_splitterBar->setSize(3, getHeight());
}

void Properties::onSplitterMoved(event::Info)
{
    invalidateChildren();
}

int Properties::getSplitWidth()
{
    return m_splitterBar->getX();
}

PropertyRow *Properties::add(const String &text, const String &value)
{
    return add(text, new property::Text(this), value);
}

PropertyRow *Properties::add(const String &text, property::Base *prop, const String &value)
{
    PropertyRow *row=new PropertyRow(this);
    row->dock(Position::Top);
    row->getLabel()->setText(text);
    row->setProperty(prop);
    prop->setPropertyValue(value, true);
    m_splitterBar->bringToFront();
    return row;
}

PropertyRow *Properties::find(const String &text)
{
    for(auto &&control : getChildren())
    {
        PropertyRow *row=gweni_cast<PropertyRow>(control);

        if(!row)
            continue;

        if(row->getLabel()->getText() == text)
            return row;
    }

    return nullptr;
}

void Properties::clear()
{
    Base::List ChildListCopy=getChildren();

    for(auto &&child : ChildListCopy)
    {
        PropertyRow *row=gweni_cast<PropertyRow>(child);

        if(!row)
            continue;

        row->delayedDelete();
    }
}

class PropertyRowLabel: public Label
{
    GWENI_CONTROL_INLINE(PropertyRowLabel, Label)
    {
        setAlignment(Position::Left|Position::CenterV);
        m_propertyRow=nullptr;
    }

    void updateColors() override
    {
        if(isDisabled())
            return setTextColor(getSkin()->Colors.Button.Disabled);

        if(m_propertyRow && m_propertyRow->isEditing())
            return setTextColor(getSkin()->Colors.Properties.Label_Selected);

        if(m_propertyRow && m_propertyRow->isHovered())
            return setTextColor(getSkin()->Colors.Properties.Label_Hover);

        setTextColor(getSkin()->Colors.Properties.Label_Normal);
    }

    void SetPropertyRow(PropertyRow *p)
    {
        m_propertyRow=p;
    }

protected:

    PropertyRow *m_propertyRow;
};


GWENI_CONTROL_CONSTRUCTOR(PropertyRow)
{
    m_property=nullptr;
    PropertyRowLabel *label=new PropertyRowLabel(this);
    label->SetPropertyRow(this);
    label->dock(Position::Left);
    label->setAlignment(Position::Left|Position::CenterV);
    label->setMargin(Margin(2, 0, 0, 0));
    label->setSizeFlags({SizeFlag::Fixed, SizeFlag::Elastic});
    m_label=label;
}

void PropertyRow::render(gweni::skin::Base *skin)
{
    /* SORRY */
    if(isEditing() != m_lastEditing)
    {
        onEditingChanged();
        m_lastEditing=isEditing();
    }

    if(isHovered() != m_lastHover)
    {
        onHoverChanged();
        m_lastHover=isHovered();
    }

    /* SORRY */
//    skin->drawPropertyRow(this, skin::Generate, m_label->right(), isEditing(), isHovered()|
//        m_property->isHovered());
    skin->drawControl(this);
}

void PropertyRow::layout(gweni::skin::Base * /*skin*/)
{
    Properties *parent=gweni_cast<Properties>(getParent());

    if(!parent)
        return;

    m_label->setWidth(parent->getSplitWidth());

    if(m_property)
        setHeight(std::max(m_label->getHeight(), m_property->getHeight()));
}

void PropertyRow::setProperty(property::Base *prop)
{
    m_property=prop;
    m_property->setParent(this);
    m_property->dock(Position::Fill);
    m_property->setSizeFlags({SizeFlag::Fixed, SizeFlag::Elastic});
    m_property->onChange.add(this, &ThisClass::onPropertyValueChanged);
}

void PropertyRow::onPropertyValueChanged(event::Info info)
{
    event::Info sinfo(info);
    sinfo.string=getProperty()->getPropertyValue();
    onChange.call(this, sinfo);
}

void PropertyRow::onEditingChanged()
{
    m_label->redraw();
}

void PropertyRow::onHoverChanged()
{
    m_label->redraw();
}

}//namespace controls
}//namespace gweni
