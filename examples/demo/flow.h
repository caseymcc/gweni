#ifndef _gweni_flow_example_h_
#define _gweni_flow_example_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
class Label;
class ScrollControl;
namespace layout
{
class Flow;
}
}

//! A card of FlowDemo: a title and a wrapped body, on the GroupBox surface.
class FlowDemoCard: public controls::Base
{
public:
    GWENI_CONTROL(FlowDemoCard, controls::Base);

public:
    void setTitle(const String &title);
    void setBody(const String &body);
    controls::Label *body() const { return m_body; }

private:
    controls::Label *m_title=nullptr;
    controls::Label *m_body=nullptr;
};

//! A dashboard's cards in a layout::Flow, in a vertical ScrollControl docked
//! to fill the demo with a stable gutter (the bar always shown), so the
//! cards' wrapped text lays out in one pass at any width. The Flow's own
//! width picks the columns: 1 below 1024 units, 2 below 1536, else 3.
class FlowDemo: public controls::Base
{
public:
    GWENI_CONTROL(FlowDemo, controls::Base);

public:
    //! The breakpoints the Flow is given.
    static constexpr int TwoColumns=1024;
    static constexpr int ThreeColumns=1536;
    static constexpr int Cards=12;

    controls::ScrollControl *scroll() const { return m_scroll; }
    controls::layout::Flow *flow() const { return m_flow; }
    const std::vector<FlowDemoCard *> &cards() const { return m_cards; }

    //! Card `index`'s text, the `generation`th time the cards are made.
    static String cardTitle(int index, int generation);
    static String cardBody(int index, int generation);

    //! Deletes every card (delayedDelete) and makes Cards new ones of the
    //! next generation in their place, in the same frame: a dashboard's
    //! rebuild, which keeps the scroll offset.
    void rebuildCards();
    //! Deletes every card at once, with no new ones.
    void removeCards();
    //! Makes Cards cards of the next generation.
    void addCards();

private:
    controls::ScrollControl *m_scroll=nullptr;
    controls::layout::Flow *m_flow=nullptr;
    std::vector<FlowDemoCard *> m_cards;
    int m_generation=0;
};

}//namespace gweni

#endif//_gweni_flow_example_h_
