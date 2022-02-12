/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/listBox.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/inputHandler.h>

namespace gweni
{
namespace controls
{

class ListBoxRow: public layout::TableRow
{
    GWENI_CONTROL_INLINE(ListBoxRow, layout::TableRow)
    {
        setMouseInputEnabled(true);
        setSelected(false);
    }

public:
    bool isSelected() const
    {
        return m_selected;
    }

    void DoSelect()
    {
        setSelected(true);
        onRowSelectedCaller.call(this);
        redraw();
    }

    void onMouseClickLeft(int /*x*/, int /*y*/, bool down) override
    {
        if(down)
            DoSelect();
    }

    void setSelected(bool b) override
    {
        m_selected=b;

        // TODO: Get these values from the skin.
        if(b)
            setTextColor(gweni::colors::White);
        else
            setTextColor(gweni::colors::Black);
    }

private:

    bool m_selected;

};


GWENI_CONTROL_CONSTRUCTOR(ListBox)
{
    setScroll(false, true);
    setAutoHideBars(true);
    setMargin(Margin(1, 1, 1, 1));
    m_innerPanel->setPadding(Padding(2, 2, 2, 2));
    m_table=newChild<controls::layout::Table>();
    m_table->setColumnCount(1);
    m_multiSelect=false;
}

layout::TableRow *ListBox::addItem(const String &label, const String &name)
{
    ListBoxRow *row=newChild<ListBoxRow>();
    m_table->addRow(row);
    row->setCellText(0, label);
    row->setName(name);
    row->onRowSelectedCaller.add(this, &ListBox::onRowSelected);
    return row;
}

void ListBox::RemoveItem(layout::TableRow *row)
{
    m_selectedRows.erase(std::find(m_selectedRows.begin(), m_selectedRows.end(), row));
    m_table->remove(row);
}

void ListBox::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
    const gweni::Rect &inner=m_innerPanel->getInnerBounds();
    m_table->setPos(inner.x, inner.y);
    m_table->setWidth(inner.w);
    m_table->sizeToChildren(false, true);
    ParentClass::layout(skin);
}

void ListBox::unselectAll()
{
    std::list<layout::TableRow *>::iterator it=m_selectedRows.begin();

    while(it != m_selectedRows.end())
    {
        ListBoxRow *row=static_cast<ListBoxRow *>(*it);
        it=m_selectedRows.erase(it);
        row->setSelected(false);
    }
}

void ListBox::onRowSelected(event::Info info)
{
    bool bClear=!gweni::input::isShiftDown();

    if(!allowMultiSelect())
        bClear=true;

    setSelectedRow(info.controlCaller, bClear);
}

layout::TableRow *ListBox::getSelectedRow()
{
    if(m_selectedRows.empty())
        return nullptr;

    return *m_selectedRows.begin();
}

gweni::String ListBox::getSelectedRowName()
{
    layout::TableRow *row=getSelectedRow();

    if(!row)
        return "";

    return row->getName();
}

void ListBox::clear()
{
    unselectAll();
    m_table->clear();
}

void ListBox::setSelectedRow(gweni::controls::Base *control, bool clearOthers)
{
    if(clearOthers)
        unselectAll();

    ListBoxRow *row=gweni_cast<ListBoxRow>(control);

    if(!row)
        return;

    // TODO: make sure this is one of our rows!
    row->setSelected(true);
    m_selectedRows.push_back(row);
    onRowSelectedCaller.call(this);
}

void ListBox::selectByString(const String &name, bool clearOthers)
{
    if(clearOthers)
        unselectAll();

    Base::List &children=m_table->getChildren();

    for(auto &&control : children)
    {
        ListBoxRow *child=gweni_cast<ListBoxRow>(control);

        if(!child)
            continue;

        if(wildcard(name, child->getText(0)))
            setSelectedRow(child, false);
    }
}

bool ListBox::onKeyDown(bool down)
{
    if(down)
    {
        Base::List &children=m_table->getChildren();
        Base::List::const_iterator begin=children.cbegin();
        Base::List::const_iterator end=children.cend();
        controls::Base *sel_row=getSelectedRow();

        if(sel_row == nullptr && !children.empty())   // no user selection yet, so
        {                                           // select first element
            sel_row=children.front();
        }

        Base::List::const_iterator result=std::find(begin, end, sel_row);

        if(result != end)
        {
            Base::List::const_iterator next=result;
            ++next;

            if(next != end)
                result=next;

            ListBoxRow *row=gweni_cast<ListBoxRow>(*result);

            if(row)
            {
                row->DoSelect();
                controls::VerticalScrollBar *scroll=gweni_cast<controls::VerticalScrollBar>(
                    m_verticalScrollBar);

                if(scroll)
                    scroll->nudgeDown(this);

                redraw();
            }
        }
    }

    return true;
}

bool ListBox::onKeyUp(bool down)
{
    if(down)
    {
        Base::List &children=m_table->getChildren();
        Base::List::const_iterator begin=children.cbegin();
        Base::List::const_iterator end=children.cend();
        controls::Base *sel_row=getSelectedRow();

        // no user selection yet, so select first element
        if(sel_row == nullptr && !children.empty())
            sel_row=children.front();

        Base::List::const_iterator result=std::find(begin, end, sel_row);

        if(result != end)
        {
            if(result != begin)
                --result;

            ListBoxRow *row=gweni_cast<ListBoxRow>(*result);

            if(row)
            {
                row->DoSelect();
                controls::VerticalScrollBar *scroll=gweni_cast<controls::VerticalScrollBar>(
                    m_verticalScrollBar);

                if(scroll)
                    scroll->nudgeUp(this);

                redraw();
            }
        }
    }

    return true;
}

}//namespace controls
}//namespace gweni
