#include "demo/logView.h"
#include "gweni/anim.h"
#include "gweni/controls/button.h"
#include "gweni/controls/label.h"
#include "gweni/platforms/platform.h"

#include <cmath>
#include <limits>
#include <string>

namespace gweni
{

namespace
{

const char *const Words[]={"the", "agent", "reads", "a", "file", "and", "then", "runs", "its", "tests", "before",
    "it", "changes", "one", "line", "of", "layout", "so", "every", "row", "is", "measured", "again", "at", "width",
    "while", "log", "keeps", "following", "new", "events", "from", "tool", "calls"};
constexpr size_t WordCount=sizeof(Words)/sizeof(Words[0]);

const char *const Commands[]={"ls -la source/controls", "git status --short", "ctest -LE gpu -j16",
    "grep -n LogView include", "cmake --build build -j", "cat docs/architecture.md"};
constexpr size_t CommandCount=sizeof(Commands)/sizeof(Commands[0]);

const char *const Files[]={"logView.cpp", "virtualList.h", "heightIndex.cpp", "texturedControls.cpp", "AGENTS.md"};
constexpr size_t FileCount=sizeof(Files)/sizeof(Files[0]);

// Tokyo Night: a tool call blue, its result green, a note grey, a warning orange
const Color CallColor(0x7a, 0xa2, 0xf7);
const Color ResultColor(0x9e, 0xce, 0x6a);
const Color NoteColor(0x56, 0x5f, 0x89);
const Color WarnColor(0xff, 0x9e, 0x64);

unsigned long long mix(long long n)
{
    unsigned long long x=static_cast<unsigned long long>(n)*0x9e3779b97f4a7c15ull+0x632be59bd9b4e019ull;

    x^=x>>29;
    x*=0xbf58476d1ce4e5b9ull;
    return x^(x>>32);
}

String sentence(unsigned long long seed, size_t words)
{
    String text;

    for(size_t i=0; i < words; ++i)
    {
        if(i > 0)
            text+=" ";
        text+=Words[(seed/(i+1)+i*13)%WordCount];
    }
    return text;
}

// The animation streaming the demo's rows, until it has streamed them all.
// It waits while the demo is hidden, so a hidden page keeps no idle loop
// drawing; its show marks the canvas, and that frame's run picks it up.
class StreamAnimation: public anim::Animation
{
public:
    void think() override
    {
        static_cast<LogViewDemo *>(m_control)->stream();
    }

