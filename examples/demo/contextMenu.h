#ifndef _gweni_contextMenu_example_h_
#define _gweni_contextMenu_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Label;
}

class ContextMenuDemo: public controls::Base
{
public:
    GWENI_CONTROL(ContextMenuDemo, controls::Base);

public:
    //! A context menu item was chosen: its caption goes in m_chosen.
    void onChosen(event::Info info);

    controls::Label *m_chosen=nullptr;
};

}//namespace gweni

#endif//_gweni_contextMenu_example_h_
