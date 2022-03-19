/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_layout_table_h_
#define _gweni_controls_layout_table_h_

#include <gweni/controls/button.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{
namespace layout
{

class Table;

class GWENI_EXPORT TableRow: public Base
{
    static const int MaxColumns=16;

    GWENI_CONTROL_INLINE(TableRow, Base)
    {
        setEven(false);

        for(int i=0; i < MaxColumns; i++)
        {
            m_columns[i]=nullptr;
        }

        m_columnCount=0;
    }

public:
    void setColumnCount(int count)
    {
        if(count == m_columnCount)
            return;

        if(count >= MaxColumns)
            m_columnCount=MaxColumns;

        for(int i=0; i < MaxColumns; i++)
        {
            if(i < count)
            {
                if(!m_columns[i])
                {
                    m_columns[i]=newChild<Label>();
                    m_columns[i]->setDock(DockPosition::Left);
                    m_columns[i]->setPadding(Padding(3, 3, 3, 3));
                }
            }
            else if(m_columns[i])
            {
                m_columns[i]->delayedDelete();
                m_columns[i]=nullptr;
            }

            m_columnCount=count;
        }
    }

    void setColumnWidth(int i, int iWidth)
    {
        if(!m_columns[i])
            return;

        if(m_columns[i]->getWidth() == iWidth)
            return;

        m_columns[i]->setWidth(iWidth);
    }

    void setCellText(int i, const String &strString)
    {
        if(!m_columns[i])
            return;

        m_columns[i]->setText(strString);
    }

    void setCellContents(int i, Base *control, bool bEnableMouseInput=false)
    {
        if(!m_columns[i])
            return;

        control->setParent(m_columns[i]);
        m_columns[i]->setMouseInputEnabled(bEnableMouseInput);
    }

    Label *getCellContents(int i)
    {
        return m_columns[i];
    }

    void sizeToContents()
    {
        int iHeight=0;

        for(int i=0; i < m_columnCount; i++)
        {
            if(!m_columns[i])
                continue;

            // Note, more than 1 child here, because the
            // label has a child built in ( The Text )
            if(m_columns[i]->numChildren() > 1)
                m_columns[i]->sizeToChildren();
            else
                m_columns[i]->sizeToContents();

            iHeight=std::max(iHeight, m_columns[i]->getHeight());
        }

        setHeight(iHeight);
    }

    void setTextColor(const gweni::Color &color)
    {
        for(int i=0; i < m_columnCount; i++)
        {
            if(!m_columns[i])
                continue;

            m_columns[i]->setTextColor(color);
        }
    }

    // You might hate this. Actually I know you will
    virtual const String &getText(int i)
    {
        return m_columns[i]->getText();
    }

    virtual void setSelected(bool /*b*/)
    {}

    //
    // This is sometimes called by derivatives.
    //
    gweni::event::Caller onRowSelectedCaller;

    virtual bool isEven()
    {
        return m_evenRow;
    }

    virtual void setEven(bool b)
    {
        m_evenRow=b;
    }

private:

    bool m_evenRow;
    int m_columnCount;
    Label *m_columns[MaxColumns];

    friend class Table;


};


class GWENI_EXPORT Table: public Base
{
public:

    GWENI_CONTROL_INLINE(Table, Base)
    {
        m_columnCount=1;
        m_defaultRowHeight=22;

        for(int i=0; i < TableRow::MaxColumns; i++)
        {
            m_columnWidth[i]=0;
        }

        m_sizeToContents=false;
    }

public:
    void setColumnCount(int i)
    {
        if(m_columnCount == i)
            return;

        for(auto &&child : getChildren())
        {
            TableRow *row=gweni_cast<TableRow>(child);

            if(!row)
                continue;

            row->setColumnCount(i);
        }

        m_columnCount=i;
    }

    void setColumnWidth(int i, int iWidth)
    {
        if(m_columnWidth[i] == iWidth)
            return;

        m_columnWidth[i]=iWidth;
        invalidate();
    }

    TableRow *addRow()
    {
        TableRow *row=newChild<TableRow>();
        addRow(row);
        return row;
    }

    void addRow(TableRow *row)
    {
        row->setParent(this);
        row->setColumnCount(m_columnCount);
        row->setHeight(m_defaultRowHeight);
        row->setDock(DockPosition::Top);
        invalidate();
    }

    TableRow *getRow(int i)
    {
        return gweni_cast<TableRow>(getChild(i));
    }

    unsigned int rowCount(int i)
    {
        return numChildren();
    }

    void remove(TableRow *row)
    {
        row->delayedDelete();
    }

    void clear()
    {
        for(auto &&child : getChildren())
        {
            TableRow *row=gweni_cast<TableRow>(child);

            if(!row)
                continue;

            remove(row);
        }
    }

    void layout(skin::Base *skin) override
    {
        ParentClass::layout(skin);

        if(m_sizeToContents)
            doSizeToContents();

        int sizeRemainder=getWidth();
        int autoSizeColumns=0;

        for(int i=0; i < TableRow::MaxColumns && i < m_columnCount; i++)
        {
            sizeRemainder-=m_columnWidth[i];

            if(m_columnWidth[i] == 0)
                autoSizeColumns++;
        }

        if(autoSizeColumns > 1)
            sizeRemainder/=autoSizeColumns;

        bool even=false;

        for(auto &&child : getChildren())
        {
            TableRow *row=gweni_cast<TableRow>(child);

            if(!row)
                continue;

            row->sizeToContents();
            row->setEven(even);
            even=!even;

            for(int i=0; i < TableRow::MaxColumns && i < m_columnCount; i++)
            {
                if(m_columnWidth[i] == 0)
                    row->setColumnWidth(i, sizeRemainder);
                else
                    row->setColumnWidth(i, m_columnWidth[i]);
            }
        }

        invalidateParent();
    }

    void postLayout(skin::Base * /*skin*/) override
    {
        if(m_sizeToContents)
        {
            sizeToChildren();
            m_sizeToContents=false;
        }
    }

    void sizeToContents()
    {
        m_sizeToContents=true;
        invalidate();
    }

    void doSizeToContents()
    {
        for(int i=0; i < TableRow::MaxColumns; i++)
        {
            m_columnWidth[i]=10;
        }

        for(auto &&child : getChildren())
        {
            TableRow *row=gweni_cast<TableRow>(child);

            if(!row)
                continue;

            row->sizeToContents();

            for(int i=0; i < TableRow::MaxColumns; i++)
            {
                if(row->m_columns[i])
                {
                    m_columnWidth[i]=std::max(m_columnWidth[i],
                        row->m_columns[i]->getWidth());
                }
            }

            // iBottom += row->getHeight();
        }

        invalidateParent();
    }

private:

    bool m_sizeToContents;
    int m_columnCount;
    int m_defaultRowHeight;

    int m_columnWidth[TableRow::MaxColumns];
};


}//namespace layout
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_layout_table_h_
