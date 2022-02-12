#ifndef _gweni_pageControl_example_h_
#define _gweni_pageControl_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class PageControlDemo : public controls::Base
{
public:
    GWENI_CONTROL(PageControlDemo, controls::Base);

public:
    void OnPageChanged(event::Info info);

    void OnFinish(event::Info info);
};

}//namespace gweni

#endif//_gweni_pageControl_example_h_
