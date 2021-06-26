/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_radiobottoncontroller_h_
#define _gweni_controls_radiobottoncontroller_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/radioButton.h>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT RadioButtonController: public Base
{
public:

    GWENI_CONTROL(RadioButtonController, Base);

    void render(skin::Base * /*skin*/) override
    {}

    virtual LabeledRadioButton *addOption(const gweni::String &text,
        const gweni::String &optionName="");

    virtual LabeledRadioButton *getSelected()
    {
        return m_selected;
    }

    virtual const gweni::String &getSelectedName()
    {
        return m_selected->getName();
    }

    virtual const String &getSelectedLabel()
    {
        return m_selected->getLabel()->getText();
    }

    event::Caller onSelectionChange;

    virtual void onRadioClicked(event::Info);

    virtual void onChange();

private:

    LabeledRadioButton *m_selected;
};


}
}
#endif//_gweni_controls_radiobottoncontroller_h_
