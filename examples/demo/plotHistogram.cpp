#include "demo/plotHistogram.h"
#include "gweni/controls/label.h"
#include "gweni/controls/plotHistogram.h"

#include <cmath>
#include <vector>

namespace gweni
{

// Two PlotHistograms, each under a Label naming it: a 32-bar series in the
// automatic range (a bell over a slow wave, no bar at its zero), and frame
// times in a fixed 0..33 ms range with a caption over the bars, two of them
// past the range and clamped to its top.
GWENI_CONTROL_CONSTRUCTOR(PlotHistogramDemo)
{
    setDock(DockPosition::Center);

    controls::Label *label=newChild<controls::Label>();
    label->setText("32 bars, automatic range");
    label->setPos(10, 10);

    std::vector<float> values(32);

    for(size_t i=0; i < values.size(); ++i)
    {
        const float x=(float(i)-15.5f)/6.0f;

        values[i]=std::exp(-x*x)*8.0f+2.0f*std::sin(float(i)*0.6f)+2.0f;
    }
    values[20]=0.0f;

    m_series=newChild<controls::PlotHistogram>();
    m_series->setPos(10, 30);
    m_series->setSize(320, 100);
    m_series->setValues(values);

    label=newChild<controls::Label>();
    label->setText("Frame times, 0 to 33 ms");
    label->setPos(10, 145);

    const float frameTimes[]={16.6f, 16.7f, 16.5f, 18.2f, 16.6f, 24.9f, 16.7f, 16.6f,
        33.4f, 16.7f, 16.6f, 17.0f, 16.6f, 40.2f, 16.7f, 16.5f, 16.6f, 20.1f, 16.7f, 16.6f};

    m_captioned=newChild<controls::PlotHistogram>();
    m_captioned->setPos(10, 165);
    m_captioned->setSize(320, 80);
    m_captioned->setScaleRange(0.0f, 33.3f);
    m_captioned->setValues(std::vector<float>(std::begin(frameTimes), std::end(frameTimes)));
    m_captioned->setCaption("avg 19.2 ms");
}

}//namespace gweni
