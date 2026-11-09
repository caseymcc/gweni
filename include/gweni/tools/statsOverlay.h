/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_tools_statsOverlay_h_
#define _gweni_tools_statsOverlay_h_

#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/plotLines.h>
#include <gweni/tools/toolSupport.h>

#include <cstdint>
#include <deque>
#include <vector>

namespace gweni
{
namespace tools
{

//! A read-only panel in the canvas's bottom-right corner showing what the
//! canvas's frames cost (docs/architecture.md, "The stats overlay"):
//! - fps, over the last second of the canvas's clock;
//! - each phase's time on platform::profileTimeMicros(), min/avg/max in ms
//!   over the frames since the last refresh: the frame (one updateCanvas() to
//!   the next), cpu (update + render + flush), update, render without the
//!   flush, and the flush (CanvasFrameStats::times);
//! - the tree: the app's controls (the tools' subtrees left out), and its live
//!   primitive and text ids;
//! - the most one frame since the last refresh did: controls rendered and
//!   changed, layout passes and entries, z visited, animations run, the app's
//!   primitive and glyph slots written, upload bytes and draw calls;
//! - two plots of the last PlotSamples refreshes: the most cpu time one frame
//!   of each took, and the most slots (primitive and glyph) one frame wrote.
//!
//! It gathers every frame, from an animation of its own run at the start of
//! the canvas's update phase (the frame before it: previousFrameStats() and the
//! renderer's frameStats()), and refreshes what it shows every RefreshSeconds
//! of the canvas's clock (platform::getTimeInSeconds()), setting a label only
//! when its string changes. Its rows have fixed sizes, so a refresh changes
//! text and never the panel's size.
//!
//! Like the Inspector it shows the app's work alone: it is
//! setExcludedFromStats(), so none of its controls, entries or animations
//! count in the canvas's counters, and the slots it wrote are taken out of the
//! renderer's (OwnSlots). The times cannot be split per control: they include
//! its own refreshes, and the draw calls include its own draws.
//!
//! It takes no input: a hit test passes through it and its children
//! (getControlAt() finds nothing, and counts no visit), so the app under it
//! keeps its hover and clicks. It places itself (Placement::Owner) Inset units
//! in from the canvas's bottom-right corner on every layout pass, and is a
//! canvas overlay (Canvas::addOverlay), numbered above the app. The library
//! binds no key: an app opens and closes it (open(), close(), toggle()).
class GWENI_EXPORT StatsOverlay: public controls::Base
{
public:
    GWENI_CONTROL(StatsOverlay, controls::Base);
public:
    //! The canvas's overlay, made and shown if it has none.
    static StatsOverlay *open(controls::Canvas *canvas);
    //! Deletes the canvas's overlay (delayed), if it has one.
    static void close(controls::Canvas *canvas);
    //! Opens the overlay, or closes it if it is open. Returns whether it is open.
    static bool toggle(controls::Canvas *canvas);
    //! The canvas's overlay, or null.
    static StatsOverlay *find(controls::Canvas *canvas);

    //! How often what it shows is refreshed, in seconds of the canvas's clock.
    static constexpr float RefreshSeconds=0.25f;
    //! The refreshes each plot holds: 15 s.
    static constexpr size_t PlotSamples=60;
    //! Its distance from the canvas's right and bottom edges, in units.
    static constexpr int Inset=8;

    //! One time over a window of frames, in microseconds.
    struct TimeRange
    {
        uint64_t min=0;
        uint64_t max=0;
        uint64_t sum=0;
        size_t count=0;

        void add(uint64_t micros);
        uint64_t avg() const { return count?sum/count:0; }
    };

