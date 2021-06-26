/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_property_combobox_h_
#define _gweni_controls_property_combobox_h_

#include <gweni/controls/property/baseProperty.h>
#include <gweni/controls/comboBox.h>

namespace gweni
{
namespace controls
{
namespace property
{
class GWENI_EXPORT ComboBox: public property::Base
{
public:

    GWENI_CONTROL_INLINE(ComboBox, property::Base)
    {
        m_comboBox=new gweni::controls::ComboBox(this);
        m_comboBox->dock(Position::Fill);
        m_comboBox->onSelection.add(this, &ParentClass::onPropertyValueChanged);
        m_comboBox->setTabable(true);
        m_comboBox->setKeyboardInputEnabled(true);
        m_comboBox->setShouldDrawBackground(false);
        setHeight(18);
    }

    String getPropertyValue() override
    {
        gweni::controls::Label *control=m_comboBox->getSelectedItem();

        if(!control)
            return "";

        return control->getName();
    }

    void setPropertyValue(const String &val, bool fireChangeEvents) override
    {
        m_comboBox->selectItemByName(val, fireChangeEvents);
    }

    bool isEditing() override
    {
        return m_comboBox->isFocussed();
    }

    bool isHovered() const override
    {
        return ParentClass::isHovered() || m_comboBox->isHovered();
    }

    gweni::controls::ComboBox *getComboBox()
    {
        return m_comboBox;
    }

protected:

    gweni::controls::ComboBox *m_comboBox;
};


}
}
}
#endif//_gweni_controls_property_combobox_h_
