#include "demo/chartTimeBars.h"
#include "gweni/controls/chart.h"
#include "gweni/controls/label.h"

#include <vector>

namespace gweni
{

double ChartTimeBarsDemo::firstDay()
{
    return static_cast<double>(controls::axis::daysFromCivil(2026, 11, 1))*86400.0;
}

// A Chart of four stacked bar series over two weeks of days on a time axis,
// pinned to UTC so its labels are the same in every zone, with a legend: a
// day's tokens by model, in millions. The values are whole numbers from a
// formula (no randomness, no libm), so every platform draws the same bars,
// and "other" has no bar on the weekends, a missing x counting as 0.
GWENI_CONTROL_CONSTRUCTOR(ChartTimeBarsDemo)
{
    setDock(DockPosition::Center);

    controls::Label *label=newChild<controls::Label>();
    label->setText("Tokens a day by model, two weeks (UTC)");
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(10, 10, 10, 4));

    m_chart=newChild<controls::Chart>();
    m_chart->setDock(DockPosition::Center);
    m_chart->setMargin(Margin(10, 0, 10, 10));
    m_chart->setUtcOffset(controls::axis::fixedUtcOffset(0));
    m_chart->setXAxisType(controls::ChartXAxisType::Time);
    m_chart->setBarInterval(86400.0);
    m_chart->setYTitle(controls::ChartAxisSide::Left, "M tokens");

    const char *const names[]={"opus", "sonnet", "haiku", "other"};
    const int base[]={22, 14, 6, 3};

    for(int s=0; s < 4; ++s)
    {
        const size_t series=m_chart->addSeries(names[s], controls::ChartSeriesType::StackedBar);
        std::vector<controls::ChartPoint> points;

        for(int day=0; day < Days; ++day)
        {
            const bool weekend=(day%7 == 0) || (day%7 == 6);

            if(s == 3 && weekend)
                continue;

            const int value=base[s]+((day*(7+3*s)+5*s)%(5+3*s))-(weekend ? base[s]/2 : 0);

            points.emplace_back(firstDay()+day*86400.0, double(value));
        }
        m_chart->setSeriesPoints(series, points);
    }
}

}//namespace gweni
