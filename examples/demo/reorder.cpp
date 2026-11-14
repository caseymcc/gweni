/*
 *  Gweni demo
 *  See license in gweni.h
 */
#include "demo/reorder.h"

#include <gweni/controls/checkBox.h>
#include <gweni/controls/dataTable.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/virtualList.h>

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

    m_pages=newChild<controls::TabControl>();
    m_pages->setDock(DockPosition::Center);
    m_pages->setMargin(Margin(10, 0, 10, 10));

    m_boardTab=m_pages->addPage("Board");
    auto *board=m_boardTab->getPage();
    board->setPadding(Padding(8, 8, 8, 8));

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
    // and the pinned card itself stays where it is
    m_reorders[Todo]->setCanDrag([pinned=controls::CardReorder::keyOf(m_pinned)](uint64_t key) {
        return key != pinned;
    });

    // the list page: 10 000 tasks, each keyed by its id
    m_listsTab=m_pages->addPage("Lists");
    auto *lists=m_listsTab->getPage();
    lists->setPadding(Padding(8, 8, 8, 8));

    auto *listBox=lists->newChild<controls::Base>();
    listBox->setDock(DockPosition::Left);
    listBox->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
    listBox->setWidth(260);
    listBox->setMargin(Margin(0, 0, 12, 0));

    auto *listHeader=listBox->newChild<controls::Label>();
    listHeader->setDock(DockPosition::Top);
    listHeader->setText(std::to_string(ListItems)+" tasks, reordered by drag and drop");
    listHeader->makeColorBright();
    listHeader->setMargin(Margin(2, 0, 0, 6));

    for(size_t i=0; i < ListItems; ++i)
        m_listKeys.push_back(m_nextTask++);

    m_list=listBox->newChild<controls::VirtualList>();
    m_list->setDock(DockPosition::Center);
    m_list->setRowBinder([this](controls::Base *row, size_t index)
        {
            if(controls::Label *label=gweni_cast<controls::Label>(row))
                label->setText(taskText(m_listKeys[index]));
        });
    m_list->setItemCount(m_listKeys.size());

    // the app's key functions: called as a drag starts, as it drops and when
    // the list changes during it, never on a move or a scroll frame
    m_listReorder=controls::VirtualListReorder::attach(m_list,
        [this](size_t index) { return m_listKeys[index]; },
        [this](uint64_t key) {
            auto found=std::find(m_listKeys.begin(), m_listKeys.end(), key);
            return (found == m_listKeys.end()) ? controls::VirtualList::NoItem
                : static_cast<size_t>(found-m_listKeys.begin());
        });
    m_listReorder->setOnItemDropped([this](const controls::ItemDrop &drop) { applyListDrop(drop); });

    // the jobs: a source-mode table keyed by job id
    for(uint64_t job=0; job < TableJobs; ++job)
        m_tableOrder.push_back(JobKeyBase+job);
    m_tableShown=m_tableOrder;

    m_table=lists->newChild<controls::DataTable>();
    m_table->setDock(DockPosition::Center);
    m_table->setColumnCount(2);
    m_table->setColumnCaption(0, "Job");
    m_table->setColumnCaption(1, "Priority");
    m_table->setColumnWidth(0, 160);
    m_table->setStriped(true);
    m_table->setItemCount(m_tableShown.size());
    m_table->setRowBinder([this](controls::DataTableRow *row, size_t item)
        {
            const uint64_t key=m_tableShown[item];
            row->setCellText(0, taskText(key));
            row->setCellText(1, std::to_string(jobPriority(key)));
        });
    m_table->onSortChanged.add(this, &ReorderDemo::onTableSorted);

    controls::Reorderable *tableReorder=m_table->setItemsReorderable(
        [this](size_t index) { return m_tableShown[index]; },
        [this](uint64_t key) {
            auto found=std::find(m_tableShown.begin(), m_tableShown.end(), key);
            return (found == m_tableShown.end()) ? controls::DataTable::NoItem
                : static_cast<size_t>(found-m_tableShown.begin());
        });
    tableReorder->setAcceptsFromGroup(false);
    tableReorder->setOnItemDropped([this](const controls::ItemDrop &drop) { applyTableDrop(drop); });

    // a job dragged out of the table goes into the task list
    m_listGroup.add(m_listReorder);
    m_listGroup.add(tableReorder);
}

