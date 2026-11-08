/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_layout_table_h_
#define _gweni_controls_layout_table_h_

#include <gweni/controls/button.h>
#include <gweni/utility.h>

#include <algorithm>
#include <vector>

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
            m_columnWidth[i]=-1;
        }

        m_columnCount=0;
    }

public:
    //! Layout only: a Table row's cells; ListBoxRow draws a row.
    GWENI_DRAWS_NOTHING

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
                    // The text reads from the top left of the padding, where
                    // sizeToContents() puts it, in a cell taller or wider
                    // than it too.
                    m_columns[i]->setTextAlignment(Alignment::Left|Alignment::Top);
                    m_columns[i]->sizeToContents();
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

    //! The width column i is laid out at. The Table sets it while it lays
    //! itself out, before this row is placed; arrangeHorizontal() places the
    //! column at it. -1 leaves the column at its own preferred width.
    //!
    //! A new width marks this row alone for the pass, which is placement: the
    //! Table that sets it arranges the row next, and nothing above the Table
    //! reads it.
    void setColumnWidth(int i, int iWidth)
    {
        if(m_columnWidth[i] == iWidth)
            return;

        m_columnWidth[i]=iWidth;
        markLayoutDirty();
        m_layoutDirtyCounted=true;
    }

    //! The width column i was given (setColumnWidth), -1 if none.
    int getColumnWidth(int i) const
    {
        return m_columnWidth[i];
    }

    //! What column i's content takes: its Label's preferred width, the text
    //! and the padding, or what setCellContents() put in it. Nothing writes
    //! that but the Label's own measure, so it is the content's whatever
    //! width the Table gave the column last.
    int getColumnContentWidth(int i) const
    {
        if(!m_columns[i])
            return 0;

        const Label *column=m_columns[i];
        return column->getPreferredSize().width+column->getMargin().left+
            column->getMargin().right;
    }

    void setCellText(int i, const String &strString)
    {
        if(!m_columns[i])
            return;

        // The Label measures the new text, and the Table, which the text's
        // invalidation reaches, sizes the column from it in the same pass.
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

protected:

    // The columns, its only children (setCellContents puts a cell's control
    // in its Label), are docked Left, each at the width the Table gave it.
    // Their preferred sizes are left as they measured, so the row's own
    // preferred width and what the Table reads next pass are the content's,
    // not the width a wider or narrower table handed out.
    void arrangeHorizontal(skin::Base * /*skin*/) override
    {
        int x=m_padding.left;
        int w=getWidth()-m_padding.left-m_padding.right;

        for(int i=0; i < m_columnCount && i < MaxColumns; i++)
        {
            Label *column=m_columns[i];

            if(!column || column->hidden())
                continue;

            const Margin &margin=column->getMargin();
            const Rect &bounds=column->getBounds();
            const int width=(m_columnWidth[i] >= 0)?m_columnWidth[i]:
                column->getPreferredSize().width;

            placeChild(column, x+margin.left, bounds.y, width, bounds.h);

            const int span=margin.left+width+margin.right;
            x+=span;
            w-=span;
        }

        m_innerBounds.x=x;
        m_innerBounds.w=w;
    }

private:

    bool m_evenRow;
    int m_columnCount;
    Label *m_columns[MaxColumns];
    int m_columnWidth[MaxColumns];

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

    //! Gives column i a fixed width (iWidth > 0), which it keeps whatever the
    //! table's width, or makes it an auto column again (0), sized from its
    //! content in layout().
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

    //! What Base::getControlAt() finds, without trying every row: while the
    //! rows are a stack (every child docked Top, the shown ones in child order
    //! by y and none overlapping the next), at most one shown row holds the
    //! point, and a binary search by y finds it, so a hit test costs O(log
    //! rows) here and one visit of that row. Otherwise (before the first
    //! layout, say, or with a row moved over another) it walks every child as
    //! Base does. The stack is checked again after any child is added,
    //! removed, moved, resized, hidden or shown, so the answer is the walk's
    //! for the bounds as they are, laid out or not.
    controls::Base *getControlAt(int x, int y, bool bOnlyIfMouseEnabled=true) override;

    //! Hands every row its columns' widths (splitColumns). It runs at the
    //! start of the table's X arrange, with the table's width final and every
    //! cell measured (calculateSize(X)), so a cell's new text, a row added,
    //! removed, hidden or shown re-sizes the columns in that same pass: each
    //! invalidates the table. The table itself is as big as its rows measure,
    //! which its parent gives it.
    void layout(skin::Base *skin) override
    {
        ParentClass::layout(skin);

        const int columns=std::min(m_columnCount, int(TableRow::MaxColumns));
        int content[TableRow::MaxColumns]={};

        // What each auto column's widest cell takes, from the cells'
        // preferred widths: calculateSize(X) has measured them this pass.
        for(auto &&child : getChildren())
        {
            TableRow *row=gweni_cast<TableRow>(child);

            if(!row || row->hidden())
                continue;

            for(int i=0; i < columns; i++)
            {
                if(m_columnWidth[i] <= 0)
                    content[i]=std::max(content[i], row->getColumnContentWidth(i));
            }
        }

        int width[TableRow::MaxColumns]={};

        splitColumns(columns, m_columnWidth, content,
            getWidth()-m_padding.left-m_padding.right, width);

        bool even=false;

        for(auto &&child : getChildren())
        {
            TableRow *row=gweni_cast<TableRow>(child);

            if(!row)
                continue;

            // Docked Top, a row is as high as its tallest column measures,
            // so it is not sized here.
            row->setEven(even);
            even=!even;

            for(int i=0; i < columns; i++)
                row->setColumnWidth(i, width[i]);
        }

        // No invalidateParent() here: the ListBox this sits in decides its
        // scrollbars from the rows' preferred sizes before laying this table
        // out, and asking it to lay out again from here re-dirtied every pass.
    }

protected:

    void onChildAdded(controls::Base *child) override
    {
        m_hitRowsStale=true;
        ParentClass::onChildAdded(child);
    }

    void onChildRemoved(controls::Base *child) override
    {
        m_hitRowsStale=true;
        ParentClass::onChildRemoved(child);
    }

    void onChildBoundsChanged(gweni::Rect oldChildBounds, Base *child) override
    {
        m_hitRowsStale=true;
        ParentClass::onChildBoundsChanged(oldChildBounds, child);
    }

    void onChildHiddenChanged(Base *child) override
    {
        m_hitRowsStale=true;
        ParentClass::onChildHiddenChanged(child);
    }

private:

    //! The widths of `columns` columns sharing `available` units. A column
    //! with a fixed width (fixed[i] > 0) keeps it. The auto ones (fixed[i] <=
    //! 0) share what is left, `free`, by their content widths, whose sum is
    //! C: each gets its content and an even share of free-C when that fits,
    //! floor(content*free/C) when it does not, and nothing when free <= 0.
    //! What the integer division leaves goes one unit each to the auto
    //! columns, lowest index first, so the columns add up to `available`
    //! exactly whenever there is an auto column and free >= 0.
    static void splitColumns(int columns, const int *fixed, const int *content,
        int available, int *width)
    {
        int free=available;
        int autoColumns=0;
        long long contentSum=0;

        for(int i=0; i < columns; i++)
        {
            if(fixed[i] > 0)
            {
                width[i]=fixed[i];
                free-=fixed[i];
            }
            else
            {
                width[i]=0;
                contentSum+=content[i];
                autoColumns++;
            }
        }

        if(autoColumns == 0 || free <= 0)
            return;

        int given=0;

        if(contentSum <= free)
        {
            const int share=(free-int(contentSum))/autoColumns;

            for(int i=0; i < columns; i++)
            {
                if(fixed[i] <= 0)
                {
                    width[i]=content[i]+share;
                    given+=width[i];
                }
            }
        }
        else
        {
            for(int i=0; i < columns; i++)
            {
                if(fixed[i] <= 0)
                {
                    width[i]=int(content[i]*(long long)free/contentSum);
                    given+=width[i];
                }
            }
        }

        // Less than one unit per auto column is left in either case.
        for(int i=0; i < columns && given < free; i++)
        {
            if(fixed[i] <= 0)
            {
                width[i]++;
                given++;
            }
        }
    }

    //! Rebuilds m_hitRows if a child was added, removed, moved, resized,
    //! hidden or shown since the last build; whether the search holds.
    bool hitRowsSearchable();

    int m_columnCount;
    int m_defaultRowHeight;

    int m_columnWidth[TableRow::MaxColumns];

    //! The shown rows that hold any point (not hidden, not empty), in child
    //! order, which is ascending y with no overlap while m_hitRowsSorted.
    std::vector<Base *> m_hitRows;
    bool m_hitRowsStale=true;
    bool m_hitRowsSorted=false;
};


}//namespace layout
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_layout_table_h_
