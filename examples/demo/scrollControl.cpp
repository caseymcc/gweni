#include "demo/scrollControl.h"
#include "gweni/controls/scrollControl.h"

namespace gweni
{

ScrollControlDemo::ScrollControlDemo(controls::Base *parent, const String &name):
    Base(parent)
{
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(10, 10, 100, 100);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Twice As Big");
        testButton->setBounds(0, 0, 200, 200);
    }
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(110, 10, 100, 100);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Same Size");
        testButton->setBounds(0, 0, 100, 100);
    }
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(210, 10, 100, 100);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Wide");
        testButton->setBounds(0, 0, 200, 50);
    }
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(310, 10, 100, 100);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Tall");
        testButton->setBounds(0, 0, 50, 200);
    }
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(410, 10, 100, 100);
        control->setScroll(false, true);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Vertical");
        testButton->setBounds(0, 0, 200, 200);
    }
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(510, 10, 100, 100);
        control->setScroll(true, false);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Horinzontal");
        testButton->setBounds(0, 0, 200, 200);
    }
    // Bottom Row
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(10, 110, 100, 100);
        control->setAutoHideBars(true);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Twice As Big");
        testButton->setBounds(0, 0, 200, 200);
    }
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(110, 110, 100, 100);
        control->setAutoHideBars(true);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Same Size");
        testButton->setBounds(0, 0, 100, 100);
    }
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(210, 110, 100, 100);
        control->setAutoHideBars(true);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Wide");
        testButton->setBounds(0, 0, 200, 50);
    }
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(310, 110, 100, 100);
        control->setAutoHideBars(true);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Tall");
        testButton->setBounds(0, 0, 50, 200);
    }
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(410, 110, 100, 100);
        control->setAutoHideBars(true);
        control->setScroll(false, true);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Vertical");
        testButton->setBounds(0, 0, 200, 200);
    }
    {
        controls::ScrollControl *control=new controls::ScrollControl(this);
        control->setBounds(510, 110, 100, 100);
        control->setAutoHideBars(true);
        control->setScroll(true, false);
        controls::Button *testButton=new controls::Button(control);
        testButton->setText("Horinzontal");
        testButton->setBounds(0, 0, 200, 200);
    }
}

}//namespace gweni
