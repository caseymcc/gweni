#ifndef _gweni_selectable_example_h_
#define _gweni_selectable_example_h_

#include <gweni/controls/base.h>
#include <gweni/controls/selectable.h>

namespace gweni
{

class SelectableDemo: public controls::Base
{
public:
    GWENI_CONTROL(SelectableDemo, controls::Base);

    //! The single-selection rows in the GroupBox.
    controls::SelectableGroup m_group;
};

}//namespace gweni

#endif//_gweni_selectable_example_h_
