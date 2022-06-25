#include "demo/numeric.h"
#include "gweni/controls/numericUpDown.h"

namespace gweni
{

NumericDemo::NumericDemo(const String &name):
    Base()
{
    controls::NumericUpDown *control=newChild<controls::NumericUpDown>();

    control->setBounds(10, 10, 50, 20);
    control->setIntValue(50);
    control->setMax(1000);
    control->setMin(-1000);
    //	control->onPress.add( this, &ThisClass::onButtonA );
}

void NumericDemo::onButtonA(event::Info info)
{
    //	UnitPrint( L"Button Pressed (using 'OnPress' event)" );
}

}//namespace gweni
