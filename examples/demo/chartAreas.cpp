#include "demo/chartAreas.h"
#include "gweni/controls/chart.h"
#include "gweni/controls/label.h"

#include <vector>

namespace gweni
{

double ChartAreasDemo::month(int index)
{
    return static_cast<double>(controls::axis::daysFromCivil(2026, 1+index, 1))*86400.0;
}

// A Chart of three stacked area series over a year of months on a time axis,
// pinned to UTC so its labels are the same in every zone, with a legend: the
// running total of a year's tokens by model, in billions, at the start of
// each month. Each band is filled from the one below it to its line; the
// months' additions are whole numbers from a formula (no randomness, no
// libm), so every platform draws the same areas.
GWENI_CONTROL_CONSTRUCTOR(ChartAreasDemo)
{
    setDock(DockPosition::Center);

    controls::Label *label=newChild<controls::Label>();
    label->setText("Tokens by model, running total over a year (UTC)");
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(10, 10, 10, 4));

    m_chart=newChild<controls::Chart>();
    m_chart->setDock(DockPosition::Center);
    m_chart->setMargin(Margin(10, 0, 10, 10));
    m_chart->setUtcOffset(controls::axis::fixedUtcOffset(0));
    m_chart->setXAxisType(controls::ChartXAxisType::Time);
    m_chart->setXGrid(true);
    m_chart->setYTitle(controls::ChartAxisSide::Left, "B tokens");

    const char *const names[]={"opus", "sonnet", "haiku"};
    const int base[]={30, 18, 6};

    for(int s=0; s < 3; ++s)
    {
        const size_t series=m_chart->addSeries(names[s], controls::ChartSeriesType::StackedArea);
        std::vector<controls::ChartPoint> points;
        int total=0;

        for(int m=0; m < Months; ++m)
        {
            points.emplace_back(month(m), double(total));
            total+=base[s]+((m*(7+3*s)+5*s)%(9+2*s));
        }
        m_chart->setSeriesPoints(series, points);
    }
}

}//namespace gweni
