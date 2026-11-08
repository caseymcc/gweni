#include "demo/sliderText.h"
#include "gweni/controls/horizontalSlider.h"
#include "gweni/controls/label.h"
#include "gweni/controls/verticalSlider.h"

namespace gweni
{

// Sliders showing their value (Slider::setValueFormat): horizontal ones with
// "%.0f", "%.2f" and a percentage, a notched one (its ticks below the field),
// a disabled one, and a vertical one with its value in a column on its right.
// Each row has a Label naming its format.
GWENI_CONTROL_CONSTRUCTOR(SliderTextDemo)
{
    setDock(DockPosition::Center);

    struct Row
    {
        const char *caption;
        const char *format;
        float min, max, value;
        int notches;
        bool disabled;
    };

    const Row rows[]=
    {
        {"%.0f", "%.0f", 0.0f, 100.0f, 25.0f, 0, false},
        {"%.2f", "%.2f", 0.0f, 1.0f, 0.42f, 0, false},
        {"%.0f %%", "%.0f %%", 0.0f, 100.0f, 60.0f, 0, false},
        {"Notched", "%.0f", 0.0f, 10.0f, 4.0f, 10, false},
        {"Disabled", "%.1f", 0.0f, 10.0f, 7.5f, 0, true},
    };

    int y=10;

    for(const Row &row:rows)
    {
        // a notched slider has room for its ticks below the field
        const int height=(row.notches > 0)?30:20;

        controls::Label *label=newChild<controls::Label>();
        label->setText(row.caption);
        label->setPos(10, y+(height-14)/2);

        controls::HorizontalSlider *slider=newChild<controls::HorizontalSlider>();
        slider->setPos(80, y);
        slider->setSize(200, height);
        slider->setRange(row.min, row.max);
        slider->setFloatValue(row.value);

        if(row.notches > 0)
        {
            slider->setNotchCount(row.notches);
            slider->setClampToNotches(true);
        }

        slider->setValueFormat(row.format);
        slider->setDisabled(row.disabled);
        y+=height+10;
    }

    controls::VerticalSlider *slider=newChild<controls::VerticalSlider>();
    slider->setPos(310, 10);
    slider->setSize(20, 180);
    slider->setRange(-50, 50);
    slider->setFloatValue(10);
    slider->setValueFormat("%+.1f");
}

}//namespace gweni
