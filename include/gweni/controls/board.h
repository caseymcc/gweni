/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_controls_board_h_
#define _gweni_controls_board_h_

#include <gweni/controls/scrollControl.h>

namespace gweni
{
namespace controls
{

//! Fixed-width BoardColumns side by side, scrolling only horizontally.
//! Columns fill the viewport's height regardless of their cards' height.
//! Make columns with newChild<BoardColumn>(); their order is their child order.
class GWENI_EXPORT Board: public ScrollControl
{
public:
    GWENI_CONTROL(Board, ScrollControl);

public:
    //! The column headers, cards and scroll bars draw.
    GWENI_DRAWS_NOTHING

    //! Width of every column, at least 1. Default 240.
    void setColumnWidth(int width);
    int getColumnWidth() const { return m_columnWidth; }
    //! Space between columns, at least 0. Default 12.
    void setGap(int gap);
    int getGap() const { return m_gap; }

protected:
    void addChild(Base *child) override;
    void removeChild(Base *child) override;
    Size contentSize() const override;
    void contentsPlaced(skin::Base *skin) override;

private:
    int m_columnWidth=240;
    int m_gap=12;
};

}
}

#endif
