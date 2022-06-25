/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_property_baseproperty_h_
#define _gweni_controls_property_baseproperty_h_

#include <gweni/controls/base.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/utility.h>


namespace gweni
{
namespace controls
{
namespace property
{

class GWENI_EXPORT Base: public controls::Base
{
public:

    GWENI_CONTROL_INLINE(Base, controls::Base)
    {
        setHeight(17);
    }

public:
    virtual String getPropertyValue() { return 0; }

    virtual void setPropertyValue(const String &v,
    bool fireChangeEvents=false) {}

    virtual bool isEditing() { return false; }

    virtual void doChanged()
    {
        event::Info info(this);

        info.string=getPropertyValue();
        onChange.call(this, info);
    }

    void onPropertyValueChanged(event::Info)
    {
        doChanged();
    }

    event::Caller onChange;
};


}//namespace property
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_property_baseproperty_h_
