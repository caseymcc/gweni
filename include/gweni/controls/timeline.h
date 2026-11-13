/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_timeline_h_
#define _gweni_controls_timeline_h_

#include <gweni/controls/base.h>
#include <gweni/controls/chartAxis.h>
#include <gweni/controls/scrollControl.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace gweni
{
namespace controls
{
class Timeline;

namespace internal
{
class Text;

//! The ScrollControl the lanes scroll in: virtual content as high as the
//! lanes, a vertical bar alone, no inset. It holds no content of its own (its
//! inner panel stays empty): the Timeline draws the bars and its caption
//! column places the captions from its scroll offset. Neither it nor its inner
//! panel takes the mouse, so the Timeline is hovered over its lanes and only
//! the bar is the body's.
class GWENI_EXPORT TimelineBody: public ScrollControl
{
public:
    GWENI_CONTROL(TimelineBody, ScrollControl);

public:
    //! A container, as a ScrollControl: its bar draws.
    GWENI_DRAWS_NOTHING

    //! The lanes' bar, shown while the lanes are higher than the body.
    BaseScrollBar *getVerticalScrollBar() const { return m_verticalScrollBar; }

protected:
    //! Tells the Timeline the offset this pass laid out.
    void contentsPlaced(skin::Base *skin) override;

private:
    friend class controls::Timeline;

    Timeline *m_timeline=nullptr;
};

//! The fixed left column holding the lanes' captions, one Text per lane, as
//! high as the bar area and clipping them to it (and to its width). The
//! Timeline shows and places the captions of the lanes in view, at their
//! lanes less the scroll offset, and hides the rest, so a scroll moves the
//! shown captions alone; the column lays out the shown ones.
class GWENI_EXPORT TimelineCaptionColumn: public controls::Base
{
public:
    GWENI_CONTROL(TimelineCaptionColumn, controls::Base);

public:
    //! The Timeline paints the column's backdrop.
    GWENI_DRAWS_NOTHING

    void arrange(skin::Base *skin, Dim dim) override;
};

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
//! level in a depth layer of its own, so the later bar is in front.
//!
//! The x range is setXRange()'s, or automatic: from the earliest start to the
//! latest end of every bar (half a second each way round a single instant,
//! 0 to 1 with no bar). The time axis's ticks and labels are axis::'s
//! (chooseTimeTicks, in the zone of setUtcOffset()), as a Chart's time axis.
//!
//! Layout, in this control's coordinates: inside its padding, the axis's row
//! of labels at the top, LabelGap above the lanes; under it the body (a
//! TimelineBody) to the bottom, whose vertical bar shows at its right when
//! the lanes are higher than it; the caption column at the body's left,
//! getCaptionColumnWidth() wide; the bar area the rest of the body's
//! viewport, right of the column. Lane i's top is i*getLaneHeight() less the
//! scroll offset below the bar area's top; a bar is BarInset in from its
//! lane's top and bottom, from its start's x to its end's x (a unit wide at
//! least). The lanes scroll up and down under the fixed axis, the captions
//! with them in their fixed column.
//!
//! The tick labels are pooled internal::Text children set only when their
//! string changes, the captions one Text per lane in the caption column; the
//! bars are the renderer's slots, never controls. The Timeline lays out its
//! children in its Y arrange, once its size is final, the body first (whose
//! viewport the bar area is).
//!
//! A change of a bar marks StateChange_Background, and lays the timeline out
//! again only when it moves an automatic range.
class GWENI_EXPORT Timeline: public Base
{
public:

    GWENI_CONTROL(Timeline, Base);

public:

    ~Timeline() override;

    //! The axis labels' gap above the lanes, and the least gap between two
    //! of them.
    static constexpr int LabelGap=4;
    static constexpr int XLabelGap=12;
    //! A caption's padding left and right in its column.
    static constexpr int CaptionPadding=6;
    //! A bar's inset from its lane's top and bottom.
    static constexpr int BarInset=3;
    //! The overlap levels: 0 to this.
    static constexpr int MaxOverlapLevel=3;
    //! Lanes a wheel notch scrolls.
    static constexpr int WheelLanes=3;
    //! How far outside the bar area a bar's edge is held before rounding, so
    //! a bar too far out for an int is cut as a near one is, never wrapped;
    //! more than a bar's corner, which the area's clip then cuts off.
    static constexpr int OffAreaMargin=8;

    // ---- lanes ----------------------------------------------------------

    //! Adds a lane, last, and returns its index.
    size_t addLane(const String &caption);
    //! Removes lane \p lane and its bars; the lanes after it move up one.
    void removeLane(size_t lane);
    void clearLanes();
    size_t getLaneCount() const { return m_lanes.size(); }
    void setLaneCaption(size_t lane, const String &caption);
    String getLaneCaption(size_t lane) const;
    //! The lane's caption Text, null past the lanes.
    internal::Text *getCaptionLabel(size_t lane) const;

    //! Each lane's height (20 by default; at least 2*BarInset+1).
    void setLaneHeight(int height);
    int getLaneHeight() const { return m_laneHeight; }

    //! The caption column's width; 0 (the default) is automatic: the widest
    //! caption with CaptionPadding each side, a third of the inner width at
    //! most.
    void setCaptionColumnWidth(int width);
    int getCaptionColumnWidth() const { return m_captionWidth; }

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
    double getXMin() const;
    double getXMax() const;

    //! The time axis's zone (axis::localUtcOffset() by default); an empty one
    //! is UTC.
    void setUtcOffset(const axis::UtcOffset &offset);
    const axis::UtcOffset &getUtcOffset() const { return m_utcOffset; }

    // ---- scrolling ------------------------------------------------------

    //! How far the lanes are scrolled, in units, from 0 to the lanes' height
    //! less the bar area's: a new offset is clamped at once to the lanes and
    //! the area the last layout gave, and laid out by the next pass.
    int getScrollOffset() const;
    void setScrollOffset(int offset);
    //! Scrolls so lane \p lane is wholly in view, the least distance.
    void scrollToLane(size_t lane);
    internal::TimelineBody *getBody() const { return m_body; }
    internal::TimelineCaptionColumn *getCaptionColumn() const { return m_captions; }

    //! WheelLanes lanes a notch (60, GLFW's); a wheel the lanes cannot use
    //! goes on to the parent.
    bool onMouseWheeled(int delta) override;

    // ---- hit test, in this control's own coordinates ----------------------

    //! The lane at \p y: -1 outside the bar area's height or past the lanes.
    int laneAt(int y) const;
    //! The bar under \p local: in the lane at its y, a bar whose rect
    //! (getBarRect()) holds its x, the highest overlap level and then the
    //! latest of those; false outside the bar area or over none. A binary
    //! search on the lane's bars sorted by start, walking back over the
    //! earlier bars whose ends (a prefix maximum) reach the point: O(log n)
    //! in a packed lane, plus the bars overlapping the point.
    bool barAt(const Point &local, size_t &lane, size_t &index) const;

    // ---- hover and click ------------------------------------------------

    //! The tooltip's text for the bar under the mouse; without one the
    //! timeline's own (defaultHoverText()).
    typedef std::function<String(const Timeline &timeline, size_t lane, size_t index)> HoverText;
    //! Fills a control (a RichLabel, a Base of Labels) for the bar under the
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

    //! Where the bars are drawn: the body's viewport right of the caption
    //! column. Empty before a layout.
    Rect getBarArea() const { return m_area; }
    //! The caption column, from the bar area's top to its bottom.
    Rect getCaptionColumnBounds() const;
    //! The x of instant \p t, and the instant at \p x.
    double xPosition(double t) const;
    double timeAt(double x) const;
    //! The lane's rect across the bar area at its scrolled place: false when
    //! none of it is in the bar area.
    bool getLaneRect(size_t lane, Rect &rect) const;
    //! The bar's rect, whole units, its edges held OffAreaMargin outside the
    //! bar area but not cut to it (the renderer clips to the area): false
    //! for a bar out of range, of a lane out of view, wholly outside the
    //! area, or with a start or end that is not finite.
    bool getBarRect(size_t lane, size_t index, Rect &rect) const;

    //! The ticks whose labels show, their positions in this control's x, and
    //! the time step, as last laid out.
    const std::vector<axis::Tick> &getTicks() const { return m_ticks; }
    axis::TimeStep getTimeStep() const { return m_timeStep; }
    const std::vector<internal::Text *> &getTickLabels() const { return m_tickLabels; }

    //! A timeline is the size it was asked to be (its constructor's 480x240
    //! or the app's).
    bool sizedByRequest(Dim /*dim*/) const override { return true; }

    //! The tick labels in the skin's Colors.Timeline.AxisText, the captions in
    //! its Colors.Timeline.CaptionText.
    void updateColors() override;

protected:

    //! The X arrange leaves the children alone; the Y arrange lays out the
    //! body, then computes the bar area and the ticks and lays out the rest.
    void arrange(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;

    //! The bar area, the ticks and the tick labels, for the body's viewport.
    void placeAxis();
    //! The captions of the lanes in view shown and placed in the caption
    //! column at the scroll offset, the rest hidden.
    void placeCaptions(skin::Base *skin);

    struct Lane
    {
        String caption;
        internal::Text *label=nullptr;
        //! in the order added
        std::vector<TimelineBar> bars;

        //! derived from the bars (ensureLane)
        mutable bool dirty=true;
        //! the bars' indices sorted by start (equal starts in index order),
        //! those starts, and the running maximum of their ends
        mutable std::vector<size_t> order;
        mutable std::vector<double> starts;
        mutable std::vector<double> maxEnds;
        mutable std::vector<int> levels;
    };

    const Lane *laneAtIndex(size_t lane) const { return lane < m_lanes.size() ? &m_lanes[lane] : nullptr; }
    void ensureLane(const Lane &lane) const;
    //! A bar of \p lane changed: marks the timeline, and the lane's derived
    //! data; \p moved the range from \p before (no bar for an added one) to
    //! \p after (none for a removed one).
    void barsChanged(Lane &lane, const TimelineBar *before, const TimelineBar *after);
    void ensureRange() const;
    //! A change of the lanes or their captions that can move the layout.
    void lanesChanged();

    //! The rect of bar \p bar in lane \p laneIndex, unclipped (see
    //! getBarRect()), without the view checks.
    bool barRect(size_t laneIndex, const TimelineBar &bar, Rect &rect) const;

    friend class internal::TimelineBody;
    //! The body laid out at \p offset.
    void bodyScrolled(int offset);

    //! Something a bar's slot is drawn from changed: marks the timeline and
    //! bumps getBarsSerial().
    void barsMoved();
    //! The hover at the mouse's last place over the timeline (none off it),
    //! the tooltip brought up to it when it changed; with \p refill it is
    //! filled again even for the same bar: the bars under it changed.
    void setHover(bool hovering, size_t lane, size_t index, bool refill=false);
    void refreshHover(bool refill=false);

    //! A label's width and the labels' line height, measured once per string
    //! for the font, renderer and text layout they were measured with.
    int labelWidth(const String &text);
    int labelLineHeight();
    void renewMeasureKey();
    internal::Text *makeLabel(Base *parent, const Color &color);

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

    internal::TimelineBody *m_body=nullptr;
    internal::TimelineCaptionColumn *m_captions=nullptr;

    std::vector<Lane> m_lanes;
    size_t m_barCount=0;
    int m_laneHeight=20;
    int m_captionWidthSetting=0;

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
    int m_captionWidth=0;
    int m_axisTop=0;
    Rect m_area;
    Rect m_viewport;
    int m_scrollOffset=0;
    std::vector<axis::Tick> m_ticks;
    axis::TimeStep m_timeStep=axis::TimeStep::Hour1;
    std::vector<internal::Text *> m_tickLabels;

    //! the measures' key and cache
    const Font *m_measureFont=nullptr;
    Font m_measureFontValue;
    const renderer::Base *m_measureRenderer=nullptr;
    unsigned m_measureSerial=0;
    int m_lineHeight=-1;
    std::unordered_map<String, int> m_widths;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_timeline_h_
