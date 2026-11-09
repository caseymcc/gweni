#ifndef _gweni_statsOverlay_example_h_
#define _gweni_statsOverlay_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace tools
{
class StatsOverlay;
}

//! A small app part (a GroupBox of Buttons, a check box and a ListBox) with
//! the stats overlay in the page's bottom-right corner, over the frames the
//! page draws.
class StatsOverlayDemo: public controls::Base
{
public:
    GWENI_CONTROL(StatsOverlayDemo, controls::Base);

public:
    tools::StatsOverlay *overlay() const { return m_overlay; }

private:
    tools::StatsOverlay *m_overlay=nullptr;
};

}//namespace gweni

#endif//_gweni_statsOverlay_example_h_
