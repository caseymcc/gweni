#ifndef _gweni_comboBox_example_h_
#define _gweni_comboBox_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

class ComboBoxDemo : public controls::Base
{
public:
    GWENI_CONTROL(ComboBoxDemo, controls::Base);

public:
    void onComboSelect(event::Info info);
};

}//namespace gweni

#endif//_gweni_comboBox_example_h_
