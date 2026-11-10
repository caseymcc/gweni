/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#include <gweni/tools/statsOverlay.h>

#include <gweni/anim.h>
#include <gweni/gweni.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/platforms/platform.h>
#include <gweni/primitiveIds.h>
#include <gweni/skin.h>
#include <gweni/utility.h>

#include <algorithm>
#include <unordered_map>

namespace gweni
{
namespace tools
{

namespace
{

// The panel's geometry, in units: every row a fixed height, every cell a fixed
// width, so a refresh changes text and never a size.
constexpr int Pad=6;
constexpr int RowHeight=15;
constexpr int ContentWidth=202;
constexpr int PlotHeight=52;
constexpr int PlotGap=4;
constexpr int RowCount=14;
constexpr int Width=ContentWidth+2*Pad;
constexpr int Height=RowCount*RowHeight+2*(PlotGap+PlotHeight)+2*Pad;

// A time row: its name, then min, avg and max right-aligned.
constexpr int TimeName=58;
constexpr int TimeColumn=48;
// A counter row: two name/value pairs, the values right-aligned.
constexpr int Name1=62;
constexpr int Value1=38;
constexpr int Name2X=Name1+Value1+8;
constexpr int Name2=60;
constexpr int Value2X=Name2X+Name2;
constexpr int Value2=ContentWidth-Value2X;

static_assert(RowCount == 14, "a row per StatsOverlay::Row");

//! setBottomReserve()'s units per canvas, with the canvas's control id: a
//! canvas made at a deleted one's address has another, so it reads 0.
struct BottomReserve
{
    size_t canvasId;
    int units;
};

std::unordered_map<const controls::Canvas *, BottomReserve> &bottomReserves()
{
    static std::unordered_map<const controls::Canvas *, BottomReserve> reserves;
    return reserves;
}

String ms(uint64_t micros)
{
    return format("%.1f", double(micros)/1000.0);
}

//! A count in at most 5 characters: as it is below 100000, else in thousands
//! or millions.
String count(size_t n)
{
    if(n < 100000)
        return format("%zu", n);
    if(n < 10000000)
        return format("%zuk", n/1000);
    return format("%zuM", n/1000000);
}

}//namespace

event::Caller StatsOverlay::onOpenChangedCaller;

void StatsOverlay::TimeRange::add(uint64_t micros)
{
    if(count == 0)
    {
        min=micros;
        max=micros;
    }
    else
    {
        min=std::min(min, micros);
        max=std::max(max, micros);
    }
    sum+=micros;
    ++count;
}

GWENI_CONTROL_CONSTRUCTOR(StatsOverlay)
{
    setExcludedFromStats(true);
    // placed and sized by itself, in the canvas's corner, not by the canvas
    setPlacement(controls::Placement::Owner);
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
    setTabable(false);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setSize(Width, Height);

    m_rows.resize(Rows);

    addCell(HeaderRow, 0, TimeName, false, "ms", true);
    addCell(HeaderRow, TimeName, TimeColumn, true, "min", true);
    addCell(HeaderRow, TimeName+TimeColumn, TimeColumn, true, "avg", true);
    addCell(HeaderRow, TimeName+2*TimeColumn, TimeColumn, true, "max", true);

    const struct { Row row; const char *name; } times[]=
    {
        {FrameRow, "frame"}, {CpuRow, "cpu"}, {UpdateRow, "update"}, {RenderRow, "render"},
        {FlushRow, "flush"},
    };

    for(const auto &time : times)
    {
        addCell(time.row, 0, TimeName, false, time.name, true);
        for(int column=0; column < 3; ++column)
            addCell(time.row, TimeName+column*TimeColumn, TimeColumn, true, "-", false);
    }

    const struct { Row row; const char *first; const char *second; } counters[]=
    {
        {FpsRow, "fps", "controls"}, {IdsRow, "prim ids", "text ids"},
        {RenderedRow, "rendered", "changed"}, {LayoutRow, "passes", "entered"},
        {ZRow, "z visited", "anims"}, {SlotsRow, "prim slots", "glyphs"},
        {UploadsRow, "upload B", "draws"},
    };

    for(const auto &counter : counters)
    {
        addCell(counter.row, 0, Name1, false, counter.first, true);
        addCell(counter.row, Name1, Value1, true, "-", false);
        addCell(counter.row, Name2X, Name2, false, counter.second, true);
        addCell(counter.row, Value2X, Value2, true, "-", false);
    }
    addCell(MaxHeaderRow, 0, ContentWidth, false, "most in one frame", true);

    int y=Pad+RowCount*RowHeight+PlotGap;

    for(controls::PlotLines **plot : {&m_cpuPlot, &m_slotsPlot})
    {
        *plot=newChild<controls::PlotLines>();
        (*plot)->setBounds(Pad, y, ContentWidth, PlotHeight);
        (*plot)->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        (*plot)->setMouseInputEnabled(false);
        (*plot)->setKeyboardInputEnabled(false);
        (*plot)->setTabable(false);
        // the segments exist from the start: a refresh only moves them
        (*plot)->setValues(std::vector<float>(PlotSamples, 0.0f));
        y+=PlotHeight+PlotGap;
    }
    m_cpuPlot->setCaption("cpu ms");
    m_slotsPlot->setCaption("slots / frame");

#if GWENI_ANIMATE
    anim::add(this, new RefreshAnimation<StatsOverlay>());
#endif
}

controls::Label *StatsOverlay::addCell(int row, int x, int width, bool right, const String &text, bool dark)
{
    controls::Label *label=newChild<controls::Label>();

    label->setBounds(Pad+x, Pad+row*RowHeight, width, RowHeight);
    label->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    label->setMouseInputEnabled(false);
    label->setKeyboardInputEnabled(false);
    label->setTabable(false);
    if(right)
        label->setTextAlignment(Alignment::Right|Alignment::CenterV);
    if(dark)
        label->makeColorDark();
    label->setText(text);
    m_rows[row].push_back(label);
    return label;
}

StatsOverlay *StatsOverlay::find(controls::Canvas *canvas)
{
    if(!canvas)
        return nullptr;
    for(controls::Base *child : canvas->getActualChildren())
    {
        StatsOverlay *overlay=gweni_cast<StatsOverlay>(child);

        if(overlay && !overlay->m_closing)
            return overlay;
    }
    return nullptr;
}

StatsOverlay *StatsOverlay::open(controls::Canvas *canvas)
{
    if(StatsOverlay *overlay=find(canvas))
        return overlay;
    if(!canvas)
        return nullptr;

    StatsOverlay *overlay=canvas->newChild<StatsOverlay>();

    canvas->addOverlay(overlay);
    overlay->refresh();

    event::Info info(canvas);

    info.integer=1;
    onOpenChangedCaller.call(overlay, info);
    return overlay;
}

void StatsOverlay::close(controls::Canvas *canvas)
{
    if(StatsOverlay *overlay=find(canvas))
    {
        overlay->m_closing=true;
        overlay->hide();
        overlay->delayedDelete();

        event::Info info(canvas);

        info.integer=0;
        onOpenChangedCaller.call(overlay, info);
    }
}

void StatsOverlay::setBottomReserve(controls::Canvas *canvas, int units)
{
    if(!canvas)
        return;
    units=std::max(0, units);
    if(bottomReserve(canvas) == units)
        return;

    if(units == 0)
        bottomReserves().erase(canvas);
    else
        bottomReserves()[canvas]={canvas->getControlId(), units};

    // the overlay alone places itself again: it is owner-placed, so nothing
    // else's layout reads where it is
    if(StatsOverlay *overlay=find(canvas))
        overlay->invalidate();
}

int StatsOverlay::bottomReserve(controls::Canvas *canvas)
{
    if(!canvas)
        return 0;

    auto &reserves=bottomReserves();
    const auto entry=reserves.find(canvas);

    if(entry == reserves.end())
        return 0;
    // left by a deleted canvas at this address
    if(entry->second.canvasId != canvas->getControlId())
    {
        reserves.erase(entry);
        return 0;
    }
    return entry->second.units;
}

bool StatsOverlay::toggle(controls::Canvas *canvas)
{
    if(find(canvas))
    {
        close(canvas);
        return false;
    }
    return open(canvas) != nullptr;
}

controls::Base *StatsOverlay::getControlAt(int /*x*/, int /*y*/, bool /*bOnlyIfMouseEnabled*/)
{
    // the app's hit tests pass through it as if it were not there: no visit
    return nullptr;
}

void StatsOverlay::arrange(skin::Base *skin, Dim dim)
{
    if(controls::Base *parent=getParent())
    {
        policyResize(getPreferredSize(), dim);

        if(dim == Dim::X)
            setArrangedBounds(std::max(0, parent->getWidth()-getWidth()-Inset), getY(), getWidth(), getHeight());
        else
        {
            // the reserve is the canvas's, for its own overlay: not one on a
            // page of the app's
            controls::Canvas *canvas=getCanvas();
            const int reserve=(parent == canvas)?bottomReserve(canvas):0;

            setArrangedBounds(getX(), std::max(0, parent->getHeight()-getHeight()-Inset-reserve),
                getWidth(), getHeight());
        }
    }

    ParentClass::arrange(skin, dim);
}

void StatsOverlay::ownIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const
{
    appendSubtreeIds(this, primitiveIds, textIds);
}

std::vector<String> StatsOverlay::lines() const
{
    std::vector<String> lines;

    for(const std::vector<controls::Label *> &row : m_rows)
    {
        String line;

        for(const controls::Label *label : row)
            line+=(line.empty()?"":" | ")+label->getText();
        lines.push_back(line);
    }
    return lines;
}

void StatsOverlay::setCell(controls::Label *label, const String &text)
{
    if(setLabelText(label, text))
        ++m_textSets;
}

size_t StatsOverlay::countControls(const controls::Base *control)
{
    size_t controls=0;

    for(const controls::Base *child : control->getActualChildren())
    {
        if(!child->isExcludedFromStats())
            controls+=1+countControls(child);
    }
    return controls;
}

void StatsOverlay::refresh()
{
    controls::Canvas *canvas=getCanvas();

    if(!canvas)
        return;
    // hidden (on a hidden page, or closed until its delayed delete), it neither
    // gathers nor shows, and starts again from the frame it is shown in
    if(!visible())
    {
        m_window=Window();
        m_haveLastStart=false;
        m_frameClock.clear();
        m_started=false;
        return;
    }

    gather(canvas);

    const float now=platform::getTimeInSeconds();

    // A clock that went back (a capture installing its stepped clock after the
    // overlay was opened on the platform's) starts the second and the window
    // again from now.
    if(!m_frameClock.empty() && (now < m_frameClock.back()))
        m_frameClock.clear();
    if(m_started && (m_nextRefresh > now+RefreshSeconds))
        m_nextRefresh=now+RefreshSeconds;

    // the frames of the last second, by the canvas's clock
    m_frameClock.push_back(now);
    while(!m_frameClock.empty() && (m_frameClock.front() <= now-1.0f))
        m_frameClock.pop_front();

    if(!m_started)
    {
        m_started=true;
        m_nextRefresh=now+RefreshSeconds;
        show(canvas);
        return;
    }
    if(now < m_nextRefresh)
        return;
    // one step, not a loop: once the float clock is coarser than a step (days
    // in, or an app's time source counting from an epoch) adding one changes
    // nothing, and after a stall the next refresh is a step from now
    m_nextRefresh+=RefreshSeconds;
    if(m_nextRefresh <= now)
        m_nextRefresh=now+RefreshSeconds;
    show(canvas);
}

void StatsOverlay::gather(controls::Canvas *canvas)
{
    const controls::CanvasFrameStats &frame=canvas->previousFrameStats();
    const controls::FrameTimes &times=frame.times;

    // nothing before the canvas's second frame, and each frame once
    if((times.startMicros == 0) || (m_haveLastStart && (times.startMicros == m_lastStart)))
        return;

    Window &window=m_window;

    if(m_haveLastStart && (times.startMicros > m_lastStart))
        window.frame.add(times.startMicros-m_lastStart);
    m_lastStart=times.startMicros;
    m_haveLastStart=true;

    window.update.add(times.updateMicros);
    window.render.add((times.renderMicros > times.flushMicros)?times.renderMicros-times.flushMicros:0);
    window.flush.add(times.flushMicros);
    window.cpu.add(times.updateMicros+times.renderMicros);

    window.rendered=std::max(window.rendered, frame.render.visited);
    window.changed=std::max(window.changed, frame.render.controls);
    window.layoutPasses=std::max(window.layoutPasses, size_t(std::max(0, frame.layout.passes)));
    window.layoutEntries=std::max(window.layoutEntries, frame.layout.visited);
    window.zVisited=std::max(window.zVisited, frame.update.zVisited);
    window.animsRun=std::max(window.animsRun, frame.update.animsRun);

    renderer::Base *render=getSkin()?getSkin()->getRenderer():nullptr;
    const AppFrameWrites writes=m_ownSlots.appWrites(render, {this});
    const renderer::FrameCounts counts=render?render->frameStats():renderer::FrameCounts();

    window.counted=writes.counted;
    window.primitiveSlots=std::max(window.primitiveSlots, writes.primitiveSlots);
    window.glyphSlots=std::max(window.glyphSlots, writes.glyphSlots);
    window.slots=std::max(window.slots, writes.primitiveSlots+writes.glyphSlots);
    window.uploadBytes=std::max(window.uploadBytes, writes.uploadBytes);
    window.drawCalls=std::max(window.drawCalls, counts.drawCalls);
    ++window.frames;
}

void StatsOverlay::show(controls::Canvas *canvas)
{
    m_shown=m_window;
    m_window=Window();
    ++m_refreshes;

    m_shownFps=0.0f;
    if(m_frameClock.size() >= 2)
    {
        const float span=m_frameClock.back()-m_frameClock.front();

        if(span > 0.0f)
            m_shownFps=float(m_frameClock.size()-1)/span;
    }

    // a walk of the tree only after it changed
    if(!m_counted || (canvas->treeSerial() != m_treeSerial))
    {
        m_treeSerial=canvas->treeSerial();
        m_counted=true;
        m_controls=countControls(canvas);
    }

    std::vector<size_t> ownPrimitives;
    std::vector<size_t> ownTexts;

    ownIds(ownPrimitives, ownTexts);

    const size_t livePrimitives=primitiveIdCount()-releasedPrimitiveIdCount();
    const size_t liveTexts=textIdCount()-releasedTextIdCount();

    m_shownPrimitiveIds=(livePrimitives > ownPrimitives.size())?livePrimitives-ownPrimitives.size():0;
    m_shownTextIds=(liveTexts > ownTexts.size())?liveTexts-ownTexts.size():0;

    const Window &shown=m_shown;
    const struct { Row row; const TimeRange &range; } times[]=
    {
        {FrameRow, shown.frame}, {CpuRow, shown.cpu}, {UpdateRow, shown.update},
        {RenderRow, shown.render}, {FlushRow, shown.flush},
    };

    for(const auto &time : times)
    {
        const std::vector<controls::Label *> &cells=m_rows[time.row];
        const bool any=(time.range.count != 0);

        setCell(cells[1], any?ms(time.range.min):"-");
        setCell(cells[2], any?ms(time.range.avg()):"-");
        setCell(cells[3], any?ms(time.range.max):"-");
    }

    auto pair=[this](Row row, const String &first, const String &second)
    {
        setCell(m_rows[row][1], first);
        setCell(m_rows[row][3], second);
    };
    const bool counted=shown.counted;
    const String none="-";

    pair(FpsRow, format("%.1f", double(m_shownFps)), count(m_controls));
    pair(IdsRow, count(m_shownPrimitiveIds), count(m_shownTextIds));
    pair(RenderedRow, count(shown.rendered), count(shown.changed));
    pair(LayoutRow, count(shown.layoutPasses), count(shown.layoutEntries));
    pair(ZRow, count(shown.zVisited), count(shown.animsRun));
    pair(SlotsRow, counted?count(shown.primitiveSlots):none, counted?count(shown.glyphSlots):none);
    pair(UploadsRow, counted?count(shown.uploadBytes):none, counted?count(shown.drawCalls):none);

    m_cpuPlot->push(float(double(shown.cpu.max)/1000.0));
    m_slotsPlot->push(float(shown.slots));
}

}//namespace tools
}//namespace gweni
