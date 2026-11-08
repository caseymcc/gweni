#ifndef _gweni_plotlines_example_h_
#define _gweni_plotlines_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class PlotLines;
}

class PlotLinesDemo: public controls::Base
{
public:
    GWENI_CONTROL(PlotLinesDemo, controls::Base);

public:
    //! The 64-sample sine, for a test to change one of its samples.
    controls::PlotLines *series() const { return m_series; }
    //! The frame times, with a caption.
    controls::PlotLines *captioned() const { return m_captioned; }

private:
    controls::PlotLines *m_series=nullptr;
    controls::PlotLines *m_captioned=nullptr;
};

}//namespace gweni

#endif//_gweni_plotlines_example_h_
