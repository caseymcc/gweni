#ifndef _gweni_window_example_h_
#define _gweni_window_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class WindowDemo:public controls::Base
{
public:
    GWENI_CONTROL(WindowDemo, controls::Base);

public:
    void openWindow(event::Info info);

    void openModalWindow(event::Info info);

private:
    int m_windowCount;
};

}//namespace gweni

#endif//_gweni_window_example_h_
