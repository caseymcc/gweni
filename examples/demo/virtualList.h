#ifndef _gweni_virtualList_example_h_
#define _gweni_virtualList_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Label;
class VirtualList;
}

class VirtualListDemo: public controls::Base
{
public:
    GWENI_CONTROL(VirtualListDemo, controls::Base);

public:
    //! The items, as many as the list is given.
    static constexpr size_t ItemCount=100000;

    //! A row was pressed: its item goes in m_pressed.
    void onItemPressed(event::Info info);
    //! The buttons: Item 50000 into view, the last item into view.
    void onMiddle(event::Info);
    void onEnd(event::Info);

    controls::VirtualList *m_list=nullptr;
    controls::Label *m_pressed=nullptr;
};

}//namespace gweni

#endif//_gweni_virtualList_example_h_
