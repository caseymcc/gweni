#ifndef _gweni_numeric_example_h_
#define _gweni_numeric_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class NumericDemo : public controls::Base
{
public:
    GWENI_CONTROL(NumericDemo, controls::Base);

    void onButtonA(event::Info info);

};


}//namespace gweni

#endif//_gweni_numeric_example_h_
