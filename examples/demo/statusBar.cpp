#include "demo/statusBar.h"
#include "gweni/controls/statusBar.h"
#include "gweni/controls/label.h"

namespace gweni
{

//StatusBarDemo::StatusBarDemo(const String &name):
//    Base()
GWENI_CONTROL_CONSTRUCTOR(StatusBarDemo)
{
    setDock(DockPosition::Center);
    controls::StatusBar *status=newChild<controls::StatusBar>();
    controls::Label *right=status->newChild<controls::Label>();
    right->setText("Label Added to Right");
    status->addControl(right, true);
}

void StatusBarDemo::layout(skin::Base *skin)
{
}

}//namespace gweni
