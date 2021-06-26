/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_property_checkbox_h_
#define _gweni_controls_property_checkbox_h_

#include <gweni/controls/property/baseProperty.h>
#include <gweni/controls/checkBox.h>

namespace gweni
{
namespace controls
{
namespace property
{
class GWENI_EXPORT CheckBox: public property::Base
{
public:

    GWENI_CONTROL_INLINE(CheckBox, property::Base)
    {
        m_checkbox=new gweni::controls::CheckBox(this);
        m_checkbox->setShouldDrawBackground(false);
        m_checkbox->onCheckChanged.add(this, &ParentClass::onPropertyValueChanged);
        m_checkbox->setTabable(true);
        m_checkbox->setKeyboardInputEnabled(true);
        m_checkbox->setPos(2, 1);
        setHeight(18);
    }

    String getPropertyValue() override
    {
        return m_checkbox->isChecked()?"1":"0";
    }

    void setPropertyValue(const String &v, bool fireChangeEvents) override
    {
        if(v == "1" || v == "true" || v == "TRUE" || v == "yes" || v == "YES")
            return m_checkbox->setChecked(true);

        return m_checkbox->setChecked(false);
    }

    bool isEditing() override
    {
        return m_checkbox->isFocussed();
    }

    bool isHovered() const override
    {
        return ParentClass::isHovered() || m_checkbox->isHovered();
    }

    gweni::controls::CheckBox *m_checkbox;
};


}
}
}
#endif//_gweni_controls_property_checkbox_h_
