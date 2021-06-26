#include "demo/statusBar.h"
#include "gweni/controls/statusBar.h"
#include "gweni/controls/label.h"

namespace gweni
{

StatusBarDemo::StatusBarDemo(controls::Base *parent, const String &name):
    Base(parent)
{
    dock(Position::Fill);
    controls::StatusBar *status=new controls::StatusBar(this);
    controls::Label *right=new controls::Label(status);
    right->setText("Label Added to Right");
    status->addControl(right, true);
}

void StatusBarDemo::layout(skin::Base *skin)
{
}

}//namespace gweni
