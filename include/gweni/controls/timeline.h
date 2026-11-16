/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_timeline_h_
#define _gweni_controls_timeline_h_

#include <gweni/controls/laneView.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace gweni
{
namespace controls
{
namespace internal
{
//! The Timeline's body and caption column are the LaneView's.
typedef LaneBody TimelineBody;
typedef LaneCaptionColumn TimelineCaptionColumn;
}//namespace internal

//! A bar of a Timeline lane: a time range in epoch seconds, drawn in a colour
//! of its own or a theme surface, and an id of the app's (a task's, say),
//! which onBarClicked hands back.
struct TimelineBar
{
    double start=0.0;
    double end=0.0;
    uint64_t id=0;
    //! The theme surface the bar is drawn in, "TimelineBar" when empty;
    //! hasColor draws it in color instead.
    String surface;
    bool hasColor=false;
    Color color;

    TimelineBar() {}
    TimelineBar(double start_, double end_, uint64_t id_=0): start(start_), end(end_), id(id_) {}

    //! The same bar drawn in \p c.
    TimelineBar &withColor(const Color &c) { hasColor=true; color=c; return *this; }
    //! The same bar drawn in the theme surface \p name.
    TimelineBar &withSurface(const String &name) { hasColor=false; surface=name; return *this; }

    bool operator==(const TimelineBar &other) const
    {
        return start == other.start && end == other.end && id == other.id && surface == other.surface
            && hasColor == other.hasColor && (!hasColor || color == other.color);
    }
    bool operator!=(const TimelineBar &other) const { return !(*this == other); }
};

//! A Gantt-style timeline: lanes, one row each, of time-range bars, with the
//! lanes' captions in a fixed column at the left and a time axis above the
//! bars.
//!
//! A lane holds its bars in the order they were added: a bar's index is its
//! place in that order (removeBar() moves the later ones down one). The app
//! packs the bars so that they do not overlap in a lane; where two do, the
//! one added later is on top: drawn over the other and the one the hit test
//! picks. Each bar has an overlap level, 0 for a bar no earlier bar of its
//! lane overlaps in time and otherwise one more than the highest level of the
//! earlier bars it overlaps, MaxOverlapLevel at most; the renderer draws a
//! level in a depth layer of its own, so the later bar is in front. Bars
//! sharing a level (more than MaxOverlapLevel+1 of them overlapping at once,
//! or two packed bars whose rounded rects both reach one column) share a
//! layer, where the earlier bar's slot, drawn first, is the one the depth
//! test keeps: there the earlier bar is on top, drawn and picked. The levels
//! are worked out lane by lane on the first use after a change, each bar
//! against the earlier bars whose ends reach its start: O(n log n) for a
//! packed lane, O(n^2) at worst (every bar under one long one).
//!
//! The x range is setXRange()'s, or automatic: from the earliest start to the
//! latest end of every bar (half a second each way round a single instant,
//! 0 to 1 with no bar). The time axis's ticks and labels are axis::'s
//! (chooseTimeTicks, in the zone of setUtcOffset()), as a Chart's time axis.
//!
//! The lanes, their captions, the axis row, the scrolled body and the
//! geometry are the LaneView's (see it), shared with the Sequencer; the bar
//! area is its lane area. A bar is BarInset in from its lane's top and
//! bottom, from its start's x to its end's x (a unit wide at least). Each
//! lane's bars are searched through an internal::LaneBarIndex (sorted by
//! start, the running maximum of the ends, the overlap levels).
//!
//! A change of a bar marks StateChange_Background, and lays the timeline out
//! again only when it moves an automatic range.
class GWENI_EXPORT Timeline: public LaneView
{
public:

    GWENI_CONTROL(Timeline, LaneView);

public:

    ~Timeline() override;

    //! A bar's inset from its lane's top and bottom.
    static constexpr int BarInset=3;
    //! The overlap levels: 0 to this (the renderer's layers for bars).
    static constexpr int MaxOverlapLevel=3;
    //! How far outside the bar area a bar's edge is held before rounding, so
    //! a bar too far out for an int is cut as a near one is, never wrapped;
    //! more than a bar's corner, which the area's clip then cuts off.
    static constexpr int OffAreaMargin=8;

    // ---- lanes (LaneView's, with their bars) ------------------------------

    //! Adds a lane, last, and returns its index.
    size_t addLane(const String &caption);
    //! Removes lane \p lane and its bars; the lanes after it move up one.
    void removeLane(size_t lane);
    void clearLanes();
    void setLaneCaption(size_t lane, const String &caption);

    // ---- bars -----------------------------------------------------------

    //! Adds a bar to \p lane, last, and returns its index there (an end
    //! before its start is swapped with it); a lane past the lanes adds
    //! nothing and returns -1 as a size_t.
    size_t addBar(size_t lane, TimelineBar bar);
    //! Gives bar \p index of \p lane new values; out of range is ignored.
    void setBar(size_t lane, size_t index, TimelineBar bar);
    //! Removes bar \p index; the later ones move down an index.
    void removeBar(size_t lane, size_t index);
    void clearBars(size_t lane);
    size_t getBarCount(size_t lane) const;
    //! Every lane's bars.
    size_t getTotalBarCount() const { return m_barCount; }
    //! The bar, or an empty one out of range.
    const TimelineBar &getBar(size_t lane, size_t index) const;
    //! The bar's overlap level (see the class comment), 0 out of range.
    int getOverlapLevel(size_t lane, size_t index) const;

    // ---- the x axis -----------------------------------------------------

    //! A fixed x range (a max not above min is taken as min+1).
    void setXRange(double min, double max);
    //! Back to the automatic x range.
    void setXAutoRange();
    bool hasFixedXRange() const { return m_xFixed; }
    //! The x range drawn, fixed or automatic.
    double getXMin() const override;
    double getXMax() const override;

    //! The time axis's zone (axis::localUtcOffset() by default); an empty one
    //! is UTC.
    void setUtcOffset(const axis::UtcOffset &offset);
    const axis::UtcOffset &getUtcOffset() const { return m_utcOffset; }

    // ---- hit test, in this control's own coordinates ----------------------

    //! The bar under \p local: in the lane at its y, a bar whose rect
    //! (getBarRect()) holds its x, the highest overlap level and then the
    //! earliest of those (the one drawn on top in a shared layer); false
    //! outside the bar area or over none. LaneBarIndex::pick(): a binary
    //! search on the lane's bars sorted by start, walking back over the bars
    //! starting before the point while the running maximum of their ends
    //! still reaches it: O(log n) plus one or two bars in a packed lane, but
    //! every bar starting before the point when one long early bar reaches it.
    bool barAt(const Point &local, size_t &lane, size_t &index) const;

    // ---- hover and click ------------------------------------------------

    //! The tooltip's text for the bar under the mouse; without one the
    //! timeline's own (defaultHoverText()).
    typedef std::function<String(const Timeline &timeline, size_t lane, size_t index)> HoverText;
    //! Fills a control (a RichText, a Base of Labels) for the bar under the
    //! mouse, shown in the canvas's ToolTip box in place of the text.
    typedef std::function<void(const Timeline &timeline, size_t lane, size_t index, Base *control)> HoverFill;

    //! The bar under the mouse (barAt()), and its rect as drawn: false with
    //! none. The renderer draws its hover look over it (one slot of its own,
    //! the bars' slots kept), and the tooltip is set only when the hovered
    //! bar changes, so a move within a bar writes nothing.
    bool isHovering() const { return m_hovering; }
    size_t getHoverLane() const { return m_hoverLane; }
    size_t getHoverIndex() const { return m_hoverIndex; }
    bool getHoverRect(Rect &rect) const;
    //! The tooltip's text (a null one for the timeline's own), or a control
    //! to fill (made with the skin, e.g. by the timeline's newChild()), which
    //! the timeline takes as its hidden child and its setToolTip() while a bar
    //! is hovered, deleting the one before; null for the text again. A change
    //! of the bars under the mouse fills it again.
    void setHoverText(const HoverText &text);
    void setHoverControl(Base *control, const HoverFill &fill);
    //! The lane's caption and the bar's start and end ("host-01  Nov 14
    //! 12:00 - Nov 14 13:30", axis::formatDateTime in the timeline's zone).
    String defaultHoverText(size_t lane, size_t index) const;

    //! Fired on a left click (the press and the release on the same bar,
    //! barAt(), with the same id) with the lane in info.point.x, the bar's
    //! index in info.point.y and its id in info.integer (cut to a size_t);
    //! also getClickedLane(), getClickedIndex() and getClickedId().
    gweni::event::Caller onBarClicked;
    size_t getClickedLane() const { return m_clickedLane; }
    size_t getClickedIndex() const { return m_clickedIndex; }
    uint64_t getClickedId() const { return m_clickedId; }

    //! The hovered bar as drawn (cut to the bar area), in canvas
    //! coordinates: the tooltip goes below it.
    gweni::Rect getToolTipAnchor() const override;

    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void onMouseLeave() override;
    void onMouseClickLeft(int x, int y, bool down) override;

    //! Bumped by every change that can move a bar's slot (the bars, the
    //! lanes, the range, the layout, the scroll), not by the hover: the
    //! renderer draws the bars again only when it moved.
    unsigned getBarsSerial() const { return m_barsSerial; }

    // ---- geometry, in this control's own coordinates, as last laid out ----

    //! The bar's rect, whole units, its edges held OffAreaMargin outside the
    //! bar area but not cut to it (the renderer clips to the area): false
    //! for a bar out of range, of a lane out of view, wholly outside the
    //! area, or with a start or end that is not finite.
    bool getBarRect(size_t lane, size_t index, Rect &rect) const;

    //! The time step of the ticks, as last laid out.
    axis::TimeStep getTimeStep() const { return m_timeStep; }

protected:

    //! The time ticks (axis::chooseTimeTicks in the timeline's zone).
    std::vector<axis::Tick> chooseTicks(double min, double max, int width) override;
    //! The bars moved on screen: barsMoved() and the hover found again.
    void laneContentMoved() override;
    //! The lanes changed: laid out again, the bars drawn again and the hover
    //! filled again.
    void lanesChanged() override;
    //! A shown tooltip of the lane's set again (the default one names it).
    void laneCaptionChanged(size_t lane) override;
    int minLaneHeight() const override { return 2*BarInset+1; }

    struct Lane
    {
        //! in the order added
        std::vector<TimelineBar> bars;
        //! derived from the bars (ensureLane)
        mutable internal::LaneBarIndex index;
    };

    const Lane *laneAtIndex(size_t lane) const { return lane < m_lanes.size() ? &m_lanes[lane] : nullptr; }
    void ensureLane(const Lane &lane) const;
    //! A bar of \p lane changed: marks the timeline, and the lane's derived
    //! data; \p moved the range from \p before (no bar for an added one) to
    //! \p after (none for a removed one).
    void barsChanged(Lane &lane, const TimelineBar *before, const TimelineBar *after);
    void ensureRange() const;

    //! The rect of bar \p bar in lane \p laneIndex, unclipped (see
    //! getBarRect()), without the view checks.
    bool barRect(size_t laneIndex, const TimelineBar &bar, Rect &rect) const;

    //! Something a bar's slot is drawn from changed: marks the timeline and
    //! bumps getBarsSerial().
    void barsMoved();
    //! The hover at the mouse's last place over the timeline (none off it),
    //! the tooltip brought up to it when it changed; with \p refill it is
    //! filled again even for the same bar: the bars under it changed.
    void setHover(bool hovering, size_t lane, size_t index, bool refill=false);
    void refreshHover(bool refill=false);

    bool m_hovering=false;
    size_t m_hoverLane=0;
    size_t m_hoverIndex=0;
    //! where the mouse last was over the timeline, in its own coordinates
    bool m_mouseInside=false;
    Point m_mouse;
    HoverText m_hoverText;
    Base *m_hoverControl=nullptr;
    HoverFill m_hoverFill;
    bool m_pressed=false;
    size_t m_pressLane=0;
    size_t m_pressIndex=0;
    uint64_t m_pressId=0;
    size_t m_clickedLane=0;
    size_t m_clickedIndex=0;
    uint64_t m_clickedId=0;
    unsigned m_barsSerial=0;

    std::vector<Lane> m_lanes;
    size_t m_barCount=0;

    bool m_xFixed=false;
    double m_xFixedMin=0.0;
    double m_xFixedMax=1.0;
    axis::UtcOffset m_utcOffset;

    //! the automatic range (ensureRange)
    mutable bool m_rangeDirty=true;
    mutable bool m_hasBars=false;
    mutable double m_dataMin=0.0;
    mutable double m_dataMax=1.0;

    //! as last laid out
    axis::TimeStep m_timeStep=axis::TimeStep::Hour1;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_timeline_h_
