#include "demo/virtualList.h"
#include "gweni/controls/button.h"
#include "gweni/controls/label.h"
#include "gweni/controls/virtualList.h"

#include <string>

namespace gweni
{

// A list of 100 000 items "Item 0" to "Item 99999", of which only the rows on
// screen exist, two buttons that scroll it to the middle and to the end, and a
// label naming the last item pressed.
GWENI_CONTROL_CONSTRUCTOR(VirtualListDemo)
{
    setDock(DockPosition::Center);

    controls::Label *title=newChild<controls::Label>();

    title->setText(std::to_string(ItemCount)+" items, a row for each one on screen");
    title->setPos(10, 10);

    m_list=newChild<controls::VirtualList>();
    m_list->setBounds(10, 34, 260, 400);
    m_list->setRowBinder([](controls::Base *row, size_t index)
        {
            if(controls::Label *label=gweni_cast<controls::Label>(row))
                label->setText("Item "+std::to_string(index));
        });
    m_list->setItemCount(ItemCount);
    m_list->onItemPressed.add(this, &VirtualListDemo::onItemPressed);

    controls::Button *middle=newChild<controls::Button>();

    middle->setText("Go to Item 50000");
    middle->setBounds(290, 34, 140, 20);
    middle->onPressCaller.add(this, &VirtualListDemo::onMiddle);

    controls::Button *end=newChild<controls::Button>();

    end->setText("Go to the end");
    end->setBounds(290, 62, 140, 20);
    end->onPressCaller.add(this, &VirtualListDemo::onEnd);

    m_pressed=newChild<controls::Label>();
    m_pressed->setText("Pressed: none");
    m_pressed->setPos(290, 94);
}

void VirtualListDemo::onMiddle(event::Info)
{
    m_list->scrollToItem(ItemCount/2);
}

void VirtualListDemo::onEnd(event::Info)
{
    m_list->scrollToItem(ItemCount-1);
}

void VirtualListDemo::onItemPressed(event::Info info)
{
    m_pressed->setText("Pressed: Item "+std::to_string(info.integer));
}

}//namespace gweni
