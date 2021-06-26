#ifndef _gweni_crossSplitter_example_h_
#define _gweni_crossSplitter_example_h_

#include <gweni/controls/base.h>
#include <gweni/controls/crossSplitter.h>

namespace gweni
{

class CrossSplitterDemo: public controls::Base
{
public:
    GWENI_CONTROL(CrossSplitterDemo, controls::Base);

    void zoomTest(event::Info info);
    void unZoomTest(event::Info info);
    void centerPanels(event::Info info);

private:
    bool m_splittersVisible;
    int	m_curZoom;
    controls::CrossSplitter *m_splitter;
};

}//namespace gweni

#endif//_gweni_crossSplitter_example_h_
