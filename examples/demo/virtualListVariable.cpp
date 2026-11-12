#include "demo/virtualListVariable.h"
#include "gweni/controls/button.h"
#include "gweni/controls/label.h"
#include "gweni/controls/virtualList.h"

#include <string>

namespace gweni
{

namespace
{

const char *const Words[]={"wrapped", "rows", "of", "mixed", "length", "measured", "as", "they", "show",
    "each", "one", "its", "own", "height", "at", "the", "list's", "width", "a", "virtual", "list"};
constexpr size_t WordCount=sizeof(Words)/sizeof(Words[0]);

// A number's text: its label, then between 1 and 37 words picked from it, so
// the rows take one to four lines at the demo's width. Negative numbers (items
// inserted at the top) are texts like any other.
String numberText(long long number, bool grown)
{
    const unsigned long long n=static_cast<unsigned long long>(number < 0?-number*7919:number);
    const size_t words=1+(n*2654435761ull>>7)%((n%5 == 0)?37:9);
    String text="Item "+std::to_string(number)+":";

    for(size_t i=0; i < words; ++i)
        text+=" "+String(Words[(n*31+i*17)%WordCount]);
    if(grown)
        text+=" and now it has grown by a sentence or two, so that it takes more lines than it did";
    return text;
}

}//namespace

// A list of 1 000 items of wrapped text, each row as high as its text at the
// list's width: the measurer is the wrap the rows' Labels do (through
// Base::wrappedTextHeight) plus their padding, so the two never disagree.
// The list is docked Left, 320 wide, so it fills the demo's height (the
// canvas's in ControlTest) and a canvas resize changes only its height. Its
// re-measure budget is the item count, so every item is measured by the
// second frame and a capture (three frames) shows it settled; the
// virtual-list-variable scenario runs the default budget. The buttons grow
// the first item, insert one above it, make the list narrower or wider (a
// new width: every item measured again) and go to the end.
GWENI_CONTROL_CONSTRUCTOR(VirtualListVariableDemo)
{
    setDock(DockPosition::Center);

    controls::Label *title=newChild<controls::Label>();

    title->setText("Wrapped rows, each as high as its text");
    title->setPos(10, 10);

    m_list=newChild<controls::VirtualList>();
    m_list->setDock(DockPosition::Left);
    m_list->setWidth(320);
    m_list->setMargin(Margin(10, 34, 0, 10));
    m_list->setRowFactory([](controls::Base *parent)
        {
            auto *row=parent->newChild<controls::internal::VirtualListRow>();

            row->setWrap(true);
            row->setPadding(Padding(RowPadX, RowPadY, RowPadX, RowPadY));
            row->setTextPadding(Padding(0, 0, 0, 0));
            return row;
        });
    m_list->setRowBinder([this](controls::Base *row, size_t index)
        {
            if(controls::Label *label=gweni_cast<controls::Label>(row))
                label->setText(itemText(index));
        });
    m_list->setItemMeasurer([this](size_t index, int width)
        {
            return m_list->wrappedTextHeight(getSkin()->getDefaultFont(), itemText(index), width-2*RowPadX)
                +2*RowPadY;
        });
    m_list->setItemCount(ItemCount);
    m_list->setRemeasureBudget(ItemCount);

    struct
    {
        const char *text;
        void (VirtualListVariableDemo::*handler)(event::Info);
    } const buttons[]={
        {"Grow the first item", &VirtualListVariableDemo::onGrowFirst},
        {"Insert one at the top", &VirtualListVariableDemo::onInsertTop},
        {"Narrower / wider", &VirtualListVariableDemo::onWidth},
        {"Go to the end", &VirtualListVariableDemo::onEnd},
    };
    int y=34;

    for(const auto &b : buttons)
    {
        controls::Button *button=newChild<controls::Button>();

        button->setText(b.text);
        button->setBounds(350, y, 150, 20);
        button->onPressCaller.add(this, b.handler);
        y+=28;
    }
}

void VirtualListVariableDemo::setItems(size_t count)
{
    m_first=0;
    m_grown.clear();
    m_list->removeItems(0, m_list->getItemCount());
    m_list->setItemCount(count);
}

String VirtualListVariableDemo::itemText(size_t index) const
{
    const long long number=m_first+static_cast<long long>(index);

    return numberText(number, m_grown.count(number) != 0);
}

void VirtualListVariableDemo::insertAtTop(size_t count)
{
    m_first-=static_cast<long long>(count);
    m_list->insertItems(0, count);
}

void VirtualListVariableDemo::growItem(size_t index)
{
    if(m_grown.insert(m_first+static_cast<long long>(index)).second)
        m_list->itemChanged(index);
}

void VirtualListVariableDemo::onGrowFirst(event::Info)
{
    growItem(0);
}

void VirtualListVariableDemo::onInsertTop(event::Info)
{
    insertAtTop(1);
}

void VirtualListVariableDemo::onWidth(event::Info)
{
    m_narrow=!m_narrow;
    m_list->setWidth(m_narrow?220:320);
}

void VirtualListVariableDemo::onEnd(event::Info)
{
    m_list->scrollToItem(m_list->getItemCount()-1);
}

}//namespace gweni
