#ifndef _gweni_demo_layout_h_
#define _gweni_demo_layout_h_

#include <gweni/controls/windowControl.h>
#include <gweni/controls/label.h>
#include <gweni/controls/button.h>
#include <gweni/controls/groupBox.h>
#include <gweni/controls/radioButton.h>
#include <gweni/controls/layout/layout.h>

namespace gweni
{

class LayoutDemo:public controls::Base
{
public:
    GWENI_CONTROL(LayoutDemo, controls::Base);

    Font m_font;
};

}//namespace gweni

#endif//_gweni_demo_layout_h_