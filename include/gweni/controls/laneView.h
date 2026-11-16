/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_laneView_h_
#define _gweni_controls_laneView_h_

#include <gweni/controls/base.h>
#include <gweni/controls/chartAxis.h>
#include <gweni/controls/scrollControl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace gweni
{
namespace controls
{
class LaneView;

namespace internal
{
class Text;

//! The ScrollControl a LaneView's lanes scroll in: virtual content as high as
//! the lanes, a vertical bar alone, no inset. It holds no content of its own
//! (its inner panel stays empty): the view draws what its lanes hold and its
//! caption column places the captions from its scroll offset. Neither it nor
//! its inner panel takes the mouse, so the view is hovered over its lanes and
//! only the bar is the body's.
class GWENI_EXPORT LaneBody: public ScrollControl
{
public:
    GWENI_CONTROL(LaneBody, ScrollControl);

public:
    //! A container, as a ScrollControl: its bar draws.
    GWENI_DRAWS_NOTHING

    //! The lanes' bar, shown while the lanes are higher than the body.
    BaseScrollBar *getVerticalScrollBar() const { return m_verticalScrollBar; }

protected:
    //! Tells the view the offset this pass laid out.
    void contentsPlaced(skin::Base *skin) override;

private:
    friend class controls::LaneView;

    LaneView *m_view=nullptr;
};

//! The fixed left column holding a LaneView's captions, a pool of Texts as
//! many as the most lanes it can show any of, as high as the lane area and
//! clipping them to it (and to its width). The view binds the captions to the
//! lanes in view and places them at their lanes less the scroll offset, so a
//! scroll moves the shown captions alone; the column lays out the shown ones.
class GWENI_EXPORT LaneCaptionColumn: public controls::Base
{
public:
    GWENI_CONTROL(LaneCaptionColumn, controls::Base);

public:
    //! The view paints the column's backdrop.
    GWENI_DRAWS_NOTHING

    void arrange(skin::Base *skin, Dim dim) override;
};

//! What a lane's time ranges are searched by: the ranges' indices sorted by
//! start (equal starts in index order), those starts, the running maximum of
//! their ends, and each range's overlap level (0 for a range no earlier range
//! of the lane overlaps, otherwise one more than the highest level of the
//! earlier ones it overlaps, maxLevel at most). Built from the ranges on the
//! first use after a change (dirty); a range whose start or end is not finite
//! is left out of the order and has level 0. Shared by the Timeline's bars and
//! the Sequencer's clips and keys.
struct LaneBarIndex
{
    bool dirty=true;
    std::vector<size_t> order;
    std::vector<double> starts;
    std::vector<double> maxEnds;
    std::vector<int> levels;

    //! Builds the index from \p count ranges, \p range(i, start, end) giving
    //! range i's start and end and returning false when it is not to be
    //! searched (not finite): O(n log n) for packed ranges, each range's
    //! level found against the earlier ranges whose ends reach its start
    //! (O(n^2) at worst, every range under one long one).
    template<typename Range>
    void build(size_t count, const Range &range, int maxLevel)
    {
        dirty=false;
        order.clear();

        std::vector<double> rangeStarts(count, 0.0);
        std::vector<double> rangeEnds(count, 0.0);
        std::vector<bool> searched(count, false);

        for(size_t i=0; i < count; ++i)
        {
            searched[i]=range(i, rangeStarts[i], rangeEnds[i]);
            if(searched[i])
                order.push_back(i);
        }
        std::stable_sort(order.begin(), order.end(),
            [&rangeStarts](size_t a, size_t b) { return rangeStarts[a] < rangeStarts[b]; });

        const size_t sorted=order.size();

        starts.resize(sorted);
        maxEnds.resize(sorted);
        for(size_t i=0; i < sorted; ++i)
        {
            starts[i]=rangeStarts[order[i]];
            maxEnds[i]=(i == 0) ? rangeEnds[order[i]] : std::max(maxEnds[i-1], rangeEnds[order[i]]);
        }

        levels.assign(count, 0);
        for(size_t i=0; i < count; ++i)
        {
            if(!searched[i])
                continue;

            const double start=rangeStarts[i];
            const double end=rangeEnds[i];
            size_t pos=static_cast<size_t>(std::lower_bound(starts.begin(), starts.end(), end)-starts.begin());
            int level=0;

            while(pos > 0 && maxEnds[pos-1] > start)
            {
                --pos;

                const size_t other=order[pos];

                if(other < i && rangeEnds[other] > start && rangeStarts[other] < end)
                    level=std::max(level, std::min(maxLevel, levels[other]+1));
            }
            levels[i]=level;
        }
    }

    //! The range drawn on top among those starting before \p high whose ends
    //! reach \p low and that \p accept(i) takes: the highest level, then the
    //! earliest (the one whose slot, drawn first, the depth test keeps in a
    //! shared layer). A binary search, then a walk back over the ranges
    //! starting before \p high while the running maximum of their ends still
    //! reaches \p low: O(log n) plus one or two ranges in a packed lane.
    template<typename Accept>
    bool pick(double low, double high, const Accept &accept, size_t &found) const
    {
        size_t pos=static_cast<size_t>(std::lower_bound(starts.begin(), starts.end(), high)-starts.begin());
        bool any=false;
        size_t best=0;

        while(pos > 0 && maxEnds[pos-1] >= low)
        {
            --pos;

            const size_t candidate=order[pos];

            if(!accept(candidate))
                continue;
            if(!any || levels[candidate] > levels[best] || (levels[candidate] == levels[best] && candidate < best))
            {
                best=candidate;
                any=true;
            }
        }
        if(any)
            found=best;
        return any;
    }
};

}//namespace internal

//! The view a Timeline and a Sequencer share: lanes, one row each, with their
//! captions in a fixed column at the left and an axis above the lanes, the
//! lanes scrolling up and down under it.
//!
//! Layout, in this control's coordinates: inside its padding, the axis's row
//! of labels at the top, LabelGap above the lanes; under it the body (a
//! LaneBody) to the bottom, whose vertical bar shows at its right when the
//! lanes are higher than it; the caption column at the body's left,
//! getCaptionColumnWidth() wide; the lane area (getBarArea()) the rest of the
//! body's viewport, right of the column. Lane i's top is i*getLaneHeight()
//! less the scroll offset below the area's top. The x range is the subclass's
//! (getXMin(), getXMax()), and so are the ticks (chooseTicks()), thinned here
//! so no label overlaps the next. A LaneView alone draws nothing (it has no
//! renderer of its own: its subclasses have).
//!
//! The tick labels are pooled internal::Text children set only when their
//! string changes, the captions a pool of Texts in the caption column bound
//! to the lanes in view (so ~100 lanes cost the ~20 on screen); what the
//! lanes hold is the renderer's slots, never controls. The view lays out its
//! children in its Y arrange, once its size is final, the body first (whose
//! viewport the lane area is).
class GWENI_EXPORT LaneView: public Base
{
public:

    GWENI_CONTROL(LaneView, Base);

public:

    ~LaneView() override;

    //! The axis labels' gap above the lanes, and the least gap between two
    //! of them.
    static constexpr int LabelGap=4;
    static constexpr int XLabelGap=12;
    //! A caption's padding left and right in its column.
    static constexpr int CaptionPadding=6;
    //! Lanes a wheel notch scrolls.
    static constexpr int WheelLanes=3;

    // ---- lanes ----------------------------------------------------------

    size_t getLaneCount() const { return m_laneCaptions.size(); }
    String getLaneCaption(size_t lane) const;
    //! The caption Text showing the lane, as last laid out: null for a lane
    //! out of view. The captions are a pool of Texts, as many as the most
    //! lanes the column can show any of, each bound to a lane in view.
    internal::Text *getCaptionLabel(size_t lane) const;

    //! Each lane's height (20 by default; at least minLaneHeight()).
    void setLaneHeight(int height);
    int getLaneHeight() const { return m_laneHeight; }

    //! The caption column's width; 0 (the default) is automatic: the widest
    //! caption with CaptionPadding each side, a third of the inner width at
    //! most.
    void setCaptionColumnWidth(int width);
    int getCaptionColumnWidth() const { return m_captionWidth; }

    // ---- the x axis -----------------------------------------------------

    //! The x range drawn: the subclass's (0 to 1 here).
    virtual double getXMin() const { return 0.0; }
    virtual double getXMax() const { return 1.0; }

    // ---- scrolling ------------------------------------------------------

    //! How far the lanes are scrolled, in units, from 0 to the lanes' height
    //! less the lane area's: a new offset is clamped at once to the lanes and
    //! the area the last layout gave, and laid out by the next pass.
    int getScrollOffset() const;
    void setScrollOffset(int offset);
    //! Scrolls so lane \p lane is wholly in view, the least distance.
    void scrollToLane(size_t lane);
    internal::LaneBody *getBody() const { return m_body; }
    internal::LaneCaptionColumn *getCaptionColumn() const { return m_captions; }

    //! WheelLanes lanes a notch (60, GLFW's); a wheel the lanes cannot use
    //! goes on to the parent.
    bool onMouseWheeled(int delta) override;

    // ---- geometry, in this control's own coordinates, as last laid out ----

    //! The lane at \p y: -1 outside the lane area's height or past the lanes.
    int laneAt(int y) const;
    //! Where the lanes' contents are drawn: the body's viewport right of the
    //! caption column. Empty before a layout.
    Rect getBarArea() const { return m_area; }
    //! The caption column, from the lane area's top to its bottom.
    Rect getCaptionColumnBounds() const;
    //! The axis's row, from the lane area's left to its right, above it: its
    //! labels' line from the inner top to the area's top.
    Rect getAxisRow() const;
    //! The x of instant \p t, and the instant at \p x.
    double xPosition(double t) const;
    double timeAt(double x) const;
    //! The lane's rect across the lane area at its scrolled place: false when
    //! none of it is in the area.
    bool getLaneRect(size_t lane, Rect &rect) const;

    //! The ticks whose labels show, their positions in this control's x, as
    //! last laid out.
    const std::vector<axis::Tick> &getTicks() const { return m_ticks; }
    const std::vector<internal::Text *> &getTickLabels() const { return m_tickLabels; }

    //! A lane view is the size it was asked to be (its constructor's or the
    //! app's).
    bool sizedByRequest(Dim /*dim*/) const override { return true; }

    //! The tick labels in the skin's Colors.Timeline.AxisText, the captions in
    //! its Colors.Timeline.CaptionText.
    void updateColors() override;

protected:

    //! The X arrange leaves the children alone; the Y arrange lays out the
    //! body, then computes the lane area and the ticks and lays out the rest.
    void arrange(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;

    // ---- for the subclass -------------------------------------------------

    //! The least lane height setLaneHeight() keeps.
    virtual int minLaneHeight() const { return 1; }
    //! The ticks between \p min and \p max over \p width units, positions
    //! from the area's left (axis::'s), before they are thinned; none here.
    virtual std::vector<axis::Tick> chooseTicks(double /*min*/, double /*max*/, int /*width*/) { return {}; }
    //! The lanes' contents moved on screen (a scroll, a new area or range):
    //! the subclass marks what it draws from them (here the background).
    virtual void laneContentMoved() { addStateChange(StateChange_Background); }
    //! The lanes changed: the default invalidates (the virtual height, the
    //! caption column and the captions in view) and calls laneContentMoved().
    virtual void lanesChanged();
    //! Lane \p lane's caption changed (after its Text was set).
    virtual void laneCaptionChanged(size_t /*lane*/) {}

    //! The lanes' captions: added last, removed (the lanes after it move up
    //! one), cleared, renamed. Each calls lanesChanged() (renaming
    //! laneCaptionChanged(), and invalidates an automatic column).
    void appendLane(const String &caption);
    void eraseLane(size_t lane);
    void eraseLanes();
    void renameLane(size_t lane, const String &caption);

    //! A label's width and the labels' line height, measured once per string
    //! for the font, renderer and text layout they were measured with.
    int labelWidth(const String &text);
    int labelLineHeight();

    //! The lane area, the ticks and the tick labels, for the body's viewport.
    void placeAxis();
    //! The ticks (getTicks()) for the lane area as last laid out and the x
    //! range now: chooseTicks(), thinned so no label overlaps the next.
    void computeTicks();
    //! Whether the tick labels are Text children placed by placeAxis() (the
    //! Timeline's), or only getTicks(), drawn by the renderer as text runs of
    //! its own (the Sequencer's, whose zoom and pan move them and must lay
    //! nothing out).
    virtual bool tickLabelsAreControls() const { return true; }
    //! The captions of the lanes in view shown and placed in the caption
    //! column at the scroll offset, from a pool of rows (see the .cpp).
    void placeCaptions(skin::Base *skin);
    //! Every caption row's lane forgotten: the lanes moved.
    void unbindCaptions();

    void renewMeasureKey();
    internal::Text *makeLabel(Base *parent, const Color &color);

    friend class internal::LaneBody;
    //! The body laid out at \p offset.
    void bodyScrolled(int offset);

    static constexpr size_t NoLane=static_cast<size_t>(-1);
    struct CaptionRow
    {
        internal::Text *label=nullptr;
        size_t lane=NoLane;
    };
    std::vector<CaptionRow> m_captionRows;

    internal::LaneBody *m_body=nullptr;
    internal::LaneCaptionColumn *m_captions=nullptr;

    std::vector<String> m_laneCaptions;
    int m_laneHeight=20;
    int m_captionWidthSetting=0;

    //! as last laid out
    int m_captionWidth=0;
    int m_axisTop=0;
    Rect m_area;
    Rect m_viewport;
    int m_scrollOffset=0;
    std::vector<axis::Tick> m_ticks;
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
#endif//_gweni_controls_laneView_h_
