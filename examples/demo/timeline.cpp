#include "demo/timeline.h"
#include "gweni/controls/label.h"
#include "gweni/controls/timeline.h"

#include <cstdio>
#include <string>

namespace gweni
{

double TimelineDemo::day()
{
    return static_cast<double>(controls::axis::daysFromCivil(2026, 11, 1))*86400.0;
}

// A Timeline of the runs of a day on Hosts hosts of SlotsPerHost agent slots
// each, one lane per slot, pinned to UTC so its labels are the same in every
// zone: Runs bars, the first Runs % lanes lanes one more than the rest, each
// lane's day cut into as many equal stretches as it has runs and each run
// placed inside its stretch, so the bars of a lane never overlap. Their
// starts, lengths and states are whole seconds from a formula (no
// randomness, no libm), so every platform draws the same bars: a run that
// failed in ChartSeries5's red, one cancelled in ChartSeries6's yellow, the
// rest done in TimelineBar's blue. A click on a run names it in the line
// below.
GWENI_CONTROL_CONSTRUCTOR(TimelineDemo)
{
    setDock(DockPosition::Center);

    controls::Label *label=newChild<controls::Label>();
    label->setText("Runs per agent slot, one day (UTC)");
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(10, 10, 10, 4));

    m_status=newChild<controls::Label>();
    m_status->setText("Click a run to open it");
    m_status->setDock(DockPosition::Bottom);
    m_status->setMargin(Margin(10, 4, 10, 10));

    m_timeline=newChild<controls::Timeline>();
    m_timeline->setDock(DockPosition::Center);
    m_timeline->setMargin(Margin(10, 0, 10, 0));
    m_timeline->setUtcOffset(controls::axis::fixedUtcOffset(0));
    m_timeline->setXRange(day(), day()+86400.0);
    m_timeline->onBarClicked.add(this, &TimelineDemo::barClicked);

    const int lanes=Hosts*SlotsPerHost;

    for(int lane=0; lane < lanes; ++lane)
    {
        char caption[32];

        std::snprintf(caption, sizeof(caption), "host-%02d / %d", lane/SlotsPerHost+1, lane%SlotsPerHost);
        m_timeline->addLane(caption);

        const int runs=Runs/lanes+((lane < Runs%lanes) ? 1 : 0);
        const int stretch=86400/runs;

        for(int k=0; k < runs; ++k)
        {
            // up to 9 % of the stretch in, 45 % to 84 % of it long
            const int offset=stretch*((lane*3+k*5)%10)/100;
            const int length=stretch*(45+(lane*7+k*13)%40)/100;
            const double start=day()+double(stretch*k+offset);
            controls::TimelineBar bar(start, start+length, runId(size_t(lane), size_t(k)));
            const int state=(lane*11+k*17)%23;

            if(state == 0 || state == 7)
                bar.withSurface("ChartSeries5");
            else if(state == 3)
                bar.withSurface("ChartSeries6");
            m_timeline->addBar(size_t(lane), bar);
        }
    }
}

void TimelineDemo::barClicked(event::Info info)
{
    const size_t lane=size_t(info.point.x);

    m_status->setText("Opened run "+std::to_string(m_timeline->getClickedId())+" on "
        +m_timeline->getLaneCaption(lane));
}

}//namespace gweni
