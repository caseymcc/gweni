#include "demo/colorPicker.h"
#include "gweni/controls/colorPicker.h"
#include "gweni/controls/hsvColorPicker.h"

namespace gweni
{

ColorPickerDemo::ColorPickerDemo(gweni::controls::Base *parent, const gweni::String &name):
    Base(parent)
{
    controls::ColorPicker *control=new controls::ColorPicker(this);

    control->setBounds(0, 10, 250, 150);

    controls::HSVColorPicker *hsvCtrl=new controls::HSVColorPicker(this);

    hsvCtrl->setBounds(260, 10, 250, 150);
}

}//namespace gweni
