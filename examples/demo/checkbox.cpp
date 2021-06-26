#include "demo/checkBox.h"
#include "gweni/controls/checkBox.h"

namespace gweni
{

CheckBoxDemo::CheckBoxDemo(gweni::controls::Base *parent, const gweni::String &name):
    Base(parent)
{
    controls::CheckBox *check=new controls::CheckBox(this);
    check->setPos(10, 10);
    check->onChecked.add(this, &CheckBoxDemo::onChecked);
    check->onUnChecked.add(this, &CheckBoxDemo::onUnchecked);
    check->onCheckChanged.add(this, &CheckBoxDemo::onCheckChanged);
    controls::CheckBoxWithLabel *labeled=new controls::CheckBoxWithLabel(this);
    labeled->setPos(10, 30);
    labeled->label()->setText("Labeled CheckBox");
    labeled->checkBox()->onChecked.add(this, &CheckBoxDemo::onChecked);
    labeled->checkBox()->onUnChecked.add(this, &CheckBoxDemo::onUnchecked);
    labeled->checkBox()->onCheckChanged.add(this, &CheckBoxDemo::onCheckChanged);
    {
        controls::CheckBox *check=new controls::CheckBox(this);
        check->setPos(10, 54);
        check->setDisabled(true);
    }
}

void CheckBoxDemo::onChecked(event::Info info)
{

}

void CheckBoxDemo::onUnchecked(event::Info info)
{

}

void CheckBoxDemo::onCheckChanged(event::Info info)
{

}

}//namespace gweni
