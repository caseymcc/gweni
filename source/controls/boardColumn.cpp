/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#include <gweni/controls/boardColumn.h>

#include <algorithm>

using namespace gweni;
using namespace gweni::controls;

namespace
{

//! A vertical wheel belongs to this column, whether its bar can move or not.
//! The inherited horizontal hook passes through to the Board.
class BoardColumnBody: public ScrollControl
{
public:
    GWENI_CONTROL_INLINE(BoardColumnBody, ScrollControl)
    {
        setDock(DockPosition::Center);
        // A body fills its column; its cards do not ask the column to grow.
        setHeight(0);
        setScroll(false, true);
        setAutoHideBars(false);
    }

    GWENI_DRAWS_NOTHING

    bool onMouseWheeled(int delta) override
    {
        if(!m_verticalScrollBar->hidden() && !m_verticalScrollBar->isDisabled())
        {
            const float amount=m_verticalScrollBar->getScrolledAmount();
            const float moved=std::clamp(
                amount-m_verticalScrollBar->getNudgeAmount()*delta/60.f, 0.f, 1.f);
            if(moved != amount)
                m_verticalScrollBar->setScrolledAmount(moved, true);
        }
        return true;
    }
};

}

GWENI_CONTROL_CONSTRUCTOR(BoardColumn)
{
    m_header=newChild<Label>();
    m_header->setDock(DockPosition::Top);
    m_header->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    m_header->setPadding(Padding(10, 0, 10, 0));
    m_header->setMouseInputEnabled(true);
    m_header->setHeight(36);

    m_body=newChild<BoardColumnBody>();
    m_innerPanel=m_body;
}

void BoardColumn::setText(const String &text)
{
    m_header->setText(text);
}

const String &BoardColumn::getText() const
{
    return m_header->getText();
}

void BoardColumn::setHeaderHeight(int height)
{
    m_header->setHeight(std::max(0, height));
}
