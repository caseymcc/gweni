/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_layout_flow_h_
#define _gweni_controls_layout_flow_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
namespace layout
{

//! Cards in as many columns as fit its own width (a container query, not the
//! canvas's): its children, row-major, each stretched to its column's width
//! and every card of a row to the row's height, the height of its tallest
//! card, with getGap() between the columns and between the rows. Its
//! preferred height is those rows and gaps at its width, so in a vertical
//! ScrollControl it scrolls as content does; there it needs a stable gutter
//! (setScroll(false, true) and setAutoHideBars(false)) to lay out in one
//! pass, since its height follows its width (docs/layout.md -> "layout::Flow").
//!
//! The column count for a content width C (its width less its padding):
//! from the breakpoints when there are any (setBreakpoints, compared with the
//! Flow's own width), else as many columns of getMinCardWidth() as fit,
//! floor((C+gap)/(minCardWidth+gap)), clamped to 1..getMaxColumns(). A card
//! is (C-(cols-1)*gap)/cols wide in whole units, the last column taking the
//! remainder (columnSpan()). Hidden children take no cell. Docked Top by
//! default, so a parent gives it its width and it asks for its height.
class GWENI_EXPORT Flow: public Base
{
public:

    GWENI_CONTROL(Flow, Base);

public:
    //! Layout only: it places its children.
    GWENI_DRAWS_NOTHING

    //! A Flow at least `below` units wide is laid out by a later breakpoint;
    //! one narrower, by the first such entry: `columns` columns.
    struct Breakpoint
    {
        int below;
        int columns;
    };

    //! Units between two columns and between two rows. Default 8.
    void setGap(int gap);
    int getGap() const { return m_gap; }

    //! The narrowest a column gets with no breakpoints, and the width the
    //! Flow asks for (one column). Default 200.
    void setMinCardWidth(int width);
    int getMinCardWidth() const { return m_minCardWidth; }

    //! The most columns with no breakpoints, and the count past the last
    //! breakpoint. Default 4.
    void setMaxColumns(int columns);
    int getMaxColumns() const { return m_maxColumns; }

    //! The column count by width: a Flow narrower than an entry's `below`
    //! takes the first such entry's `columns` (at least 1), and one at least
    //! as wide as every entry's, getMaxColumns(). Kept sorted by `below`. An
    //! empty table (the default) gives the min-card-width rule back.
    void setBreakpoints(std::vector<Breakpoint> breakpoints);
    const std::vector<Breakpoint> &getBreakpoints() const { return m_breakpoints; }

    //! The column count for a Flow `width` units wide whose content box (the
    //! width less its padding) is `contentWidth` units.
    int columnsFor(int width, int contentWidth) const;
    //! The column count the last layout pass laid the cards out in (0 before
    //! one).
    int getColumns() const { return m_columns; }

    //! Column `column` of `columns` across a content box `contentWidth`
    //! units wide with `gap` between columns: its x from the box's left and
    //! its width, (contentWidth-(columns-1)*gap)/columns, the last column
    //! taking the remainder.
    static void columnSpan(int contentWidth, int columns, int gap, int column, int &x, int &w);

    //! X: one column, getMinCardWidth() and the padding, whatever its width.
    //! Y: its rows at the column widths its X arrange gave the cards: each
    //! row its tallest card, a gap between rows. A Y measure before this
    //! pass's X arrange (a ScrollControl measuring its content before laying
    //! it out) enters no card and keeps the last height: the cards' widths
    //! are not this pass's yet, so their heights would be measured at a stale
    //! width, and the ScrollControl measures again once they are laid out.
    void calculateSize(skin::Base *skin, Dim dim) override;
    //! X: the column count from its width, and each card's x and width. Y:
    //! each row's y and every card's height its row's. Both before the cards
    //! are arranged, so a card placed anew is laid out in the same pass, and
    //! one whose bounds stay is not entered.
    void arrange(skin::Base *skin, Dim dim) override;

    void onChildAdded(Base *child) override;
    void onChildRemoved(Base *child) override;

private:
    //! getColumns(), or before any X arrange the count for the current width.
    int laidOutColumns() const;
    //! The row's height as the last Y measure gave it; `card`'s preferred
    //! height for a row it did not see.
    int rowHeight(size_t row, const Base *card) const;

    int m_gap=8;
    int m_minCardWidth=200;
    int m_maxColumns=4;
    std::vector<Breakpoint> m_breakpoints;

    //! The column count the X arrange laid the cards out in.
    int m_columns=0;
    //! Each row's height, as the last Y measure gave it.
    std::vector<int> m_rowHeights;
};

}//namespace layout
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_layout_flow_h_
