#ifndef _gweni_rangeslider_example_h_
#define _gweni_rangeslider_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class RangeSlider;
}

class RangeSliderDemo: public controls::Base
{
public:
    GWENI_CONTROL(RangeSliderDemo, controls::Base);

public:

    //! The rows, for the range-slider scenario.
    controls::RangeSlider *price=nullptr;
    controls::RangeSlider *stepped=nullptr;
    controls::RangeSlider *joined=nullptr;
    controls::RangeSlider *disabled=nullptr;
    controls::RangeSlider *vertical=nullptr;
};

}//namespace gweni

#endif//_gweni_rangeslider_example_h_
