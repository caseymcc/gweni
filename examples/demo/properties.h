#ifndef _gweni_properties_example_h_
#define _gweni_properties_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class PropertiesDemo : public controls::Base
{
public:
    GWENI_CONTROL(PropertiesDemo, controls::Base);

public:
    void onFirstNameChanged(event::Info info);
};

}//namespace gweni

#endif//_gweni_properties_example_h_
