#ifndef _gweni_checkbox_example_h_
#define _gweni_checkbox_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class CheckBoxDemo: public controls::Base
{
public:
    GWENI_CONTROL(CheckBoxDemo, controls::Base);

    void onChecked(event::Info info);
    void onUnchecked(event::Info info);
    void onCheckChanged(event::Info info);
};

}//namespace gweni

#endif//_gweni_checkbox_example_h_
