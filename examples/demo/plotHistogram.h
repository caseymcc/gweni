#ifndef _gweni_plothistogram_example_h_
#define _gweni_plothistogram_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class PlotHistogram;
}

class PlotHistogramDemo: public controls::Base
{
public:
    GWENI_CONTROL(PlotHistogramDemo, controls::Base);

public:
    //! The 32-bar series, for a test to change one of its samples.
    controls::PlotHistogram *series() const { return m_series; }
    //! The one with a caption.
    controls::PlotHistogram *captioned() const { return m_captioned; }

private:
    controls::PlotHistogram *m_series=nullptr;
    controls::PlotHistogram *m_captioned=nullptr;
};

}//namespace gweni

#endif//_gweni_plothistogram_example_h_
