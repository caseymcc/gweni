/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_properties_h_
#define _gweni_controls_properties_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/property/baseProperty.h>
#include <gweni/controls/property/text.h>
#include <gweni/controls/splitterBar.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{
class PropertyRow;

class GWENI_EXPORT Properties: public Base
{
public:

    GWENI_CONTROL(Properties, Base);

public:
    void postLayout(gweni::skin::Base *skin) override;

    PropertyRow *add(const String &text, const String &value="");
    PropertyRow *add(const String &text, property::Base *prop,
        const String &value="");
    PropertyRow *find(const String &text);

    virtual int getSplitWidth();

    virtual void clear();

protected:

    virtual void onSplitterMoved(event::Info info);

    controls::SplitterBar *m_splitterBar;

};


class GWENI_EXPORT PropertyRow: public Base
{
public:

    GWENI_CONTROL(PropertyRow, Base);

public:
    virtual Label *getLabel()
    {
        return m_label;
    }

    virtual void            setProperty(property::Base *prop);
    virtual property::Base *getProperty()
    {
        return m_property;
    }

    void layout(gweni::skin::Base *skin) override;
    void render(gweni::skin::Base *skin) override;

    virtual bool isEditing()
    {
        return m_property && m_property->isEditing();
    }

    bool isHovered() const override
    {
        return ParentClass::isHovered() || (m_property && m_property->isHovered());
    }

    virtual void onEditingChanged();
    virtual void onHoverChanged();

    event::Caller onChange;

protected:

    void onPropertyValueChanged(event::Info info);

    Label *m_label;
    property::Base *m_property;

    bool m_lastEditing;
    bool m_lastHover;

};


}
}
#endif//_gweni_controls_properties_h_
