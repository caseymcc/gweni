#ifndef _gweni_chartlines_example_h_
#define _gweni_chartlines_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Chart;
}

class ChartLinesDemo: public controls::Base
{
public:
    GWENI_CONTROL(ChartLinesDemo, controls::Base);

public:
    //! The chart, for a test to change, hover and click it.
    controls::Chart *chart() const { return m_chart; }
    //! Its day's start (epoch seconds, 00:00 UTC) and its samples, every
    //! quarter hour.
    static double start();
    static constexpr int Samples=97;

private:
    controls::Chart *m_chart=nullptr;
};

}//namespace gweni

#endif//_gweni_chartlines_example_h_
