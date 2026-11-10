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
//! - fps, over the last second of the canvas's clock, from the frames it saw
//!   move on (a clock that stands still counts no frame);
//! - each phase's time on platform::profileTimeMicros(), min/avg/max in ms
//!   (the "time (ms)" corner) over the frames since the last refresh: the
//!   interval (one updateCanvas() to the next), cpu (update + render + flush),
//!   update, render without the flush, and the flush (CanvasFrameStats::times);
//! - the tree: the app's controls (the tools' subtrees left out), and its live
//!   primitive and text ids (the live ids less those every tool on the canvas
//!   holds, Canvas::excludedSlots());
//! - the most one frame since the last refresh did: controls rendered and
//!   changed, layout passes and entries, z visited, animations run, the app's
//!   primitive and glyph slots written, upload bytes (with their unit,
//!   bytes()) and draw calls;
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
//! count in the canvas's counters, and the slots every tool on the canvas (an
//! Inspector too) wrote are taken out of the renderer's (appFrameWrites(),
//! over Canvas::excludedSlots()). The times cannot be split per control: they include
//! its own refreshes, and the draw calls include its own draws.
//!
//! Only its name cells take the mouse, for their tooltips: each row and
//! column name (nameCells()) has one saying what it shows, and getControlAt()
//! finds the name cell under the point over the cells' fixed rects, counting
//! no visit. Everywhere else (the values, the plots, the gaps, the
//! background) a hit test passes through it, so the app under it keeps its
//! hover and clicks; a click on a name does nothing. The canvas shows a
//! name's tooltip in its ToolTip box, excluded from the counts as a tool's
//! (Canvas, "Tooltip"), and the box's slots are taken out as its own while it
//! holds one of them (ownIds()). It places itself (Placement::Owner) Inset units
//! in from its parent's bottom-right corner on every layout pass; open() makes
//! it on the canvas, as a canvas overlay (Canvas::addOverlay), numbered above
//! the app. Made as any other control's child (a demo page's), it is that
//! control's. Hidden, it neither gathers nor shows. The library binds no key:
//! an app opens and closes it (open(), close(), toggle()), and hears of every
//! open and close through onOpenChangedCaller (a toggle button kept in step).
//! An app with a bar along the canvas's bottom keeps the canvas's overlay
//! above it with setBottomReserve().
//!
//! Its counters and times need Canvas::updateCanvas() and renderCanvas(),
//! which record previousFrameStats() and its times: on a
//! controls::WindowCanvas, which runs neither, it shows '-' and 0.
class GWENI_EXPORT StatsOverlay: public controls::Base
{
public:
    GWENI_CONTROL(StatsOverlay, controls::Base);
public:
    //! The canvas's overlay, made and shown if it has none. It first gathers
    //! and shows in the next update phase (its first refresh, a window of the
    //! frame before it), where the canvas's previous frame and the renderer's
    //! writes are one frame's.
    static StatsOverlay *open(controls::Canvas *canvas);
    //! Hides the canvas's overlay and deletes it (delayed), if it has one;
    //! from then on find() no longer finds it.
    static void close(controls::Canvas *canvas);
    //! close() if find() finds one, else open(). Returns whether it is open.
    static bool toggle(controls::Canvas *canvas);
    //! The canvas's overlay, or null; one closed and waiting for its delayed
    //! delete is not found (Canvas::isPendingDelete()), so toggle() or open()
    //! after close() in one frame makes a new one.
    static StatsOverlay *find(controls::Canvas *canvas);

    //! Called when a canvas's overlay opens or closes: by open() when it makes
    //! one and by close() when it closes one (so by toggle() too), after
    //! find() gives the new answer; never for an overlay made as another
    //! control's child, nor by open() or close() when there was nothing to do.
    //! The info's control is the canvas (info.control->getCanvas() is it) and
    //! its integer 1 for an open, 0 for a close; the caller is the overlay. A handler sees every canvas's, and
    //! one deleted is dropped (event::Handler).
    static event::Caller onOpenChangedCaller;

    //! Keeps \p units clear below the canvas's overlay, above its Inset (a
    //! status bar's height): its bottom edge is Inset+units in from the
    //! canvas's. Kept for the canvas, so an overlay opened later takes it too;
    //! 0, the default, forgets it. Only an overlay on the canvas itself
    //! (open()'s) reads it, and a change lays out that overlay alone; set
    //! from the app's layout (an arrange placing its bar), it is in effect in
    //! that pass, which reaches the overlay after the app's controls. The
    //! canvas is told apart by its control id, never reused, so a canvas made
    //! later at a deleted one's address starts at 0; an app that sets it
    //! gives it back (0) when it goes.
    static void setBottomReserve(controls::Canvas *canvas, int units);
    static int bottomReserve(controls::Canvas *canvas);

