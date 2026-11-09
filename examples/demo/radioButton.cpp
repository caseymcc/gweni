#include "demo/radioButton.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/radioButtonController.h"

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(RadioButtonDemo)
{
    setLayout(new controls::VerticalLayout());

    controls::RadioButtonController *rc=newChild<controls::RadioButtonController>();
    rc->addOption("Option 1");
    rc->addOption("Option 2");
    rc->addOption("Option 3");
    rc->addOption("Option 4");
    rc->setBounds(30, 30, 200, 200);
    rc->onSelectionChange.add(this, &RadioButtonDemo::onChange);

    // a disabled group, "Option B" chosen: the disabled look, checked and
    // unchecked
    controls::RadioButtonController *disabled=newChild<controls::RadioButtonController>();
    for(const char *text:{"Option A", "Option B"})
    {
        controls::LabeledRadioButton *option=disabled->addOption(text);
        option->setDisabled(true);
        if(String(text) == "Option B")
            option->select();
    }
}

void RadioButtonDemo::onChange(event::Info info)
{
//    controls::RadioButtonController *rc=(controls::RadioButtonController *) control;
//    controls::LabeledRadioButton *selected=rc->GetSelected();
    //			UnitPrint( Format( L"RadioButton changed (using 'onChange' event)\n Chosen Item: '%ls'", selected->getLabel()->getText().getUnicode().c_str() ) );
}

}//namespace gweni
