#include "demo/plotLines.h"
#include "gweni/controls/label.h"
#include "gweni/controls/plotLines.h"

#include <cmath>
#include <vector>

namespace gweni
{

// Two PlotLines, each under a Label naming it: a sine of 64 samples over two
// periods in the automatic range (-1 to 1), and 40 frame times in a fixed
// 0..33 ms range with a caption, a 1.5-unit line, two spikes past the range
// drawn on its top edge.
GWENI_CONTROL_CONSTRUCTOR(PlotLinesDemo)
{
    setDock(DockPosition::Center);

    controls::Label *label=newChild<controls::Label>();
    label->setText("64 samples, automatic range");
    label->setPos(10, 10);

    std::vector<float> values(64);

    for(size_t i=0; i < values.size(); ++i)
        values[i]=std::sin(float(i)*2.0f*3.14159265f/32.0f);

    m_series=newChild<controls::PlotLines>();
    m_series->setPos(10, 30);
    m_series->setSize(320, 100);
    m_series->setValues(values);

    label=newChild<controls::Label>();
    label->setText("Frame times, 0 to 33 ms");
    label->setPos(10, 145);

    const float frameTimes[]={16.6f, 16.7f, 16.5f, 18.2f, 16.6f, 24.9f, 16.7f, 16.6f,
        33.4f, 16.7f, 16.6f, 17.0f, 16.6f, 40.2f, 16.7f, 16.5f, 16.6f, 20.1f, 16.7f, 16.6f,
        16.6f, 17.4f, 16.5f, 16.8f, 22.3f, 16.6f, 16.7f, 16.6f, 19.0f, 16.6f, 16.5f, 16.7f,
        28.1f, 16.6f, 16.7f, 16.6f, 16.5f, 18.4f, 16.6f, 16.7f};

    m_captioned=newChild<controls::PlotLines>();
    m_captioned->setPos(10, 165);
    m_captioned->setSize(320, 80);
    m_captioned->setScaleRange(0.0f, 33.3f);
    m_captioned->setLineThickness(1.5f);
    m_captioned->setValues(std::vector<float>(std::begin(frameTimes), std::end(frameTimes)));
    m_captioned->setCaption("avg 18.5 ms");
}

}//namespace gweni
