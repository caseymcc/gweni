#ifndef _gweni_statusBar_example_h_
#define _gweni_statusBar_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class StatusBarDemo : public controls::Base
{
public:
    GWENI_CONTROL(StatusBarDemo, controls::Base);

public:
    void layout(skin::Base *skin);
};

}//namespace gweni

#endif//_gweni_statusBar_example_h_