int ReorderDemo::jobPriority(uint64_t key)
{
    return static_cast<int>(((key-JobKeyBase)*7919u)%100u);
}

void ReorderDemo::showTableOrder()
{
    m_tableShown=m_tableOrder;

    const int column=m_table->getSortColumn();

    if(column >= 0)
    {
        const bool ascending=(m_table->getSortDirection() == controls::SortDirection::Ascending);

        std::stable_sort(m_tableShown.begin(), m_tableShown.end(), [column, ascending](uint64_t a, uint64_t b) {
            const int64_t x=(column == 0) ? static_cast<int64_t>(a) : jobPriority(a);
            const int64_t y=(column == 0) ? static_cast<int64_t>(b) : jobPriority(b);
            return ascending ? (x < y) : (x > y);
        });
    }
    m_table->rebind();
}

void ReorderDemo::onTableSorted(event::Info)
{
    showTableOrder();
}

// Within the table (only while unsorted): the job moved in the hand order.
void ReorderDemo::applyTableDrop(const controls::ItemDrop &drop)
{
    m_drops.push_back(drop);

    auto found=std::find(m_tableOrder.begin(), m_tableOrder.end(), drop.key);

    if(found == m_tableOrder.end())
        return;

    m_tableOrder.erase(found);
    m_tableOrder.insert(m_tableOrder.begin()+static_cast<std::ptrdiff_t>(std::min(drop.targetIndex,
        m_tableOrder.size())), drop.key);
    showTableOrder();
}

String ReorderDemo::taskText(uint64_t key)
{
    if(key >= JobKeyBase)
        return "Job #"+std::to_string(key-JobKeyBase);
    return "Task #"+std::to_string(key);
}

void ReorderDemo::showBoard()
{
    m_boardTab->doAction();
}

void ReorderDemo::showLists()
{
    m_listsTab->doAction();
}

// Within the list: the task taken out at its index and put at the target
// index, and the rows on screen bound again. From the table: the job taken
// out of the table's orders and put in the list.
void ReorderDemo::applyListDrop(const controls::ItemDrop &drop)
{
    m_drops.push_back(drop);

    if(drop.source != m_listReorder)
    {
        auto found=std::find(m_tableOrder.begin(), m_tableOrder.end(), drop.key);

        if(found == m_tableOrder.end())
            return;

        m_tableOrder.erase(found);
        m_tableShown.erase(std::find(m_tableShown.begin(), m_tableShown.end(), drop.key));
        m_table->setItemCount(m_tableShown.size());
        m_table->rebind();

        m_listKeys.insert(m_listKeys.begin()+static_cast<std::ptrdiff_t>(std::min(drop.targetIndex,
            m_listKeys.size())), drop.key);
        m_list->insertItems(std::min(drop.targetIndex, m_listKeys.size()-1), 1);
        return;
    }

    if(drop.sourceIndex >= m_listKeys.size())
        return;

    const uint64_t key=m_listKeys[drop.sourceIndex];

    m_listKeys.erase(m_listKeys.begin()+static_cast<std::ptrdiff_t>(drop.sourceIndex));
    m_listKeys.insert(m_listKeys.begin()+static_cast<std::ptrdiff_t>(std::min(drop.targetIndex,
        m_listKeys.size())), key);
    m_list->refresh();
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

    // a task at the list's top, and the oldest live task gone past the cap
    const uint64_t task=m_nextTask++;

    m_listKeys.insert(m_listKeys.begin(), task);
    m_list->insertItems(0, 1);
    m_liveTasks.push_back(task);

    while(m_liveTasks.size() > LiveCap)
    {
        auto found=std::find(m_listKeys.begin(), m_listKeys.end(), m_liveTasks.front());

        if(found != m_listKeys.end())
        {
            const size_t index=static_cast<size_t>(found-m_listKeys.begin());

            m_listKeys.erase(found);
            m_list->removeItems(index, 1);
        }
        m_liveTasks.erase(m_liveTasks.begin());
    }
}

}