    //! How often what it shows is refreshed, in seconds of the canvas's clock.
    static constexpr float RefreshSeconds=0.25f;
    //! The refreshes each plot holds: 15 s.
    static constexpr size_t PlotSamples=60;
    //! Its distance from the canvas's right and bottom edges, in units.
    static constexpr int Inset=8;
    //! The most frame times fps is counted from (frameClockSize()).
    static constexpr size_t MaxFrameClock=1024;

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
        size_t primitiveSlots=0;    //!< the app's (appFrameWrites())
        size_t glyphSlots=0;
        size_t slots=0;             //!< primitive + glyph slots in one frame
        size_t uploadBytes=0;       //!< the app's (appFrameWrites())
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
    //! The canvas-clock times fps is counted from: one per frame whose clock
    //! moved on in the last second, at most MaxFrameClock.
    size_t frameClockSize() const { return m_frameClock.size(); }
    //! The refreshes so far, and the Label setText() calls they made.
    size_t refreshes() const { return m_refreshes; }
    size_t textSets() const { return m_textSets; }
    //! Every row's text, its cells joined with " | ", in order.
    std::vector<String> lines() const;
    //! The name cells (every cell with a tooltip), row by row, in order.
    std::vector<controls::Label *> nameCells() const;
    //! A byte count in at most 5 characters, as the upload row shows it: as it
    //! is below 1000 ("512B"), else in kB, MB, ... of 1000, with its tenths
    //! below 10 ("1.2MB", "12kB"), truncated.
    static String bytes(size_t n);

    controls::PlotLines *cpuPlot() const { return m_cpuPlot; }
    controls::PlotLines *slotsPlot() const { return m_slotsPlot; }

    //! The primitive and text ids its own controls hold, and the canvas's
    //! ToolTip box's while that shows (or last showed) a name cell's tooltip.
    void ownIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const;

    //! Only its name cells take the mouse, for their tooltips: the visible
    //! name cell under the point, else null, with no hit-test visit.
    controls::Base *getControlAt(int x, int y, bool bOnlyIfMouseEnabled=true) override;
    //! Its size is the one its rows make, set when it is made.
    bool sizedByRequest(Dim /*dim*/) const override { return true; }

protected:
    //! Takes its size, and the place Inset in from its parent's bottom-right
    //! corner (and bottomReserve() more from the bottom on the canvas), one
    //! axis per arrange, as a Popup does next to its anchor.
    void arrange(skin::Base *skin, Dim dim) override;

private:
    //! Its top edge in its parent: Inset up from the bottom, and the canvas's
    //! bottomReserve() more on the canvas.
    int cornerY();

    //! The rows: a time row has a name and min, avg and max columns, a counter
    //! row two name/value pairs.
    enum Row
    {
        HeaderRow, FrameRow, CpuRow, UpdateRow, RenderRow, FlushRow,
        FpsRow, IdsRow, MaxHeaderRow, RenderedRow, LayoutRow, ZRow, SlotsRow, UploadsRow,
        Rows
    };

    //! A cell of \p row; one with a \p toolTip is a name cell, which alone
    //! takes the mouse.
    controls::Label *addCell(int row, int x, int width, bool right, const String &text, bool dark,
                             const String &toolTip=String());
    void setCell(controls::Label *label, const String &text);
    //! The frame before this one, into m_window.
    void gather(controls::Canvas *canvas);
    //! m_window onto the labels and the plots.
    void show(controls::Canvas *canvas);
    //! The canvas's controls but the tools' (setExcludedFromStats()) subtrees.
    static size_t countControls(const controls::Base *control);
    //! The canvas's ToolTip box while it shows (or last showed, until it shows
    //! another control's) a name cell's tooltip, else null.
    const controls::Base *ownToolTipBox() const;

    //! Each row's cells, left to right.
    std::vector<std::vector<controls::Label *>> m_rows;
    controls::PlotLines *m_cpuPlot=nullptr;
    controls::PlotLines *m_slotsPlot=nullptr;

    Window m_window;
    Window m_shown;
    //! The previous frame's start, to time the interval to the next.
    uint64_t m_lastStart=0;
    bool m_haveLastStart=false;
    //! The canvas's clock at each frame gathered in the last second whose
    //! clock moved on from the one before, for fps (at most MaxFrameClock).
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
