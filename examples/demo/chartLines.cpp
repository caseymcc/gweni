#include "demo/chartLines.h"
#include "gweni/controls/chart.h"
#include "gweni/controls/label.h"

#include <cstdio>
#include <vector>

namespace gweni
{

double ChartLinesDemo::start()
{
    return static_cast<double>(controls::axis::daysFromCivil(2026, 11, 12))*86400.0;
}

// A Chart over one day (UTC) with a value axis on each side: a host's memory
// in GB as a line on the left, and its GPU's use in percent as a step line on
// the right, every quarter hour; two captioned markers where the host was
// restarted and deployed, and a shaded span for its maintenance window. The
// memory climbs and falls back at the restart; the values are whole numbers
// from a formula, so every platform draws the same lines.
GWENI_CONTROL_CONSTRUCTOR(ChartLinesDemo)
{
    setDock(DockPosition::Center);

    controls::Label *label=newChild<controls::Label>();
    label->setText("Host memory and GPU use, one day (UTC)");
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(10, 10, 10, 4));

    m_chart=newChild<controls::Chart>();
    m_chart->setDock(DockPosition::Center);
    m_chart->setMargin(Margin(10, 0, 10, 10));
    m_chart->setUtcOffset(controls::axis::fixedUtcOffset(0));
    m_chart->setXAxisType(controls::ChartXAxisType::Time);
    m_chart->setXRange(start(), start()+86400.0);
    m_chart->setYRange(controls::ChartAxisSide::Right, 0.0, 100.0);
    m_chart->setYTitle(controls::ChartAxisSide::Left, "GB");
    m_chart->setYTitle(controls::ChartAxisSide::Right, "GPU %");
    m_chart->setYFormatter(controls::ChartAxisSide::Right, [](double value)
        {
            char text[16];

            std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(value));
            return String(text);
        });

    const size_t memory=m_chart->addSeries("memory", controls::ChartSeriesType::Line);
    const size_t gpu=m_chart->addSeries("gpu", controls::ChartSeriesType::StepLine, controls::ChartAxisSide::Right);
    std::vector<controls::ChartPoint> memoryPoints;
    std::vector<controls::ChartPoint> gpuPoints;

    for(int i=0; i < Samples; ++i)
    {
        const double t=start()+i*900.0;
        // the restart at 06:00 (sample 24) drops the memory back
        const int since=(i < 24) ? i+40 : i-24;
        const int memoryTenths=120+since*3+((i*7)%11);
        const int gpuUse=(i < 40 || i > 52) ? 20+((i*13)%5)*15 : 5;

        memoryPoints.emplace_back(t, memoryTenths/10.0);
        if(i%4 == 0)
            gpuPoints.emplace_back(t, double(gpuUse));
    }
    m_chart->setSeriesPoints(memory, memoryPoints);
    m_chart->setSeriesPoints(gpu, gpuPoints);

    m_chart->addSpan(start()+10*3600.0, start()+13*3600.0);
    m_chart->addMarker(start()+6*3600.0, "restart");
    m_chart->addMarker(start()+15.5*3600.0, "deploy");
}

}//namespace gweni
