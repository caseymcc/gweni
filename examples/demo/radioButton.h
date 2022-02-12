#ifndef _gweni_radioButton_example_h_
#define _gweni_radioButton_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class RadioButtonDemo : public controls::Base
{
public:
    GWENI_CONTROL(RadioButtonDemo, controls::Base);

public:
    void onChange(event::Info info);
};

}//namespace gweni

#endif//_gweni_radioButton_example_h_
