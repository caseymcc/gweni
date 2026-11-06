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
    m_splitWidth=80;
    m_splitterBar=newChild<SplitterBar>();
    m_splitterBar->setPos(m_splitWidth, 0);
    m_splitterBar->setCursor(gweni::cursor::SizeWE);
    m_splitterBar->onDragged.add(this, &Properties::onSplitterMoved);
    m_splitterBar->setShouldDrawBackground(false);
    m_splitterBar->doNotIncludeInSize();
    // Placed by arrange(), not by the alignment rules.
    m_splitterBar->setAlignment(Alignment::Absolute);
}

// The rows are docked Top, so this control's preferred height is theirs. The
// bar runs down the split, its height this control's: it is placed before
// each pass places it, once the size it is taken from is final.
void Properties::arrange(gweni::skin::Base *skin, Dim dim)
{
    {
        PlacementScope placement;

        if(dim == Dim::X)
            m_splitterBar->setBounds(getSplitWidth(), 0, SplitterBarWidth, m_splitterBar->getHeight());
        else
            m_splitterBar->setBounds(m_splitterBar->getX(), 0, SplitterBarWidth, getHeight());
    }

    ParentClass::arrange(skin, dim);
}

void Properties::onSplitterMoved(event::Info)
{
    m_splitWidth=m_splitterBar->getX();
    invalidateChildren();
}

int Properties::getSplitWidth()
{
    if(getWidth() > SplitterBarWidth)
        return std::max(0, std::min(m_splitWidth, getWidth()-SplitterBarWidth));

    return m_splitWidth;
}

PropertyRow *Properties::add(const String &text, const String &value)
{
    property::Text *textProperty=newChild<property::Text>();

    return add(text, textProperty, value);
}

PropertyRow *Properties::add(const String &text, property::Base *prop, const String &value)
{
    PropertyRow *row=newChild<PropertyRow>();
    row->setDock(DockPosition::Top);
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
        setAlignment(Alignment::Left|Alignment::CenterV);
        m_propertyRow=nullptr;
    }

public:
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
    // render() compares against these on its first call
    m_lastEditing=false;
    m_lastHover=false;
    PropertyRowLabel *label=newChild<PropertyRowLabel>();
    label->SetPropertyRow(this);
    label->setDock(DockPosition::Left);
    label->setAlignment(Alignment::Left|Alignment::CenterV);
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
    //skin->drawControl(this);
    if(getStateChange() != StateChange_Nothing)
        m_skinControl->update(skin->getRenderer(), this);
}

// The label is as wide as the split. It is Fixed, so its width is asked for
// before it is measured; the split is clamped to the Properties' width, which
// is final only in the X arrange, so the label is handed the clamped split
// there too. Docked Top, the row is as high as the taller of the label and
// the property, which its preferred height already is.
void PropertyRow::calculateSize(gweni::skin::Base *skin, Dim dim)
{
    Properties *parent=gweni_cast<Properties>(getParent());

    if(dim == Dim::X && parent)
    {
        PlacementScope placement;

        m_label->setWidth(parent->getSplitWidth());
    }

    ParentClass::calculateSize(skin, dim);
}

void PropertyRow::arrange(gweni::skin::Base *skin, Dim dim)
{
    Properties *parent=gweni_cast<Properties>(getParent());

    if(dim == Dim::X && parent)
    {
        Size preferred=m_label->getPreferredSize();

        preferred.width=parent->getSplitWidth();
        m_label->setPreferredSize(preferred);
    }

    ParentClass::arrange(skin, dim);
}

void PropertyRow::setProperty(property::Base *prop)
{
    m_property=prop;
    m_property->setParent(this);
    m_property->setDock(DockPosition::Center);
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
