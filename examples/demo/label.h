#ifndef _gweni_label_example_h_
#define _gweni_label_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class LabelDemo: public controls::Base
{
public:
    GWENI_CONTROL(LabelDemo, controls::Base);

public:
    Font m_font;
};

}//namespace gweni

#endif//_gweni_label_example_h_
