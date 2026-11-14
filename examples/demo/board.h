/*
 *  Gweni demo
 *  See license in gweni.h
 */
#ifndef _gweni_board_example_h_
#define _gweni_board_example_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
class Board;
class BoardColumn;
class Label;
namespace layout { class Flow; }
}

//! A task card with a title and a wrapped description on the GroupBox surface.
class BoardDemoCard: public controls::Base
{
public:
    GWENI_CONTROL(BoardDemoCard, controls::Base);

public:
    void setTitle(const String &title);
    void setBody(const String &body);
    controls::Label *body() const { return m_body; }

private:
    controls::Label *m_title=nullptr;
    controls::Label *m_body=nullptr;
};

//! A horizontally scrolling task board. Backlog overflows, Ready fits, and
//! each column holds wrapped cards in a one-column Flow with a stable gutter.
class BoardDemo: public controls::Base
{
public:
    GWENI_CONTROL(BoardDemo, controls::Base);

public:
    GWENI_DRAWS_NOTHING

    static constexpr size_t ColumnCount=5;
    static constexpr size_t OverflowingColumn=0;
    static constexpr size_t FittingColumn=1;

    controls::Board *board() const { return m_board; }
    const std::vector<controls::BoardColumn *> &columns() const { return m_columns; }
    const std::vector<BoardDemoCard *> &cards(size_t column) const { return m_cards.at(column); }
    //! Adds a card in place; leaves the board's and every body's offsets alone.
    BoardDemoCard *addCard(size_t column, const String &title, const String &body);

private:
    controls::Board *m_board=nullptr;
    std::vector<controls::BoardColumn *> m_columns;
    std::vector<controls::layout::Flow *> m_flows;
    std::vector<std::vector<BoardDemoCard *>> m_cards;
};

}

#endif
