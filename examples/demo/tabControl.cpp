#include "demo/tabControl.h"
#include "gweni/controls/tabControl.h"
#include "gweni/controls/radioButtonController.h"

namespace gweni
{

TabControlDemo::TabControlDemo(controls::Base *parent, const String &name):
    Base(parent)
{
    {
        m_dockControlLeft=new controls::TabControl(this);
        m_dockControlLeft->setBounds(10, 10, 200, 200);
        {
            controls::TabButton *button=m_dockControlLeft->addPage("Controls");
            Base *page=button->getPage();
            {
                controls::RadioButtonController *pRadio=new controls::RadioButtonController(page);
                pRadio->setBounds(10, 10, 100, 100);
                pRadio->addOption("Top")->select();
                pRadio->addOption("Bottom");
                pRadio->addOption("Left");
                pRadio->addOption("Right");
                pRadio->onSelectionChange.add(this, &ThisClass::onDockChange);
            }
        }
        m_dockControlLeft->addPage("Red");
        m_dockControlLeft->addPage("Green");
        m_dockControlLeft->addPage("Blue");
    }
    {
        controls::TabControl *pDragMe=new controls::TabControl(this);
        pDragMe->setBounds(220, 10, 200, 200);
        pDragMe->addPage("You");
        pDragMe->addPage("Can");
        pDragMe->addPage("Reorder")->setImage("test16.png");
        pDragMe->addPage("These");
        pDragMe->addPage("Tabs");
        pDragMe->setAllowReorder(true);
    }
}

void TabControlDemo::onDockChange(event::Info info)
{
    controls::RadioButtonController *rc=(controls::RadioButtonController *)info.controlCaller;

    if(rc->getSelectedLabel() == "Top") { m_dockControlLeft->setTabStripPosition(Position::Top); }

    if(rc->getSelectedLabel() == "Bottom") { m_dockControlLeft->setTabStripPosition(Position::Bottom); }

    if(rc->getSelectedLabel() == "Left") { m_dockControlLeft->setTabStripPosition(Position::Left); }

    if(rc->getSelectedLabel() == "Right") { m_dockControlLeft->setTabStripPosition(Position::Right); }
}

}//namespace gweni
