#ifndef _gweni_slider_example_h_
#define _gweni_slider_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class SliderDemo : public controls::Base
{
public:
    GWENI_CONTROL(SliderDemo, controls::Base);

    void sliderMoved(event::Info info);
};

}//namespace gweni

#endif//_gweni_slider_example_h_
