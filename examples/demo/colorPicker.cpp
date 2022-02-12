#include "demo/colorPicker.h"
#include "gweni/controls/colorPicker.h"
#include "gweni/controls/hsvColorPicker.h"

namespace gweni
{

ColorPickerDemo::ColorPickerDemo(const gweni::String &name):
    Base()
{
    controls::ColorPicker *control=newChild<controls::ColorPicker>();

    control->setBounds(0, 10, 250, 150);

    controls::HSVColorPicker *hsvCtrl=newChild<controls::HSVColorPicker>();

    hsvCtrl->setBounds(260, 10, 250, 150);
}

}//namespace gweni
