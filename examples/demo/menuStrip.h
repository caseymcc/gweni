#ifndef _gweni_menuStrip_example_h_
#define _gweni_menuStrip_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class MenuStripDemo : public controls::Base
{
public:
    GWENI_CONTROL(MenuStripDemo, controls::Base);

public:
    void menuItemSelect(event::Info info);
};

}//namespace gweni

#endif//_gweni_menuStrip_example_h_
