/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/radioButtonController.h>
#include <gweni/controls/radioButton.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(RadioButtonController)
{
    m_selected=nullptr;
    setTabable(false);
    setKeyboardInputEnabled(false);
}

void RadioButtonController::onRadioClicked(event::Info info)
{
    auto fromPanel=info.controlCaller;

    RadioButton *checkedRadioButton=gweni_cast<RadioButton>(fromPanel);

    // Iterate through all other buttons and set them to false;
    for(auto &&child : getChildren())
    {
        LabeledRadioButton *lRB=gweni_cast<LabeledRadioButton>(child);

        if(lRB)
        {
            RadioButton *childRadioButton=lRB->getRadioButton();

            if(childRadioButton == checkedRadioButton)
                m_selected=lRB;
            else
                lRB->getRadioButton()->setChecked(false);
        }
    }

    onChange();
}

void RadioButtonController::onChange()
{
    onSelectionChange.call(this);
}

LabeledRadioButton *RadioButtonController::addOption(const gweni::String &text,
    const gweni::String &optionName)
{
    LabeledRadioButton *lrb=newChild<LabeledRadioButton>();
    lrb->setName(optionName);
    lrb->getLabel()->setText(text);
    lrb->getRadioButton()->onChecked.add(this, &RadioButtonController::onRadioClicked);
    lrb->setDock(DockPosition::Top);
    lrb->setMargin(Margin(0, 1, 0, 1));
    lrb->setKeyboardInputEnabled(false);
    lrb->setTabable(false);
    invalidate();
    return lrb;
}

}//namespace controls
}//namespace gweni
