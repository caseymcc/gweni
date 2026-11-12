#ifndef _gweni_logView_example_h_
#define _gweni_logView_example_h_

#include <gweni/controls/base.h>
#include <gweni/controls/logView.h>

#include <vector>

namespace gweni
{
namespace controls
{
class Label;
}

//! An agent run's log as master-mind's dashboard shows it: a LogView of mixed
//! events (assistant text, tool calls, tool output as collapsible blocks) and,
//! beside it, a five-line excerpt LogView per running agent, each with a small
//! cap. Rows are streamed in by an animation on the canvas's clock, so a run on
//! a stepped clock streams the same rows every time and one on a frozen clock
//! none.
class LogViewDemo: public controls::Base
{
public:
    GWENI_CONTROL(LogViewDemo, controls::Base);

public:
    //! The events the main log holds when built, and how many more it streams
    //! at StreamRate a second (while the demo is shown) before it stops.
    static constexpr int PrefilledRows=240;
    static constexpr int StreamRows=600;
    static constexpr float StreamRate=12.0f;
    //! The excerpt views, their rows and their cap.
    static constexpr int ExcerptCount=3;
    static constexpr int ExcerptRows=5;
    static constexpr size_t ExcerptCap=20;
    //! The rows "Show older" prepends.
    static constexpr int OlderRows=40;

    //! Event n of the main log, the same for the same n (negative: an older
    //! one, "Show older"'s).
    static controls::LogRow event(long long n);
    //! Line n of excerpt agent's stream.
    static controls::LogRow excerptLine(int agent, long long n);

    //! Streams the rows due by the canvas clock: one frame of the animation.
    void stream();

    void onShowOlder(event::Info);
    void onClear(event::Info);

    controls::LogView *m_log=nullptr;
    std::vector<controls::LogView *> m_excerpts;

private:
    //! The clock's time when streaming began, less the time the demo spent
    //! hidden; the events streamed so far; the lowest event shown.
    float m_start=0.0f;
    float m_last=0.0f;
    bool m_started=false;
    int m_streamed=0;
    long long m_oldest=0;
};

}//namespace gweni

#endif//_gweni_logView_example_h_
