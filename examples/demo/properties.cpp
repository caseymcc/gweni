#include "demo/properties.h"
#include "gweni/controls/properties.h"
#include "gweni/controls/propertyTree.h"
#include "gweni/controls/property/colorSelector.h"
#include "gweni/controls/property/checkBox.h"
#include "gweni/controls/property/comboBox.h"

namespace gweni
{

PropertiesDemo::PropertiesDemo(const String &name):
    Base()
{
    {
        controls::Properties *props=newChild<controls::Properties>();
        props->setBounds(10, 10, 150, 300);
        {
            {
                controls::PropertyRow *row=props->add("First Name");
                row->onChange.add(this, &PropertiesDemo::onFirstNameChanged);
            }
            props->add("Middle Name");
            props->add("Last Name");
        }
    }
    {
        controls::PropertyTree *ptree=newChild<controls::PropertyTree>();
        ptree->setBounds(200, 10, 200, 200);
        {
            controls::Properties *props=ptree->add("Item One");
            props->add("Middle Name");
            props->add("Last Name");
            props->add("Four");
        }
        {
            controls::Properties *props=ptree->add("Item Two");
            props->add("More Items");
            props->add("CheckBox", props->newChild<controls::property::CheckBox>(), "1");
            props->add("To Fill");
            props->add("ColorSelector", props->newChild<controls::property::ColorSelector>(), "255 0 0");
            props->add("Out Here");
            // Combo Box Test
            {
                controls::property::ComboBox *combo=props->newChild<controls::property::ComboBox>();
                combo->getComboBox()->addItem("Option One", "one");
                combo->getComboBox()->addItem("Number Two", "two");
                combo->getComboBox()->addItem("Door Three", "three");
                combo->getComboBox()->addItem("Four Legs", "four");
                combo->getComboBox()->addItem("Five Birds", "five");
                controls::PropertyRow *row=props->add("ComboBox", combo, "1");
                row->onChange.add(this, &PropertiesDemo::onFirstNameChanged);
            }
        }
        ptree->expandAll();
    }
}

void PropertiesDemo::onFirstNameChanged(event::Info info)
{
    //controls::PropertyRow *row=(controls::PropertyRow *) control;
    //UnitPrint(Format("First Name Changed: %ls", row->getProperty()->getPropertyValue().getUnicode().c_str()));
}

}//namespace gweni
