#ifndef _gweni_charttimebars_example_h_
#define _gweni_charttimebars_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Chart;
}

class ChartTimeBarsDemo: public controls::Base
{
public:
    GWENI_CONTROL(ChartTimeBarsDemo, controls::Base);

public:
    //! The chart, for a test to change, hover and click it.
    controls::Chart *chart() const { return m_chart; }
    //! Its first day (epoch seconds, 00:00 UTC) and the days it shows.
    static double firstDay();
    static constexpr int Days=14;

private:
    controls::Chart *m_chart=nullptr;
};

}//namespace gweni

#endif//_gweni_charttimebars_example_h_
