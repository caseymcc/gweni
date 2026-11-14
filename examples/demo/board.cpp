/*
 *  Gweni demo
 *  See license in gweni.h
 */
#include "demo/board.h"

#include <gweni/controls/board.h>
#include <gweni/controls/boardColumn.h>
#include <gweni/controls/label.h>
#include <gweni/controls/layout/flow.h>

namespace gweni
{
namespace
{

const char *const ColumnTitles[]={"Backlog", "Ready", "In progress", "Review", "Done"};
const int CardCounts[]={12, 2, 6, 3, 2};
const char *const CardTitles[]={
    "Add wrapped descriptions", "Keep column widths steady", "Check horizontal input",
    "Scroll within a column", "Keep the board position", "Grow a card in place",
    "Move sideways with Shift", "Leave space between columns", "Keep fitting columns steady",
    "Add another task", "Review the layout", "Finish the board"};
const char *const Descriptions[]={
    "A longer task description wraps to the width of its card. More text makes the card taller "
    "while the column stays the height of the board.",
    "Scroll this column to see its other tasks. Reaching either end keeps the wheel here, "
    "so the board stays where you left it.",
    "Use a horizontal wheel or hold Shift while wheeling to move across the board, even when "
    "the pointer is over a card.",
    "Adding a card or extending its description keeps every scroll position. Each column "
    "has room for its scrollbar before the first card is added."};

}

GWENI_CONTROL_CONSTRUCTOR(BoardDemoCard)
{
    setPadding(Padding(10, 8, 10, 10));

    m_title=newChild<controls::Label>();
    m_title->setDock(DockPosition::Top);
    m_title->setWrap(true);
    m_title->makeColorBright();
    m_title->setMargin(Margin(0, 0, 0, 4));

    m_body=newChild<controls::Label>();
    m_body->setDock(DockPosition::Top);
    m_body->setWrap(true);
}

void BoardDemoCard::setTitle(const String &title)
{
    m_title->setText(title);
}

void BoardDemoCard::setBody(const String &body)
{
    m_body->setText(body);
}

GWENI_CONTROL_CONSTRUCTOR(BoardDemo)
{
    setDock(DockPosition::Center);

    auto *hint=newChild<controls::Label>();
    hint->setDock(DockPosition::Top);
    hint->setWrap(true);
    hint->setText("Wheel in a column to scroll it. Use Shift+wheel to move across the board.");
    hint->setMargin(Margin(10, 8, 10, 6));

    m_board=newChild<controls::Board>("TaskBoard");
    m_board->setColumnWidth(240);
    m_board->setGap(14);
    m_cards.resize(ColumnCount);

    for(size_t columnIndex=0; columnIndex < ColumnCount; ++columnIndex)
    {
        auto *column=m_board->newChild<controls::BoardColumn>();
        column->setText(ColumnTitles[columnIndex]);
        column->getHeader()->makeColorBright();
        m_columns.push_back(column);

        auto *flow=column->newChild<controls::layout::Flow>();
        flow->setMaxColumns(1);
        flow->setGap(8);
        m_flows.push_back(flow);

        for(int cardIndex=0; cardIndex < CardCounts[columnIndex]; ++cardIndex)
        {
            const size_t index=columnIndex*3+static_cast<size_t>(cardIndex);
            const String title=CardTitles[index%12];
            String body=Descriptions[index%4];
            if(columnIndex == FittingColumn)
                body=cardIndex == 0
                    ?"This column has room to spare. Its wheel stays here even when every card fits."
                    :"Add more tasks without changing the width of the cards already here.";
            addCard(columnIndex, title, body);
        }
    }
}

BoardDemoCard *BoardDemo::addCard(size_t column, const String &title, const String &body)
{
    auto *card=m_flows.at(column)->newChild<BoardDemoCard>();
    card->setTitle(title);
    card->setBody(body);
    m_cards.at(column).push_back(card);
    return card;
}

}
