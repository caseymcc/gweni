/*
 *  Gweni demo
 *  See license in gweni.h
 */
#include "demo/reorder.h"

#include <gweni/controls/checkBox.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>

#include <algorithm>
#include <string>

namespace gweni
{
namespace
{

const char *const ColumnTitles[]={"Todo: reorder, own cards only", "Running: takes any card",
    "Done: takes any card"};
const char *const TodoCards[]={"Pinned: release notes", "Fix the login redirect", "Write the migration",
    "Review the API change", "Update the screenshots", "Tidy the build flags"};
const char *const RunningCards[]={"Profile the scheduler", "Port the dashboard"};
const char *const DoneCards[]={"Ship 0.9", "Rotate the keys"};

}

GWENI_CONTROL_CONSTRUCTOR(ReorderDemoCard)
{
    setPadding(Padding(10, 7, 10, 7));
    setDock(DockPosition::Top);
    setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    setHeight(ReorderDemo::CardHeight);
    setMargin(Margin(0, 0, 0, ReorderDemo::CardGap));

    m_title=newChild<controls::Label>();
    m_title->setDock(DockPosition::Top);
    m_title->setMouseInputEnabled(false);
}

void ReorderDemoCard::setTitle(const String &title)
{
    m_title->setText(title);
}

String ReorderDemoCard::title() const
{
    return m_title->getText();
}

GWENI_CONTROL_CONSTRUCTOR(ReorderDemo)
{
    setDock(DockPosition::Center);

    auto *top=newChild<controls::Base>();
    top->setDock(DockPosition::Top);
    top->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    top->setHeight(28);
    top->setMargin(Margin(10, 8, 10, 4));

    m_liveToggle=top->newChild<controls::CheckBoxWithLabel>("live");
    m_liveToggle->label()->setText("Live updates");
    m_liveToggle->setDock(DockPosition::Left);
    m_liveToggle->checkBox()->onCheckChanged.add(this, &ReorderDemo::onLiveChanged);

    auto *hint=top->newChild<controls::Label>();
    hint->setDock(DockPosition::Center);
    hint->setMargin(Margin(16, 2, 0, 0));
    hint->setText("Drag a card to reorder it or move it to another column. Escape cancels.");

    auto *board=newChild<controls::Base>();
    board->setDock(DockPosition::Center);
    board->setMargin(Margin(10, 0, 10, 10));

    for(size_t index=0; index < ColumnCount; ++index)
    {
        auto *columnBox=board->newChild<controls::Base>();
        columnBox->setDock(DockPosition::Left);
        columnBox->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
        columnBox->setWidth(220);
        columnBox->setMargin(Margin(0, 0, 12, 0));

        auto *header=columnBox->newChild<controls::Label>();
        header->setDock(DockPosition::Top);
        header->setText(ColumnTitles[index]);
        header->makeColorBright();
        header->setMargin(Margin(2, 0, 0, 6));

        auto *column=columnBox->newChild<controls::ScrollControl>();
        column->setDock(DockPosition::Center);
        column->setScroll(false, true);
        column->setAutoHideBars(false);
        m_columns.push_back(column);

        controls::CardReorder *reorder=controls::CardReorder::attach(column);
        reorder->setOnItemDropped([this](const controls::ItemDrop &drop) { applyDrop(drop); });
        m_group.add(reorder);
        m_reorders.push_back(reorder);
    }

    // Todo is ordered by hand and keeps its own cards; nothing goes above the
    // pinned card. Running and Done take any column's cards, in no order.
    m_reorders[Todo]->setAcceptsFromGroup(false);
    m_reorders[Todo]->setCanDrop([](controls::Reorderable *, uint64_t, controls::Reorderable *,
        size_t index) { return index > 0; });
    m_reorders[Running]->setReorderWithin(false);
    m_reorders[Done]->setReorderWithin(false);

    for(const char *title : TodoCards)
        addCard(Todo, title);
    for(const char *title : RunningCards)
        addCard(Running, title);
    for(const char *title : DoneCards)
        addCard(Done, title);
    m_pinned=cards(Todo).front();
}

std::vector<ReorderDemoCard *> ReorderDemo::cards(size_t column) const
{
    std::vector<ReorderDemoCard *> result;

    for(controls::Base *card : m_reorders.at(column)->cards())
    {
        if(ReorderDemoCard *demoCard=gweni_cast<ReorderDemoCard>(card))
            result.push_back(demoCard);
    }
    return result;
}

ReorderDemoCard *ReorderDemo::addCard(size_t column, const String &title, size_t index)
{
    const std::vector<ReorderDemoCard *> before=cards(column);
    auto *card=m_columns.at(column)->newChild<ReorderDemoCard>();

    card->setTitle(title);
    if(index < before.size())
        card->bringNextToControl(before[index], false);
    return card;
}

// The drop as the app applies it: the card moved to the target column, at the
// target index among the cards there once it has left its own.
void ReorderDemo::applyDrop(const controls::ItemDrop &drop)
{
    m_drops.push_back(drop);

    controls::Base *card=controls::CardReorder::cardFor(drop.key);
    auto target=std::find(m_reorders.begin(), m_reorders.end(), drop.target);

    if(!card || (target == m_reorders.end()))
        return;

    const size_t column=static_cast<size_t>(target-m_reorders.begin());
    std::vector<ReorderDemoCard *> others=cards(column);

    others.erase(std::remove(others.begin(), others.end(), card), others.end());

    if(card->getParent() != m_columns[column])
        card->setParent(m_columns[column]);

    if(drop.targetIndex < others.size())
        card->bringNextToControl(others[drop.targetIndex], false);
    else if(!others.empty())
        card->bringNextToControl(others.back(), true);
}

void ReorderDemo::onLiveChanged(event::Info)
{
    setLiveUpdates(m_liveToggle->checkBox()->isChecked());
}

void ReorderDemo::setLiveUpdates(bool live)
{
    controls::Canvas *canvas=getCanvas();

    if(m_liveTimer && canvas)
        canvas->cancel(m_liveTimer);
    m_liveTimer=controls::TimerHandle();

    if(live && canvas)
        m_liveTimer=canvas->every(LiveSeconds, [this]() { liveTick(); }, this);
}

void ReorderDemo::liveTick()
{
    ++m_liveCount;
    ReorderDemoCard *card=addCard(Todo, "Live update "+std::to_string(m_liveCount), 1);
    m_liveCards.push_back(card->getControlId());

    while(m_liveCards.size() > LiveCap)
    {
        delete controls::Base::getControlById(m_liveCards.front());
        m_liveCards.erase(m_liveCards.begin());
    }
}

}
