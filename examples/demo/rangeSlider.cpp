#include "demo/rangeSlider.h"
#include "gweni/controls/label.h"
#include "gweni/controls/rangeSlider.h"

namespace gweni
{

// Range sliders (controls::RangeSlider): a price band with each thumb's value
// as "%.0f" and a gap of 50, a stepped one on its notches, a joined "lo – hi"
// text, a disabled one, and a vertical one with its values in a column on its
// right. Each row has a Label naming it.
GWENI_CONTROL_CONSTRUCTOR(RangeSliderDemo)
{
    setDock(DockPosition::Center);

    auto row=[this](int y, int height, const char *caption) {
        controls::Label *label=newChild<controls::Label>();
        label->setText(caption);
        label->setPos(10, y+(height-14)/2);

        controls::RangeSlider *slider=newChild<controls::RangeSlider>();
        slider->setPos(80, y);
        slider->setSize(240, height);
        return slider;
    };

    price=row(10, 20, "Price");
    price->setRange(0, 1000);
    price->setMinimumGap(50);
    price->setValues(200, 800);
    price->setValueFormat("%.0f");

    // room for its ticks below the track
    stepped=row(40, 30, "Stepped");
    stepped->setRange(0, 100);
    stepped->setNotchCount(10);
    stepped->setClampToNotches(true);
    stepped->setValues(20, 60);

    joined=row(80, 20, "Hours");
    joined->setRange(0, 24);
    joined->setValues(9, 17);
    joined->setValueTextMode(controls::RangeSlider::ValueTextMode::Joined);
    joined->setValueFormat("%.0f");

    disabled=row(110, 20, "Disabled");
    disabled->setRange(0, 10);
    disabled->setValues(2.5, 7.5);
    disabled->setValueFormat("%.1f");
    disabled->setDisabled(true);

    vertical=newChild<controls::RangeSlider>();
    vertical->setHorizontal(false);
    vertical->setPos(350, 10);
    vertical->setSize(20, 180);
    vertical->setRange(-50, 50);
    vertical->setValues(-10, 30);
    vertical->setValueFormat("%+.0f");
}

}//namespace gweni
