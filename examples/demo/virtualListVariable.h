#ifndef _gweni_virtualListVariable_example_h_
#define _gweni_virtualListVariable_example_h_

#include <gweni/controls/base.h>

#include <set>

namespace gweni
{
namespace controls
{
class Label;
class VirtualList;
}

class VirtualListVariableDemo: public controls::Base
{
public:
    GWENI_CONTROL(VirtualListVariableDemo, controls::Base);

public:
    //! The items the demo starts with.
    static constexpr size_t ItemCount=1000;
    //! The rows' padding, which the measurer adds to the wrapped text's height.
    static constexpr int RowPadX=6;
    static constexpr int RowPadY=3;

    //! The list holds count items again, the first one "Item 0".
    void setItems(size_t count);
    //! The text of the item at index, as the binder and the measurer read it.
    String itemText(size_t index) const;
    //! Adds count items above the first one (numbered below it, from -1).
    void insertAtTop(size_t count);
    //! Gives the item at index a longer text (once: a second call does
    //! nothing), so it wraps onto more lines.
    void growItem(size_t index);

    //! The buttons.
    void onGrowFirst(event::Info);
    void onInsertTop(event::Info);
    void onWidth(event::Info);
    void onEnd(event::Info);

    controls::VirtualList *m_list=nullptr;

private:
    //! Item index's number: the first item's is m_first.
    long long m_first=0;
    std::set<long long> m_grown;
    bool m_narrow=false;
};

}//namespace gweni

#endif//_gweni_virtualListVariable_example_h_
