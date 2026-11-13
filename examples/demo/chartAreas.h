#ifndef _gweni_chartareas_example_h_
#define _gweni_chartareas_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Chart;
}

class ChartAreasDemo: public controls::Base
{
public:
    GWENI_CONTROL(ChartAreasDemo, controls::Base);

public:
    //! The chart, for a test to change, hover and click it.
    controls::Chart *chart() const { return m_chart; }
    //! The first of its months (epoch seconds, 00:00 UTC on the 1st) and how
    //! many it shows.
    static double month(int index);
    static constexpr int Months=12;

private:
    controls::Chart *m_chart=nullptr;
};

}//namespace gweni

#endif//_gweni_chartareas_example_h_
