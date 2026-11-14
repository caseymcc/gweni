/*
 *  Gweni demo
 *  See license in gweni.h
 */
#ifndef _gweni_reorder_example_h_
#define _gweni_reorder_example_h_

#include <gweni/controls/base.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/reorderable.h>

#include <vector>

namespace gweni
{
namespace controls
{
class CheckBoxWithLabel;
class DataTable;
class Label;
class ScrollControl;
class TabButton;
class TabControl;
class VirtualList;
}

//! A task card: its title on the GroupBox surface.
class ReorderDemoCard: public controls::Base
{
public:
    GWENI_CONTROL(ReorderDemoCard, controls::Base);

public:
    void setTitle(const String &title);
    String title() const;

private:
    controls::Label *m_title=nullptr;
};

//! Drag and drop reordering, on two pages. The Board: a three-column board
//! of cards, one
//! controls::CardReorder per column in one ReorderGroup: Todo is reordered
//! within itself and takes no other column's cards, with nothing dropped above
//! its pinned first card (canDrop); Running and Done take cards from any
//! column but are not reordered within. The demo applies every drop to its
//! cards. The Lists: a VirtualList of ListItems tasks (a
//! controls::VirtualListReorder keyed by each task's id), reordered within
//! itself, the demo moving the task in its vector; beside it a source-mode
//! DataTable of TableJobs jobs (DataTable::setItemsReorderable, keyed by job
//! id), sortable by either column: while unsorted it is reordered by hand
//! (the demo keeps that order and shows it again when the sort goes back to
//! none), sorted it takes none of its own rows, and either way a job dragged
//! into the task list goes there (the list and the table in one ReorderGroup,
//! the table taking nothing from the list). "Live updates" adds, every
//! LiveSeconds, a card below the pinned one and a task at the list's top and,
//! past LiveCap live ones of each, deletes the oldest.
class ReorderDemo: public controls::Base
{
public:
    GWENI_CONTROL(ReorderDemo, controls::Base);

public:
    GWENI_DRAWS_NOTHING

    enum Column : size_t
    {
        Todo=0,
        Running,
        Done,
        ColumnCount
    };

    static constexpr double LiveSeconds=1.5;
    static constexpr size_t LiveCap=3;
    static constexpr int CardHeight=32;
    static constexpr int CardGap=6;
    static constexpr size_t ListItems=10000;
    static constexpr size_t TableJobs=2000;
    //! A job's key, apart from the tasks' (which count up from 0)
    static constexpr uint64_t JobKeyBase=1000000;

    controls::ScrollControl *column(size_t index) const { return m_columns.at(index); }
    controls::CardReorder *columnReorder(size_t index) const { return m_reorders.at(index); }
    //! A column's cards in order
    std::vector<ReorderDemoCard *> cards(size_t column) const;
    //! Todo's pinned first card
    ReorderDemoCard *pinned() const { return m_pinned; }
    //! Adds a card to a column at \p index (its end past the last)
    ReorderDemoCard *addCard(size_t column, const String &title, size_t index=SIZE_MAX);

    //! Every drop the demo applied, in order
    const std::vector<controls::ItemDrop> &drops() const { return m_drops; }

    controls::TabControl *pages() const { return m_pages; }
    void showBoard();
    void showLists();

    controls::VirtualList *list() const { return m_list; }
    controls::VirtualListReorder *listReorder() const { return m_listReorder; }
    //! The list's task ids, in order
    const std::vector<uint64_t> &listKeys() const { return m_listKeys; }
    //! The text the list shows for a task (or a job dropped into it)
    static String taskText(uint64_t key);

    controls::DataTable *table() const { return m_table; }
    //! The jobs the table shows, in its order now (sorted or by hand)
    const std::vector<uint64_t> &tableKeys() const { return m_tableShown; }
    //! A job's priority, the second column
    static int jobPriority(uint64_t key);

    controls::CheckBoxWithLabel *liveToggle() const { return m_liveToggle; }
    void setLiveUpdates(bool live);
    //! One step of the live updates: a card below the pinned one, and the
    //! oldest live card deleted past LiveCap
    void liveTick();

private:
    void onLiveChanged(event::Info info);
    void applyDrop(const controls::ItemDrop &drop);
    void applyListDrop(const controls::ItemDrop &drop);
    void applyTableDrop(const controls::ItemDrop &drop);
    void onTableSorted(event::Info info);
    // the shown order from the hand order and the sort column
    void showTableOrder();

    std::vector<controls::ScrollControl *> m_columns;
    std::vector<controls::CardReorder *> m_reorders;
    controls::ReorderGroup m_group;
    controls::TabControl *m_pages=nullptr;
    controls::TabButton *m_boardTab=nullptr;
    controls::TabButton *m_listsTab=nullptr;
    controls::VirtualList *m_list=nullptr;
    controls::VirtualListReorder *m_listReorder=nullptr;
    std::vector<uint64_t> m_listKeys;
    std::vector<uint64_t> m_liveTasks;
    controls::DataTable *m_table=nullptr;
    controls::ReorderGroup m_listGroup;
    std::vector<uint64_t> m_tableOrder;
    std::vector<uint64_t> m_tableShown;
    uint64_t m_nextTask=0;
    ReorderDemoCard *m_pinned=nullptr;
    std::vector<controls::ItemDrop> m_drops;
    controls::CheckBoxWithLabel *m_liveToggle=nullptr;
    controls::TimerHandle m_liveTimer;
    std::vector<size_t> m_liveCards;
    int m_liveCount=0;
};

}

#endif
