#include "demo/colorPicker.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/colorPicker.h"
#include "gweni/controls/hsvColorPicker.h"

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(ColorPickerDemo)
{
    setLayout(new controls::VerticalLayout());

    controls::ColorPicker *control=newChild<controls::ColorPicker>();

    control->setBounds(0, 10, 250, 150);

    controls::HSVColorPicker *hsvCtrl=newChild<controls::HSVColorPicker>();

    hsvCtrl->setBounds(260, 10, 250, 150);
}

}//namespace gweni