    double nextThinkTime() const override
    {
        if(!m_control->visible())
            return std::numeric_limits<double>::infinity();
        return anim::Animation::nextThinkTime();
    }
};

}//namespace

// The main log fills the demo below a row of buttons, an excerpt column to
// its right: a caption and a five-row LogView per agent, each capped and
// following. The rows wrap at the log's width, and every one is measured by
// the second frame (the re-measure budget covers the log), so a capture after
// a resize or a scale change shows the same thumb a fresh build shows.
GWENI_CONTROL_CONSTRUCTOR(LogViewDemo)
{
    setDock(DockPosition::Center);

    controls::Base *bar=newChild<controls::Base>();

    bar->setDock(DockPosition::Top);
    bar->setHeight(24);
    bar->setMargin(Margin(10, 8, 10, 4));

    struct
    {
        const char *text;
        void (LogViewDemo::*handler)(event::Info);
    } const buttons[]={
        {"Show older", &LogViewDemo::onShowOlder},
        {"Clear", &LogViewDemo::onClear},
    };

    for(const auto &b : buttons)
    {
        controls::Button *button=bar->newChild<controls::Button>();

        button->setText(b.text);
        button->setDock(DockPosition::Left);
        button->setWidth(90);
        button->setMargin(Margin(0, 0, 6, 0));
        button->onPressCaller.add(this, b.handler);
    }

    controls::Base *column=newChild<controls::Base>();

    column->setDock(DockPosition::Right);
    column->setWidth(230);
    column->setMargin(Margin(0, 0, 10, 10));

    for(int agent=0; agent < ExcerptCount; ++agent)
    {
        controls::Label *caption=column->newChild<controls::Label>();

        caption->setText("agent-"+std::to_string(agent+1));
        caption->setDock(DockPosition::Top);
        caption->setMargin(Margin(0, agent == 0?0:8, 0, 2));

        controls::LogView *excerpt=column->newChild<controls::LogView>();

        excerpt->setDock(DockPosition::Top);
        excerpt->setMaxRows(ExcerptCap);
        excerpt->setHeight(excerpt->heightForRows(ExcerptRows));
        for(int i=0; i < ExcerptRows; ++i)
            excerpt->append(excerptLine(agent, i));
        m_excerpts.push_back(excerpt);
    }

    m_log=newChild<controls::LogView>();
    m_log->setDock(DockPosition::Center);
    m_log->setMargin(Margin(10, 0, 10, 10));
    m_log->setRemeasureBudget(PrefilledRows+StreamRows+16*OlderRows);
    for(int i=0; i < PrefilledRows; ++i)
        m_log->append(event(i));

    anim::add(this, new StreamAnimation());
}

controls::LogRow LogViewDemo::event(long long n)
{
    const unsigned long long seed=mix(n);
    const String number=std::to_string(n);

    switch((n%9+9)%9)
    {
    case 1:
    case 5:
        // the assistant's text, one to four lines at the demo's width
        return controls::LogRow("assistant "+number+": "+sentence(seed, 6+seed%34)+".");
    case 2:
        return controls::LogRow("-> Read "+String(Files[seed%FileCount])+" (event "+number+")", CallColor);
    case 4:
    {
        // a tool call's output: a block of 3 to 40 lines
        const size_t lines=3+seed%38;
        std::vector<String> output;

        for(size_t i=0; i < lines; ++i)
            output.push_back("  "+std::to_string(i+1)+"  "+sentence(seed+i*7919, 2+(seed>>i)%6));
        return controls::LogRow::block("Bash: "+String(Commands[seed%CommandCount])+" #"+number,
            std::move(output));
    }
    case 7:
        if(seed%3 == 0)
            return controls::LogRow("warning: event "+number+" took "+std::to_string(100+seed%900)+" ms", WarnColor);
        return controls::LogRow("<- ok, "+std::to_string(seed%400)+" lines (event "+number+")", ResultColor);
    default:
        return controls::LogRow("note "+number+": "+sentence(seed, 3+seed%5), NoteColor);
    }
}

controls::LogRow LogViewDemo::excerptLine(int agent, long long n)
{
    const unsigned long long seed=mix(n*31+agent);
    const String step=std::to_string(n+1);

    switch(seed%4)
    {
    case 0:
        return controls::LogRow(step+" -> Read "+String(Files[(seed>>8)%FileCount]), CallColor);
    case 1:
        return controls::LogRow(step+" <- ok", ResultColor);
    case 2:
        return controls::LogRow(step+" Bash: "+String(Commands[(seed>>8)%CommandCount]));
    default:
        return controls::LogRow(step+" "+sentence(seed, 3), NoteColor);
    }
}

// The rows due by the clock since streaming began, the time hidden left out
// (a page the shell does not show streams nothing and invalidates nothing).
// The excerpts stream a row every half second plus a third per agent.
void LogViewDemo::stream()
{
    const double now=platform::getTimeInSeconds();

    if(!m_started)
    {
        m_started=true;
        m_start=now;
        m_last=now;
    }
    if(!visible())
    {
        m_start+=now-m_last;
        m_last=now;
        m_wasHidden=true;
        return;
    }
    // shown again: an idle loop may have slept since the last hidden run
    if(m_wasHidden)
    {
        m_start+=now-m_last;
        m_wasHidden=false;
    }
    m_last=now;

    const float elapsed=static_cast<float>(std::max(0.0, now-m_start));
    const int due=std::min(StreamRows, static_cast<int>(std::floor(elapsed*StreamRate)));

    while(m_streamed < due)
    {
        m_log->append(event(PrefilledRows+m_streamed));
        ++m_streamed;
    }

    const float streamTime=static_cast<float>(StreamRows)/StreamRate;
    const float excerptTime=std::min(elapsed, streamTime);

    for(int agent=0; agent < ExcerptCount; ++agent)
    {
        controls::LogView *excerpt=m_excerpts[agent];
        const float period=0.5f+agent/3.0f;
        const long long lines=ExcerptRows+static_cast<long long>(std::floor(excerptTime/period));
        // every row it was given: the ids run on past the cap
        long long shown=excerpt->lastRowId()+1;

        while(shown < lines)
            excerpt->append(excerptLine(agent, shown++));
    }

    if(m_streamed >= StreamRows)
        anim::cancel(this);
}

void LogViewDemo::onShowOlder(event::Info)
{
    std::vector<controls::LogRow> rows;

    for(int i=OlderRows; i > 0; --i)
        rows.push_back(event(m_oldest-i));
    m_oldest-=OlderRows;
    m_log->prepend(rows);
}

void LogViewDemo::onClear(event::Info)
{
    m_log->clear();
}

}//namespace gweni
