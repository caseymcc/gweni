#include "demo/listBox.h"
#include "gweni/controls/listBox.h"

namespace gweni
{

ListBoxDemo::ListBoxDemo(const String &name):
    Base()
{
    {
        controls::ListBox *ctrl=newChild<controls::ListBox>();
        ctrl->setBounds(10, 10, 100, 200);
        ctrl->addItem("First");
        ctrl->addItem("Blue");
        ctrl->addItem("Yellow");
        ctrl->addItem("Orange");
        ctrl->addItem("Brown");
        ctrl->addItem("Black");
        ctrl->addItem("Green");
        ctrl->addItem("Dog");
        ctrl->addItem("Cat Blue");
        ctrl->addItem("Shoes");
        ctrl->addItem("Shirts");
        ctrl->addItem("Chair");
        ctrl->addItem("Last");
        ctrl->selectByString("Bl*", true);
        ctrl->setAllowMultiSelect(true);
        ctrl->setKeyboardInputEnabled(true);
        ctrl->onRowSelectedCaller.add(this, &ListBoxDemo::rowSelected);
    }
    {
        controls::ListBox *ctrl=newChild<controls::ListBox>();
        ctrl->setBounds(120, 10, 200, 200);
        ctrl->setColumnCount(3);
        ctrl->setAllowMultiSelect(true);
        ctrl->onRowSelectedCaller.add(this, &ListBoxDemo::rowSelected);
        {
            controls::layout::TableRow *row=ctrl->addItem("Baked Beans");
            row->setCellText(1, "Heinz");
            row->setCellText(2, "£3.50");
        }
        {
            controls::layout::TableRow *row=ctrl->addItem("Bananas");
            row->setCellText(1, "Trees");
            row->setCellText(2, "$1.27");
        }
        {
            controls::layout::TableRow *row=ctrl->addItem("Chicken");
            row->setCellText(1, "\u5355\u5143\u6D4B\u8BD5");
            row->setCellText(2, "\u20AC8.95");
        }
    }
}


void ListBoxDemo::rowSelected(event::Info info)
{
//    controls::ListBox *ctrl=(controls::ListBox *) control;
//    UnitPrint(Format("Listbox Item Selected: %ls", ctrl->GetSelectedRow()->getText(0).getUnicode().c_str()));
}

}//namespace gweni
