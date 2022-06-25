#ifndef _gweni_labelMultiline_example_h_
#define _gweni_labelMultiline_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class LabelMultilineDemo : public controls::Base
{
public:
    GWENI_CONTROL(LabelMultilineDemo, controls::Base);

public:
    Font m_font;
};

}//namespace gweni

#endif//_gweni_labelMultiline_example_h_
