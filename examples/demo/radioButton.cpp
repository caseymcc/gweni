#include "demo/radioButton.h"
#include "gweni/controls/radioButtonController.h"

namespace gweni
{

RadioButtonDemo::RadioButtonDemo(const String &name):
    Base()
{
    controls::RadioButtonController *rc=newChild<controls::RadioButtonController>();
    rc->addOption("Option 1");
    rc->addOption("Option 2");
    rc->addOption("Option 3");
    rc->addOption("Option 4");
    rc->setBounds(30, 30, 200, 200);
    rc->onSelectionChange.add(this, &RadioButtonDemo::onChange);
}

void RadioButtonDemo::onChange(event::Info info)
{
//    controls::RadioButtonController *rc=(controls::RadioButtonController *) control;
//    controls::LabeledRadioButton *selected=rc->GetSelected();
    //			UnitPrint( Format( L"RadioButton changed (using 'onChange' event)\n Chosen Item: '%ls'", selected->getLabel()->getText().getUnicode().c_str() ) );
}

}//namespace gweni
