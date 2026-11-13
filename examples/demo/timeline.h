#ifndef _gweni_timeline_example_h_
#define _gweni_timeline_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Label;
class Timeline;
}

class TimelineDemo: public controls::Base
{
public:
    GWENI_CONTROL(TimelineDemo, controls::Base);

public:
    //! The timeline, for a test to change, hover, click and scroll it.
    controls::Timeline *timeline() const { return m_timeline; }
    //! The line under it saying which run was clicked last.
    controls::Label *status() const { return m_status; }
    //! The day it shows (epoch seconds, 00:00 UTC): the x range is that day.
    static double day();
    //! Its lanes: SlotsPerHost agent slots on each of Hosts hosts, and the
    //! runs between them.
    static constexpr int Hosts=12;
    static constexpr int SlotsPerHost=8;
    static constexpr int Runs=1000;
    //! The app id of run \p index of \p lane.
    static uint64_t runId(size_t lane, size_t index) { return uint64_t(lane)*100+index; }

private:
    void barClicked(event::Info info);

    controls::Timeline *m_timeline=nullptr;
    controls::Label *m_status=nullptr;
};

}//namespace gweni

#endif//_gweni_timeline_example_h_