    //! What the frames of one refresh window did: their times, and the most
    //! one of them did of each counter.
    struct Window
    {
        size_t frames=0;
        TimeRange frame;    //!< one updateCanvas() to the next
        TimeRange cpu;      //!< update + render (the flush included)
        TimeRange update;
        TimeRange render;   //!< render without the flush
        TimeRange flush;
        size_t rendered=0;          //!< ChangeStats::visited
        size_t changed=0;           //!< ChangeStats::controls
        size_t layoutPasses=0;
        size_t layoutEntries=0;     //!< LayoutStats::visited
        size_t zVisited=0;
        size_t animsRun=0;
        size_t primitiveSlots=0;    //!< the app's (OwnSlots::appWrites())
        size_t glyphSlots=0;
        size_t slots=0;             //!< primitive + glyph slots in one frame
        size_t uploadBytes=0;       //!< the app's (OwnSlots::appWrites())
        size_t drawCalls=0;         //!< renderer::FrameCounts::drawCalls
        bool counted=false;         //!< the renderer counts its slots
    };

    //! Gathers the frame before this one into the current window, and, every
    //! RefreshSeconds, shows it and starts the next. Its animation calls it once
    //! a frame, at the start of the canvas's update phase; without
    //! GWENI_ANIMATE the app calls it.
    void refresh();

    //! The window the panel shows now (the last one refreshed), and its fps.
    const Window &shownWindow() const { return m_shown; }
    float shownFps() const { return m_shownFps; }
    //! The tree's numbers it shows now.
    size_t shownControls() const { return m_controls; }
    size_t shownPrimitiveIds() const { return m_shownPrimitiveIds; }
    size_t shownTextIds() const { return m_shownTextIds; }
    //! The refreshes so far, and the Label setText() calls they made.
    size_t refreshes() const { return m_refreshes; }
    size_t textSets() const { return m_textSets; }
    //! Every row's text, its cells joined with " | ", in order.
    std::vector<String> lines() const;

    controls::PlotLines *cpuPlot() const { return m_cpuPlot; }
    controls::PlotLines *slotsPlot() const { return m_slotsPlot; }

    //! The primitive and text ids its own controls hold.
    void ownIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const;

    //! It takes no input: nothing of it is ever under the mouse.
    controls::Base *getControlAt(int x, int y, bool bOnlyIfMouseEnabled=true) override;
    //! Its size is the one its rows make, set when it is made.
    bool sizedByRequest(Dim /*dim*/) const override { return true; }

protected:
    //! Takes its size, and the place Inset in from the canvas's bottom-right
    //! corner, one axis per arrange, as a Popup does next to its anchor.
    void arrange(skin::Base *skin, Dim dim) override;

private:
    //! The rows: a time row has a name and min, avg and max columns, a counter
    //! row two name/value pairs.
    enum Row
    {
        HeaderRow, FrameRow, CpuRow, UpdateRow, RenderRow, FlushRow,
        FpsRow, IdsRow, MaxHeaderRow, RenderedRow, LayoutRow, ZRow, SlotsRow, UploadsRow,
        Rows
    };

    controls::Label *addCell(int row, int x, int width, bool right, const String &text, bool dark);
    void setCell(controls::Label *label, const String &text);
    //! The frame before this one, into m_window.
    void gather(controls::Canvas *canvas);
    //! m_window onto the labels and the plots.
    void show(controls::Canvas *canvas);
    //! The canvas's controls but the tools' (setExcludedFromStats()) subtrees.
    static size_t countControls(const controls::Base *control);

    //! Each row's cells, left to right.
    std::vector<std::vector<controls::Label *>> m_rows;
    controls::PlotLines *m_cpuPlot=nullptr;
    controls::PlotLines *m_slotsPlot=nullptr;

    Window m_window;
    Window m_shown;
    OwnSlots m_ownSlots;
    //! The previous frame's start, to time the interval to the next.
    uint64_t m_lastStart=0;
    bool m_haveLastStart=false;
    //! The canvas's clock at each frame gathered in the last second, for fps.
    std::deque<float> m_frameClock;
    float m_shownFps=0.0f;
    float m_nextRefresh=0.0f;
    bool m_started=false;
    unsigned m_treeSerial=0;
    bool m_counted=false;
    size_t m_controls=0;
    size_t m_shownPrimitiveIds=0;
    size_t m_shownTextIds=0;
    size_t m_refreshes=0;
    size_t m_textSets=0;
};

}//namespace tools
}//namespace gweni

#endif//_gweni_tools_statsOverlay_h_
