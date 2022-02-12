/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_listbox_h_
#define _gweni_controls_listbox_h_

#include <gweni/gweni.h>
#include <gweni/controls/layout/table.h>
#include <gweni/controls/scrollControl.h>


namespace gweni
{
namespace controls
{

class GWENI_EXPORT ListBox: public ScrollControl
{
public:

    GWENI_CONTROL(ListBox, ScrollControl);

public:
    typedef std::list<layout::TableRow *> Rows;

    layout::TableRow *addItem(const String &label, const String &name="");

    void RemoveItem(layout::TableRow *row);

    //void render(skin::Base *skin) override;
    void layout(skin::Base *skin) override;

    void unselectAll();

    void setAllowMultiSelect(bool multiSelect)
    {
        m_multiSelect=multiSelect;
    }

    bool allowMultiSelect() const
    {
        return m_multiSelect;
    }

    const ListBox::Rows &getSelectedRows()
    {
        return m_selectedRows;
    }

    virtual layout::TableRow *getSelectedRow();
    virtual String getSelectedRowName();

    virtual void setSelectedRow(Base *row, bool clearOthers=true);
    virtual void selectByString(const String &string, bool clearOthers=true);

    event::Caller onRowSelectedCaller;

    layout::Table *getTable()
    {
        return m_table;
    }

    void clear() override;

    // Pass through, to embedded table
    void setColumnCount(int count)
    {
        m_table->setColumnCount(count);
    }

    void setColumnWidth(int count, int iSize)
    {
        m_table->setColumnWidth(count, iSize);
    }

protected:

    void onRowSelected(event::Info);
    bool onKeyDown(bool down) override;
    bool onKeyUp(bool down) override;

    layout::Table *m_table;
    ListBox::Rows m_selectedRows;

    bool m_multiSelect;
};


}//namespace controls
}//namespace gweni

#endif//_gweni_controls_listbox_h_
