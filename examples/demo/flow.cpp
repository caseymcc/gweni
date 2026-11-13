#include "demo/flow.h"
#include "gweni/controls/label.h"
#include "gweni/controls/layout/flow.h"
#include "gweni/controls/scrollControl.h"

#include <string>

namespace gweni
{

namespace
{

const char *const Names[]={"localworker", "gweni", "master-mind", "picard", "riker", "vcpkg cache",
    "render pool", "site build", "bench runs", "references", "docs samples", "nightly"};
const char *const Sentences[]={
    "Every card stretches to its column's width, and its text wraps there.",
    "The row is as tall as its tallest card.",
    "Columns follow the Flow's own width, not the window's.",
    "A stable gutter keeps the viewport's width fixed, so a resize lays out once.",
    "Four agents share the host and take turns on the render lock.",
    "Nothing here is measured at a width it does not have."};
constexpr int SentenceCount=sizeof(Sentences)/sizeof(Sentences[0]);

}//namespace

GWENI_CONTROL_CONSTRUCTOR(FlowDemoCard)
{
    setPadding(Padding(10, 8, 10, 10));

    m_title=newChild<controls::Label>();
    m_title->setDock(DockPosition::Top);
    m_title->makeColorBright();
    m_title->setMargin(Margin(0, 0, 0, 4));

    m_body=newChild<controls::Label>();
    m_body->setDock(DockPosition::Top);
    m_body->setWrap(true);
}

void FlowDemoCard::setTitle(const String &title)
{
    m_title->setText(title);
}

void FlowDemoCard::setBody(const String &body)
{
    m_body->setText(body);
}

String FlowDemo::cardTitle(int index, int generation)
{
    const String name=Names[index%(sizeof(Names)/sizeof(Names[0]))];

    return (generation == 0)?name:name+" ("+std::to_string(generation)+")";
}

// One to four sentences, picked by the index and the generation, so the
// cards of a row differ in height.
String FlowDemo::cardBody(int index, int generation)
{
    const int count=1+(index*7+generation*3)%4;
    String body;

    for(int i=0; i < count; ++i)
        body+=String(i?" ":"")+Sentences[(index*5+i+generation)%SentenceCount];
    return body;
}

GWENI_CONTROL_CONSTRUCTOR(FlowDemo)
{
    setDock(DockPosition::Center);

    m_scroll=newChild<controls::ScrollControl>();
    m_scroll->setDock(DockPosition::Center);
    m_scroll->setScroll(false, true);
    m_scroll->setAutoHideBars(false);
    // A height asked for, which the fill dock does not read: the
    // ScrollControl then asks for it rather than for its content, whose
    // height its Y measure has only at the width before (it lays the
    // content out at its new width after), so no size above it depends on
    // an earlier pass.
    m_scroll->setHeight(200);

    m_flow=m_scroll->newChild<controls::layout::Flow>();
    m_flow->setGap(10);
    m_flow->setMinCardWidth(240);
    m_flow->setBreakpoints({{TwoColumns, 1}, {ThreeColumns, 2}});
    m_flow->setMaxColumns(3);

    addCards();
}

void FlowDemo::addCards()
{
    for(int i=0; i < Cards; ++i)
    {
        FlowDemoCard *card=m_flow->newChild<FlowDemoCard>();

        card->setTitle(cardTitle(i, m_generation));
        card->setBody(cardBody(i, m_generation));
        m_cards.push_back(card);
    }
    ++m_generation;
}

void FlowDemo::removeCards()
{
    for(FlowDemoCard *card:m_cards)
        delete card;
    m_cards.clear();
}

void FlowDemo::rebuildCards()
{
    for(FlowDemoCard *card:m_cards)
        card->delayedDelete();
    m_cards.clear();
    addCards();
}

}//namespace gweni
