#ifndef _gweni_tabControl_example_h_
#define _gweni_tabControl_example_h_

#include <gweni/controls/base.h>
#include <gweni/controls/tabControl.h>

namespace gweni
{

class TabControlDemo : public controls::Base
{
public:
    GWENI_CONTROL(TabControlDemo, controls::Base);

    void onDockChange(event::Info info);

private:
    controls::TabControl *m_dockControlLeft;
    Font m_font;
};

}//namespace gweni

#endif//_gweni_tabControl_example_h_
