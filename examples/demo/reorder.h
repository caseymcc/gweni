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
class Label;
class ScrollControl;
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

//! Drag and drop reordering on a three-column board of cards, one
//! controls::CardReorder per column in one ReorderGroup: Todo is reordered
//! within itself and takes no other column's cards, with nothing dropped above
//! its pinned first card (canDrop); Running and Done take cards from any
//! column but are not reordered within. The demo applies every drop to its
//! cards. "Live updates" adds a card below the pinned one every LiveSeconds
//! and, past LiveCap live cards, deletes the oldest.
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

    controls::CheckBoxWithLabel *liveToggle() const { return m_liveToggle; }
    void setLiveUpdates(bool live);
    //! One step of the live updates: a card below the pinned one, and the
    //! oldest live card deleted past LiveCap
    void liveTick();

private:
    void onLiveChanged(event::Info info);
    void applyDrop(const controls::ItemDrop &drop);

    std::vector<controls::ScrollControl *> m_columns;
    std::vector<controls::CardReorder *> m_reorders;
    controls::ReorderGroup m_group;
    ReorderDemoCard *m_pinned=nullptr;
    std::vector<controls::ItemDrop> m_drops;
    controls::CheckBoxWithLabel *m_liveToggle=nullptr;
    controls::TimerHandle m_liveTimer;
    std::vector<size_t> m_liveCards;
    int m_liveCount=0;
};

}

#endif
