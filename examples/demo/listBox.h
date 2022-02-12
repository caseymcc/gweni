#ifndef _gweni_listBox_example_h_
#define _gweni_listBox_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class ListBoxDemo : public controls::Base
{
public:
    GWENI_CONTROL(ListBoxDemo, controls::Base);

public:
    void ListBoxDemo::rowSelected(event::Info info);

private:
    Font m_font;
};

}//namespace gweni

#endif//_gweni_listBox_example_h_
