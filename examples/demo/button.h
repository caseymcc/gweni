#ifndef _gweni_button_example_h_
#define _gweni_button_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class ButtonDemo : public controls::Base
{
public:
    GWENI_CONTROL(ButtonDemo, controls::Base);

public:
    void onButtonA(event::Info info);
    void onToggle(event::Info info);
    void onToggleOn(event::Info info);
    void onToggleOff(event::Info info);
};

}//namespace gweni

#endif//_gweni_button_example_h_
