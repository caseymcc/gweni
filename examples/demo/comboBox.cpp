#include "demo/comboBox.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/comboBox.h"

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(ComboBoxDemo)
{
    setLayout(new controls::VerticalLayout());

    {
        controls::ComboBox *combo=newChild<controls::ComboBox>();

        combo->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
        combo->setWidth(200);
        combo->addItem("Option One", "one");
        combo->addItem("Number Two", "two");
        combo->addItem("Door Three", "three");
        combo->addItem("Four Legs", "four");
        combo->addItem("Five Birds", "five");
        combo->onSelection.add(this, &ComboBoxDemo::onComboSelect);
    }
    {
        // Empty..
        controls::ComboBox *combo=newChild<controls::ComboBox>();

        combo->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
        combo->setWidth(200);
    }
    {
        // Lots of items
        controls::ComboBox *combo=newChild<controls::ComboBox>();

        combo->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
        combo->setWidth(200);

        for(int i=0; i < 500; i++)
        {
            combo->addItem("Lots Of Options");
        }
    }
}

void ComboBoxDemo::onComboSelect(event::Info info)
{
//    controls::ComboBox *combo=(controls::ComboBox *) control;
//    UnitPrint(Format(L"Combo Changed: %ls", combo->getSelectedItem()->getText().getUnicode().c_str()));
}


}//namespace gweni
