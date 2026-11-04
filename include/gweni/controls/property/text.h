/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_property_text_h_
#define _gweni_controls_property_text_h_

#include <gweni/controls/property/baseProperty.h>
#include <gweni/controls/textBox.h>

namespace gweni
{
namespace controls
{
namespace property
{

class GWENI_EXPORT Text: public Base
{
public:

    GWENI_CONTROL_INLINE(Text, Base)
    {
        m_textBox=newChild<TextBox>();
        m_textBox->setDock(DockPosition::Center);
        m_textBox->setShouldDrawBackground(false);
        m_textBox->onTextChangedCaller.add(this, &ParentClass::onPropertyValueChanged);
    }

public:
    String getPropertyValue() override
    {
        return m_textBox->getText();
    }

    void setPropertyValue(const String &v, bool fireChangeEvents) override
    {
        m_textBox->setText(v, fireChangeEvents);
    }

    bool isEditing() override
    {
        return m_textBox->isFocussed();
    }

    bool isHovered() const override
    {
        return ParentClass::isHovered() || m_textBox->isHovered();
    }

    TextBox *m_textBox;
};


}//namespace property
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_property_text_h_
