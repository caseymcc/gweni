/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_fieldlabel_h_
#define _gweni_controls_fieldlabel_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/label.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT FieldLabel: public controls::Label
{
public:

    static inline FieldLabel *setup(controls::Base *control, const gweni::String &text)
    {
        FieldLabel *plbl=new FieldLabel(control->getParent());
        plbl->setText(text);
        plbl->setSize(control->getWidth(), control->getHeight());
        plbl->dock(control->getDock());
        plbl->setField(control);
        return plbl;
    }

public:

    GWENI_CONTROL_INLINE(FieldLabel, controls::Label)
    {
        m_field=nullptr;
        setMargin(Margin(0, 1, 0, 1));
        setAlignment(Position::CenterV|Position::Left);
    }

public:
    void setField(controls::Base *field)
    {
        field->setParent(this);
        field->dock(Position::Right);
        m_field=field;
    }

    void layout(gweni::skin::Base *pskin) override
    {
        m_field->setWidth(getWidth()-70);
        ParentClass::layout(pskin);
    }

protected:

    controls::Base *m_field;

};


}
}

#endif//_gweni_controls_fieldlabel_h_
