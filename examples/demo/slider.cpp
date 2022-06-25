#include "demo/slider.h"
#include "gweni/controls/radioButtonController.h"
#include "gweni/controls/verticalSlider.h"
#include "gweni/controls/horizontalSlider.h"

namespace gweni
{

SliderDemo::SliderDemo(const String &name):
    Base()
{
    {
        controls::HorizontalSlider *slider=newChild<controls::HorizontalSlider>();
        slider->setPos(10, 10);
        slider->setSize(150, 20);
        slider->setRange(0, 100);
        slider->setFloatValue(25);
        slider->onValueChanged.add(this, &SliderDemo::sliderMoved);
    }
    {
        controls::HorizontalSlider *slider=newChild<controls::HorizontalSlider>();
        slider->setPos(10, 40);
        slider->setSize(150, 20);
        slider->setRange(0, 100);
        slider->setFloatValue(20);
        slider->setNotchCount(10);
        slider->setClampToNotches(true);
        slider->onValueChanged.add(this, &SliderDemo::sliderMoved);
    }
    {
        controls::VerticalSlider *slider=newChild<controls::VerticalSlider>();
        slider->setPos(160, 10);
        slider->setSize(20, 200);
        slider->setRange(0, 100);
        slider->setFloatValue(25);
        slider->onValueChanged.add(this, &SliderDemo::sliderMoved);
    }
    {
        controls::VerticalSlider *slider=newChild<controls::VerticalSlider>();
        slider->setPos(190, 10);
        slider->setSize(20, 200);
        slider->setRange(0, 100);
        slider->setFloatValue(20);
        slider->setNotchCount(10);
        slider->setClampToNotches(true);
        slider->onValueChanged.add(this, &SliderDemo::sliderMoved);
    }
}

void SliderDemo::sliderMoved(event::Info info)
{
//    controls::Slider *slider=(controls::Slider *) control;
//    UnitPrint(Format(L"Slider Value: %.2f", (float)slider->getFloatValue()));
}

}//namespace gweni
