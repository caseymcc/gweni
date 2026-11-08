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
    m_splitterBar->setPlacement(Placement::Owner);
}

// The rows are docked Top, so this control's preferred height is theirs. The
// bar runs down the split, its height this control's: it is this control's to
// place (Placement::Owner), at the start of each axis's arrange, once the size
// it is taken from is final.
void Properties::arrange(gweni::skin::Base *skin, Dim dim)
{
    if(dim == Dim::X)
        placeChild(m_splitterBar, getSplitWidth(), 0, SplitterBarWidth, m_splitterBar->getHeight());
    else
        placeChild(m_splitterBar, m_splitterBar->getX(), 0, SplitterBarWidth, getHeight());

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
    // The row places its label and its property itself (Placement::Owner):
    // the label is as wide as the split, which the Properties gives it, not
    // its own request.
    PropertyRowLabel *label=newChild<PropertyRowLabel>();
    label->SetPropertyRow(this);
    label->setPlacement(Placement::Owner);
    label->setMargin(Margin(2, 0, 0, 0));
    m_label=label;
}

// The row's look follows its own hover and its property's hover and focus,
// polled when one of them changes: the render phase calls only the hooks of a
// control that changed itself, never a clean row's for a change below it.
void PropertyRow::pollInteraction()
{
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
}

void PropertyRow::onMouseEnter()
{
    ParentClass::onMouseEnter();
    pollInteraction();
}

void PropertyRow::onMouseLeave()
{
    ParentClass::onMouseLeave();
    pollInteraction();
}

void PropertyRow::onDescendantInteraction(Base *control)
{
    ParentClass::onDescendantInteraction(control);
    pollInteraction();
}

void PropertyRow::render(gweni::skin::Base *skin)
{
//    skin->drawPropertyRow(this, skin::Generate, m_label->right(), isEditing(), isHovered()|
//        m_property->isHovered());
    //skin->drawControl(this);
    if(getStateChange() != StateChange_Nothing)
        m_skinControl->update(skin->getRenderer(), this);
}

// The label column is the split and the property the rest of the row. The
// width asked for is the split as set, not as clamped to the Properties'
// current width, which an earlier pass gave it: the label's margins, the split
// and the property's preferred width. The height is the taller of the two.
// Both are still measured, for their own subtrees.
void PropertyRow::calculateSize(gweni::skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    const Margin &labelMargin=m_label->getMargin();

    if(dim == Dim::X)
    {
        Properties *parent=gweni_cast<Properties>(getParent());
        int width=m_padding.left+m_padding.right+labelMargin.left+labelMargin.right
            +(parent?parent->getRequestedSplitWidth():m_label->getPreferredSize().width);

        if(m_property)
            width+=m_property->getPreferredSize().width+m_property->getMargin().left
                +m_property->getMargin().right;

        m_preferredSize.width=preferredFromContent(width, Dim::X);
    }
    else
    {
        int height=m_label->getPreferredSize().height+labelMargin.top+labelMargin.bottom;

        if(m_property)
            height=std::max(height, m_property->getPreferredSize().height
                +m_property->getMargin().top+m_property->getMargin().bottom);

        m_preferredSize.height=preferredFromContent(height+m_padding.top+m_padding.bottom, Dim::Y);
    }
}

// The label from the left padding to the split, clamped to the Properties'
// width, which is final in the X arrange; the property from there to the
// right padding. Both fill the row's height inside its padding.
void PropertyRow::arrange(gweni::skin::Base *skin, Dim dim)
{
    const Margin &labelMargin=m_label->getMargin();

    if(dim == Dim::X)
    {
        Properties *parent=gweni_cast<Properties>(getParent());
        const int labelX=m_padding.left+labelMargin.left;
        const int labelWidth=parent?parent->getSplitWidth():m_label->getPreferredSize().width;

        placeChild(m_label, labelX, m_label->getY(), labelWidth, m_label->getHeight());

        if(m_property)
        {
            const Margin &margin=m_property->getMargin();
            const int x=labelX+labelWidth+labelMargin.right+margin.left;

            placeChild(m_property, x, m_property->getY(),
                std::max(0, getWidth()-m_padding.right-margin.right-x), m_property->getHeight());
        }
    }
    else
    {
        const int innerHeight=getHeight()-m_padding.top-m_padding.bottom;

        placeChild(m_label, m_label->getX(), m_padding.top+labelMargin.top, m_label->getWidth(),
            std::max(0, innerHeight-labelMargin.top-labelMargin.bottom));

        if(m_property)
        {
            const Margin &margin=m_property->getMargin();

            placeChild(m_property, m_property->getX(), m_padding.top+margin.top, m_property->getWidth(),
                std::max(0, innerHeight-margin.top-margin.bottom));
        }
    }

    ParentClass::arrange(skin, dim);
}

void PropertyRow::setProperty(property::Base *prop)
{
    m_property=prop;
    m_property->setPlacement(Placement::Owner);
    m_property->setParent(this);
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
